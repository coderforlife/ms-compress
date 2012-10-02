// ms-compress: implements Microsoft compression algorithms
// Copyright (C) 2012  Jeffrey Bush  jeff@coderforlife.com
//
// This library is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// Adapted from 7-zip. See http://www.7-zip.org/.

#ifndef LZX_DECOMPRESSION_CORE_H
#define LZX_DECOMPRESSION_CORE_H
#include "compression-api.h"

#include "Bitstream.h"
#include "HuffmanDecoder.h"
#include "LZXConstants.h"

#ifdef VERBOSE_DECOMPRESSION
#include <stdio.h>
#include <ctype.h>
#endif

/////////////////// Decompression Functions ////////////////////////////
#define LZX_DECOMPRESS_INIT() \
	if (in_len < 4) { if (in_len) { PRINT_ERROR("LZX Decompression Error: Invalid Data: Less than 4 input bytes\n"); errno = E_INVALID_DATA; } return 0;  } \
	InputBitstream bits(in, in_len); \
	byte mainLevels[kMainTableSize]; memset(mainLevels, 0, sizeof(mainLevels)); \
	byte lenLevels [kNumLenSymbols]; memset(lenLevels,  0, sizeof(lenLevels ));

#define LZX_DECOMPRESS_READ_BLOCK_TYPE() \
	uint32_t blockType = bits.ReadBits(kNumBlockTypeBits); \
	if (blockType == 0 || blockType > kBlockTypeUncompressed) { PRINT_ERROR("LZX Decompression Error: Invalid Data: Illegal block type\n"); errno = E_INVALID_DATA; return 0; }

#define LZX_DECOMPRESS_CHECK_SIZE() \
	if (size == 0xFFFFFFFF) { PRINT_ERROR("LZX Decompression Error: Invalid Data: Illegal block size\n"); errno = E_INVALID_DATA; return 0; } \
	if (out_len < size) { PRINT_ERROR("LZX Decompression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }

#define LZX_DECOMPRESS_CHECK_UNCOMPRESSED_SIZE(CONDITION) \
	if ((CONDITION)) { PRINT_ERROR("LZX Decompression Error: Invalid Data: Uncompressed block size doesn't have enough bytes\n"); errno = E_INVALID_DATA; return 0; }

inline static bool lzx_decompress_read_table(InputBitstream *bits, bytes levels, uint32_t numSymbols)
{
	HuffmanDecoder<kNumHuffmanBits, kLevelTableSize> levelDecoder;
	byte levelLevels[kLevelTableSize];
	for (uint32_t i = 0; i < kLevelTableSize; i++)
		levelLevels[i] = (byte)bits->ReadBits(kNumBitsForPreTreeLevel);
	if (!levelDecoder.SetCodeLengths(levelLevels)) { return false; }

	for (uint32_t i = 0, count; i < numSymbols; i += count)
	{
		byte symbol;
		uint32_t number = levelDecoder.DecodeSymbol(bits);
			 if (number == kLevelSymbolZeros   ) { count = kLevelSymbolZerosStartValue    + bits->ReadBits(kLevelSymbolZerosNumBits   ); symbol = 0; }
		else if (number == kLevelSymbolZerosBig) { count = kLevelSymbolZerosBigStartValue + bits->ReadBits(kLevelSymbolZerosBigNumBits); symbol = 0; }
		else if (number == kLevelSymbolSame || number <= kNumHuffmanBits)
		{
			if (number <= kNumHuffmanBits) { count = 1; }
			else
			{
				count = kLevelSymbolSameStartValue + bits->ReadBits(kLevelSymbolSameNumBits);
				number = levelDecoder.DecodeSymbol(bits);
				if (number > kNumHuffmanBits) { return false; }
			}
			symbol = (byte)((17 + levels[i] - number) % (kNumHuffmanBits + 1)); // (byte)((number==0) ? 0 : (17 - number))
		}
		else { return false; }
		memset(levels + i, symbol, count);
	}

	return true;
}
inline static bool lzx_decompress_chunk(InputBitstream *bits, bytes out, size_t out_len, uint32_t repDistances[kNumRepDistances], byte mainLevels[kMainTableSize], byte lenLevels[kNumLenSymbols], unsigned int numPosLenSlots, bool isAlignOffsetBlock)
{
	HuffmanDecoder<kNumHuffmanBits, kMainTableSize> mainDecoder;
	HuffmanDecoder<kNumHuffmanBits, kNumLenSymbols> lenDecoder;
	HuffmanDecoder<kNumHuffmanBits, kAlignTableSize> alignDecoder;

	if (isAlignOffsetBlock)
	{
		byte alignLevels[kAlignTableSize];
		for (uint32_t i = 0; i < kAlignTableSize; i++)
			alignLevels[i] = (byte)bits->ReadBits(kNumBitsForAlignLevel);
		if (!alignDecoder.SetCodeLengths(alignLevels)) { PRINT_ERROR("LZX Decompression Error: Invalid Data: Unable to parse align offset tree\n"); errno = E_INVALID_DATA; return false; }
	}
#ifdef PRINT_ERRORS
	if (!lzx_decompress_read_table(bits, mainLevels, 0x100) ||
		!lzx_decompress_read_table(bits, mainLevels + 0x100, numPosLenSlots) ||
		!memset(mainLevels + 256 + numPosLenSlots, 0, kMainTableSize - (0x100 + numPosLenSlots)) ||
		!mainDecoder.SetCodeLengths(mainLevels)) { PRINT_ERROR("LZX Decompression Error: Invalid Data: Unable to parse main tree\n"); errno = E_INVALID_DATA; return false; }
	if (!lzx_decompress_read_table(bits, lenLevels, kNumLenSymbols) || !lenDecoder.SetCodeLengths(lenLevels)) { PRINT_ERROR("LZX Decompression Error: Invalid Data: Unable to parse length tree\n"); errno = E_INVALID_DATA; return false; }
#else
	if (!lzx_decompress_read_table(bits, mainLevels, 0x100) ||
		!lzx_decompress_read_table(bits, mainLevels + 0x100, numPosLenSlots)) { errno = E_INVALID_DATA; return false; }
	memset(mainLevels + 256 + numPosLenSlots, 0, kMainTableSize - (0x100 + numPosLenSlots));
	if (!mainDecoder.SetCodeLengths(mainLevels) || 
		!lzx_decompress_read_table(bits, lenLevels, kNumLenSymbols) ||
		!lenDecoder.SetCodeLengths(lenLevels)) { errno = E_INVALID_DATA; return false; }
#endif

	bytes end = out + out_len;
#ifdef VERBOSE_DECOMPRESSION
	bool last_literal = false;
#endif
	while (out < end)
	{
		uint32_t sym = mainDecoder.DecodeSymbol(bits);
		if (sym < 0x100)
		{
#ifdef VERBOSE_DECOMPRESSION
			if (!last_literal)
			{
				printf("\nLiterals: ");
				last_literal = true;
			}
			if (isprint(sym)) { printf("%c", (char)sym); }
			else if (sym == '\0') { printf("\\0"); }
			else if (sym == '\a') { printf("\\a"); }
			else if (sym == '\b') { printf("\\b"); }
			else if (sym == '\t') { printf("\\t"); }
			else if (sym == '\n') { printf("\\n"); }
			else if (sym == '\v') { printf("\\v"); }
			else if (sym == '\f') { printf("\\f"); }
			else if (sym == '\r') { printf("\\r"); }
			else if (sym == '\\') { printf("\\\\"); }
			else { printf("\\x%02X", sym); }
#endif
			*out++ = (byte)sym;
		}
		else
		{
			sym -= 0x100;
			if (sym >= numPosLenSlots) { PRINT_ERROR("LZX Decompression Error: Invalid Data: Unable to parse symbol\n"); errno = E_INVALID_DATA; return false; }
			uint32_t len = sym % kNumLenSlots, off = sym / kNumLenSlots;

			if (len == kNumLenSlots - 1)
			{
				uint_fast16_t l = lenDecoder.DecodeSymbol(bits);
				if (l >= kNumLenSymbols) { PRINT_ERROR("LZX Decompression Error: Invalid Data: Unable to parse length symbol\n"); errno = E_INVALID_DATA; return false; }
				len = l + kNumLenSlots - 1;
			}
			len += kMatchMinLen;

			if (off < kNumRepDistances)
			{
				uint32_t dist = repDistances[off];
				repDistances[off] = repDistances[0];
				repDistances[0] = dist;
			}
			else
			{
				byte n;
				if (off < kNumPowerPosSlots)
				{
					n = (byte)(off >> 1) - 1;
					off = (2 | (off & 1)) << n;
				}
				else
				{
					n = kNumLinearPosSlotBits;
					off = (off - 0x22) << kNumLinearPosSlotBits;
				}
				if (isAlignOffsetBlock && n >= kNumAlignBits)
				{
					off += bits->ReadBits(n - kNumAlignBits) << kNumAlignBits;
					uint32_t a = alignDecoder.DecodeSymbol(bits);
					if (a >= kAlignTableSize) { PRINT_ERROR("LZX Decompression Error: Invalid Data: Unable to parse align offset symbol\n"); errno = E_INVALID_DATA; return false; }
					off += a;
				}
				else
				{
					off += bits->ReadBits(n);
				}
				repDistances[2] = repDistances[1];
				repDistances[1] = repDistances[0];
				repDistances[0] = off - kNumRepDistances;
			}
			// TODO: check 'off'
			off = repDistances[0] + 1;

			// TODO: faster copy method
			//if (this->repDistances[0])
			//	memmove(out, out - off, len);
			//else
			//	memset(out, out[-1], len);
			//out += len;
			
#ifdef VERBOSE_DECOMPRESSION
			printf("\nMatch: %d @ %d", len, off);
			last_literal = false;
#endif

			for (const_bytes end = out + len; out < end; ++out) { *out = *(out-off); }
		}
	}
#ifdef VERBOSE_DECOMPRESSION
	printf("\n");
#endif
	return true;
}
inline static void lzx_decompress_translate_block(const const_bytes start, bytes buf, const const_bytes end, const int32_t translation_size)
{
	while (buf < end && (buf = (bytes)memchr(buf, 0xE8, end - buf)) != NULL)
	{
		int32_t pos = (int32_t)(buf++ - start);
		int32_t absValue = GET_UINT32(buf);
		if (absValue >= -pos && absValue < translation_size)
		{
			uint32_t relValue = (absValue >= 0) ? absValue - pos : absValue + translation_size;
			SET_UINT32(buf, relValue);
		}
		buf += 4;
	}
}
inline static void lzx_decompress_translate(bytes buf, size_t len, const int32_t translation_size)
{
	const const_bytes start = buf;
	if (len > 0x40000000) { len = 0x40000000; }
	while (len > 0x8000)
	{
		lzx_decompress_translate_block(start, buf, buf + 0x8000 - kMinTranslationLength, translation_size);
		len -= 1 << 15;
		buf += 1 << 15;
	}
	if (len > kMinTranslationLength)
		lzx_decompress_translate_block(start, buf, buf + len - kMinTranslationLength, translation_size);
}


/////////////////// Decompression Dry-run Functions ////////////////////////////
inline static bool lzx_decompress_chunk_dry_run(InputBitstream *bits, size_t out_len, byte mainLevels[kMainTableSize], byte lenLevels[kNumLenSymbols], uint32_t numPosLenSlots, bool isAlignBlock)
{
	HuffmanDecoder<kNumHuffmanBits, kMainTableSize> mainDecoder;
	HuffmanDecoder<kNumHuffmanBits, kNumLenSymbols> lenDecoder;
	HuffmanDecoder<kNumHuffmanBits, kAlignTableSize> alignDecoder;

	if (isAlignBlock)
	{
		byte alignLevels[kMaxTableSize];
		for (uint32_t i = 0; i < kAlignTableSize; i++)
			alignLevels[i] = (byte)bits->ReadBits(kNumBitsForAlignLevel);
		if (!alignDecoder.SetCodeLengths(alignLevels)) { errno = E_INVALID_DATA; return false; }
	}
	if (!lzx_decompress_read_table(bits, mainLevels, 256) ||
		!lzx_decompress_read_table(bits, mainLevels + 256, numPosLenSlots)) { errno = E_INVALID_DATA; return false; }
	memset(mainLevels + 256 + numPosLenSlots, 0, kMainTableSize - (256 + numPosLenSlots));
	if (!mainDecoder.SetCodeLengths(mainLevels) ||
		!lzx_decompress_read_table(bits, lenLevels, kNumLenSymbols) ||
		!lenDecoder.SetCodeLengths(lenLevels)) { errno = E_INVALID_DATA; return false; }

	while (out_len)
	{
		uint32_t sym = mainDecoder.DecodeSymbol(bits);
		if (sym < 0x100)
		{
			--out_len;
		}
		else
		{
			sym -= 0x100;
			if (sym >= numPosLenSlots) { errno = E_INVALID_DATA; return false; }
			uint32_t len = kMatchMinLen + sym % kNumLenSlots, off = sym / kNumLenSlots;

			if (len == kMatchMinLen + kNumLenSlots - 1)
			{
				uint_fast16_t l = lenDecoder.DecodeSymbol(bits);
				if (l >= kNumLenSymbols) { errno = E_INVALID_DATA; return false; }
				len = l + kMatchMinLen + kNumLenSlots - 1;
			}

			if (off >= kNumRepDistances)
			{
				byte n = (off < kNumPowerPosSlots) ? (byte)(off >> 1) - 1 : kNumLinearPosSlotBits;
				if (isAlignBlock && n >= kNumAlignBits)
				{
					bits->ReadBits(n - kNumAlignBits);
					if (alignDecoder.DecodeSymbol(bits) >= kAlignTableSize) { errno = E_INVALID_DATA; return false; }
				}
				else
				{
					bits->ReadBits(n);
				}
			}
			out_len -= len;
		}
	}
	return true;
}

#endif
