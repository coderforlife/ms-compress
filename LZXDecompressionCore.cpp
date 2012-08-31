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

#include "LZXDecompressionCore.h"

#include "Bitstream.h"
#include "HuffmanDecoder.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b)) // Get the minimum of 2

typedef HuffmanDecoder<kNumHuffmanBits, kMainTableSize> MainDecoder;
typedef HuffmanDecoder<kNumHuffmanBits, kNumLenSymbols> LenDecoder;
typedef HuffmanDecoder<kNumHuffmanBits, kAlignTableSize> AlignDecoder;
typedef HuffmanDecoder<kNumHuffmanBits, kLevelTableSize> LevelDecoder;

////////////////////////////// Decompression Functions /////////////////////////////////////////////
static bool lzx_read_table(InputBitstream *bits, byte *lastLevels, byte *newLevels, uint32_t numSymbols)
{
	LevelDecoder levelDecoder;
	byte levelLevels[kLevelTableSize];
	for (uint32_t i = 0; i < kLevelTableSize; i++)
		levelLevels[i] = (byte)bits->ReadBits(kNumBitsForPreTreeLevel);
	if (!levelDecoder.SetCodeLengths(levelLevels)) { return false; }

	uint32_t num = 0;
	byte symbol = 0;
	for (uint32_t i = 0; i < numSymbols;)
	{
		if (num != 0) { lastLevels[i] = newLevels[i] = symbol; i++; num--; continue; }

		uint32_t number = levelDecoder.DecodeSymbol(bits);

			 if (number == kLevelSymbolZeros   ) { num = kLevelSymbolZerosStartValue    + bits->ReadBits(kLevelSymbolZerosNumBits   ); symbol = 0; }
		else if (number == kLevelSymbolZerosBig) { num = kLevelSymbolZerosBigStartValue + bits->ReadBits(kLevelSymbolZerosBigNumBits); symbol = 0; }
		else if (number == kLevelSymbolSame || number <= kNumHuffmanBits)
		{
			if (number <= kNumHuffmanBits) { num = 1; }
			else
			{
				num = kLevelSymbolSameStartValue + bits->ReadBits(kLevelSymbolSameNumBits);
				number = levelDecoder.DecodeSymbol(bits);
				if (number > kNumHuffmanBits) { return false; }
			}
			symbol = (byte)((17 + lastLevels[i] - number) % (kNumHuffmanBits + 1)); // (byte)((number==0) ? 0 : (17 - number))
		}
		else { return false; }
	}
	return true;
}

static bool lzx_copy_uncompressed(InputBitstream *bits, bytes out, size_t out_len, uint32_t repDistances[kNumRepDistances])
{
	const_bytes in = bits->Get16BitAlignedByteStream(kNumRepDistances * sizeof(uint32_t) + out_len);
	if (!in) { PRINT_ERROR("LZX Decompression Error: Invalid Data: Uncompressed block size doesn't have enough bytes\n"); errno = E_INVALID_DATA; return false; }

	for (uint32_t i = 1; i < kNumRepDistances; i++)
	{
		repDistances[i] = GET_UINT32(in) - 1;
		in += sizeof(uint32_t);
	}
	memcpy(out, in, out_len); // TODO: multi-byte copy

	return true;
}

static bool lzx_decompress_chunk(InputBitstream *bits, bytes out, size_t out_len,
	uint32_t repDistances[kNumRepDistances], byte lastMainLevels[kMainTableSize], byte lastLenLevels[kNumLenSymbols], uint32_t numPosLenSlots, bool isAlignBlock)
{
	MainDecoder mainDecoder;
	LenDecoder lenDecoder;
	AlignDecoder alignDecoder;

	byte newLevels[kMaxTableSize];
	if (isAlignBlock)
	{
		for (uint32_t i = 0; i < kAlignTableSize; i++)
			newLevels[i] = (byte)bits->ReadBits(kNumBitsForAlignLevel);
		if (!alignDecoder.SetCodeLengths(newLevels))
			return false;
	}
	if (!lzx_read_table(bits, lastMainLevels, newLevels, 256) ||
		!lzx_read_table(bits, lastMainLevels + 256, newLevels + 256, numPosLenSlots)) { return false; }
	memset(newLevels + 256 + numPosLenSlots, 0, kMainTableSize - (256 + numPosLenSlots));
	if (!mainDecoder.SetCodeLengths(newLevels) ||
		!lzx_read_table(bits, lastLenLevels, newLevels, kNumLenSymbols) ||
		!lenDecoder.SetCodeLengths(newLevels)) { return false; }

	bytes end = out + out_len;
	while (out < end)
	{
		uint32_t sym = mainDecoder.DecodeSymbol(bits);
		if (sym < 0x100)
		{
			*out++ = (byte)sym;
		}
		else
		{
			sym -= 0x100;
			if (sym >= numPosLenSlots) { return false; }
			uint32_t len = kMatchMinLen + sym % kNumLenSlots, off = sym / kNumLenSlots;

			if (len == kMatchMinLen + kNumLenSlots - 1)
			{
				uint_fast16_t l = lenDecoder.DecodeSymbol(bits);
				if (l >= kNumLenSymbols) { return false; }
				len = l + kMatchMinLen + kNumLenSlots - 1;
			}

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
				if (isAlignBlock && n >= kNumAlignBits)
				{
					off += bits->ReadBits(n - kNumAlignBits) << kNumAlignBits;
					uint32_t a = alignDecoder.DecodeSymbol(bits);
					if (a >= kAlignTableSize) { return false; }
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
				
			for (const_bytes end = out + len; out < end; ++out) { *out = *(out-off); }
		}
	}
	return true;
}

static void lzx_decompress_translate(bytes out, size_t out_len, int32_t translation_size)
{
	if (out_len >= 6)
	{
		const_bytes start = out, end = out + MIN(out_len, 0x3FFFFFFF) - 6;
		while ((out = (bytes)memchr(out, 0xE8, end - out)) != NULL)
		{
			int32_t pos = (int32_t)(out++ - start);
			int32_t absValue = GET_UINT32(out);
			if (absValue >= -pos && absValue < translation_size)
			{
				uint32_t relValue = (absValue >= 0) ? absValue - pos : absValue + translation_size;
				SET_UINT32(out, relValue);
			}
			out += 4;
		}
	}
}

size_t lzx_decompress_core(const_bytes in, size_t in_len, bytes out, size_t out_len, bool wimMode, uint32_t numPosLenSlots)
{
	if (in_len < 2)
	{
		if (in_len) { PRINT_ERROR("LZX Decompression Error: Invalid Data: Less than 2 input bytes\n"); errno = E_INVALID_DATA; }
		return 0; 
	}

	const bytes out_orig = out;

	InputBitstream bits(in, in_len);

	uint32_t translation_size;
	if (wimMode) { translation_size = 12000000; }
	else if (!bits.ReadBit()) { translation_size = 0; }
	else
	{
		if (bits.RemainingBytes() < sizeof(uint32_t)) { PRINT_ERROR("LZX Decompression Error: Invalid Data: Unable to read translation size\n"); errno = E_INVALID_DATA; return 0; }
		translation_size = bits.ReadUInt32();
	}
	
	byte lastMainLevels[kMainTableSize];
	byte lastLenLevels[kNumLenSymbols];
	uint32_t repDistances[kNumRepDistances] = { 0, 0, 0 };
	memset(lastMainLevels, 0, sizeof(lastMainLevels));
	memset(lastLenLevels,  0, sizeof(lastLenLevels ));

	while (bits.RemainingRawBytes())
	{
		uint32_t blockType = bits.ReadBits(kNumBlockTypeBits);
		if (blockType == 0 || blockType > kBlockTypeUncompressed) { PRINT_ERROR("LZX Decompression Error: Invalid Data: Illegal block type\n"); errno = E_INVALID_DATA; return 0; }
		uint32_t size;
		if (wimMode) { size = bits.ReadBit() ? 0x8000 : bits.ReadBits(16); }
		else         { size = bits.ReadManyBits(kUncompressedBlockSizeNumBits); }
		if (out_len < size) { PRINT_ERROR("LZX Decompression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
		if (size == 0 || 
			blockType == kBlockTypeUncompressed && !lzx_copy_uncompressed(&bits, out, size, repDistances) ||
			!lzx_decompress_chunk(&bits, out, size, repDistances, lastMainLevels, lastLenLevels, numPosLenSlots, blockType == kBlockTypeAligned)) { errno = E_INVALID_DATA; return 0; }
		out += size;
		out_len -= size;
	}

	if (translation_size)
		lzx_decompress_translate(out_orig, out - out_orig, translation_size);
	return out - out_orig;
}


/////////////////// Decompression Dry-run Functions ////////////////////////////
static bool lzx_decompress_chunk_dry_run(InputBitstream *bits, size_t out_len, uint32_t repDistances[kNumRepDistances], byte lastMainLevels[kMainTableSize], byte lastLenLevels[kNumLenSymbols], uint32_t numPosLenSlots, bool isAlignBlock)
{
	MainDecoder mainDecoder;
	LenDecoder lenDecoder;
	AlignDecoder alignDecoder;

	byte newLevels[kMaxTableSize];
	if (isAlignBlock)
	{
		for (uint32_t i = 0; i < kAlignTableSize; i++)
			newLevels[i] = (byte)bits->ReadBits(kNumBitsForAlignLevel);
		if (!alignDecoder.SetCodeLengths(newLevels))
			return false;
	}
	if (!lzx_read_table(bits, lastMainLevels, newLevels, 256) ||
		!lzx_read_table(bits, lastMainLevels + 256, newLevels + 256, numPosLenSlots)) { return false; }
	memset(newLevels + 256 + numPosLenSlots, 0, kMainTableSize - (256 + numPosLenSlots));
	if (!mainDecoder.SetCodeLengths(newLevels) ||
		!lzx_read_table(bits, lastLenLevels, newLevels, kNumLenSymbols) ||
		!lenDecoder.SetCodeLengths(newLevels)) { return false; }

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
			if (sym >= numPosLenSlots) { return false; }
			uint32_t len = kMatchMinLen + sym % kNumLenSlots, off = sym / kNumLenSlots;

			if (len == kMatchMinLen + kNumLenSlots - 1)
			{
				uint_fast16_t l = lenDecoder.DecodeSymbol(bits);
				if (l >= kNumLenSymbols) { return false; }
				len = l + kMatchMinLen + kNumLenSlots - 1;
			}

			if (off < kNumRepDistances)
			{
				uint32_t dist = repDistances[off];
				repDistances[off] = repDistances[0];
				repDistances[0] = dist;
			}
			else
			{
				byte n = (off < kNumPowerPosSlots) ? (byte)(off >> 1) - 1 : kNumLinearPosSlotBits;
				if (isAlignBlock && n >= kNumAlignBits)
				{
					bits->ReadBits(n - kNumAlignBits);
					if (alignDecoder.DecodeSymbol(bits) >= kAlignTableSize) { return false; }
				}
				else
				{
					bits->ReadBits(n);
				}
				repDistances[2] = repDistances[1];
				repDistances[1] = repDistances[0];
				repDistances[0] = off - kNumRepDistances;
			}
			// TODO: check 'off'
			off = repDistances[0] + 1;
			out_len -= len;
		}
	}
	return true;
}
size_t lzx_decompress_dry_run_core(const_bytes in, size_t in_len, uint32_t numPosLenSlots)
{
	if (in_len < 2)
	{
		if (in_len) { errno = E_INVALID_DATA; }
		return 0; 
	}

	InputBitstream bits(in, in_len);
	size_t out_pos = 0;

	if (bits.ReadBit())
	{
		if (bits.RemainingBytes() < sizeof(uint32_t)) { errno = E_INVALID_DATA; return 0; }
		bits.ReadUInt32();
	}
	
	byte lastMainLevels[kMainTableSize];
	byte lastLenLevels[kNumLenSymbols];
	uint32_t repDistances[kNumRepDistances] = { 0, 0, 0 };
	memset(lastMainLevels, 0, sizeof(lastMainLevels));
	memset(lastLenLevels,  0, sizeof(lastLenLevels ));

	while (bits.RemainingRawBytes())
	{
		uint32_t blockType = bits.ReadBits(kNumBlockTypeBits);
		if (blockType == 0 || blockType > kBlockTypeUncompressed) { errno = E_INVALID_DATA; return 0; }
		uint32_t size = bits.ReadManyBits(kUncompressedBlockSizeNumBits);
		if (size == 0 || blockType != kBlockTypeUncompressed && !lzx_decompress_chunk_dry_run(&bits, size, repDistances, lastMainLevels, lastLenLevels, numPosLenSlots, blockType == kBlockTypeAligned)) { errno = E_INVALID_DATA; return 0; }
		out_pos += size;
	}

	return out_pos;
}
