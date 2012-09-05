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

#ifndef LZX_COMPRESSION_CORE_H
#define LZX_COMPRESSION_CORE_H
#include "compression-api.h"

#include "Bitstream.h"
#include "HuffmanEncoder.h"
#include "LZXConstants.h"

#define LZX_COMPRESSION_INIT() \
	if (in_len == 0) { return 0; } \
	if (out_len < 4) { PRINT_ERROR("LZX Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; } \
	OutputBitstream bits(out, out_len); \
	uint32_t repDistances[kNumRepDistances] = { 1, 1, 1 }; \
	byte last_symbol_lens[kMainTableSize], last_length_lens[kNumLenSymbols]; \
	const_bytes symbol_lens, length_lens; \
	memset(last_symbol_lens, 0, kMainTableSize); \
	memset(last_length_lens, 0, kNumLenSymbols);


inline static void lzx_compress_translate_block(const const_bytes start, bytes buf, const const_bytes end, const int32_t translation_size)
{
	while ((buf = (bytes)memchr(buf, 0xE8, end - buf)) != NULL)
	{
		int32_t pos = (int32_t)(buf++ - start);
		int32_t relValue = GET_UINT32(buf);
		if (relValue >= -pos && relValue < translation_size)
		{
			uint32_t absValue = relValue > 0 ? relValue - translation_size : relValue + pos;
			SET_UINT32(buf, absValue);
		}
		buf += 4;
	}
}
inline static void lzx_compress_translate(const const_bytes start, bytes buf, size_t len, const int32_t translation_size)
{
	//const const_bytes start = buf;
	//if (len > 0x40000000) { len = 0x40000000; }
	if (buf - start + len > 0x40000000) { len = 0x40000000 + start - buf; }
	while (len > (1 << 15))
	{
		lzx_compress_translate_block(start, buf, buf + 0x8000 - 6, translation_size);
		len -= 1 << 15;
		buf += 1 << 15;
	}
	if (len >= 6)
		lzx_compress_translate_block(start, buf, buf + len - 6, translation_size);
}
static const byte Log2Table[256] = 
{
#define LT(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n
	/*-1*/0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
	LT(4), LT(5), LT(5), LT(6), LT(6), LT(6), LT(6),
	LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7)
#undef LT
};
//inline static byte highbit(uint32_t x) { uint_fast16_t y, z = x >> 16; return z ? 16 + Log2Table[z] : ((y = x >> 8) ? 8 + Log2Table[y] : Log2Table[x]); } // returns 0 - 23 (0x0 - 0x17)
inline static byte highbit(uint32_t x) { uint_fast16_t y = x >> 8, z; return y ? (((z = y >> 8) != 0) ? 16 + Log2Table[z] : 8 + Log2Table[y]) : Log2Table[x]; } // returns 0 - 23 (0x0 - 0x17)
template<typename LZXDictionary>
inline static size_t lzx_compress_lz77(const_bytes in, size_t in_len, bytes out, uint32_t repDistances[kNumRepDistances], uint32_t symbol_counts[kMainTableSize], uint32_t length_counts[kNumLenSymbols], LZXDictionary *d)
{
	uint32_t mask;
	const const_bytes out_orig = out;
	uint32_t* mask_out;
	byte i;

	memset(symbol_counts, 0, kMainTableSize*sizeof(uint32_t));
	memset(length_counts, 0, kNumLenSymbols*sizeof(uint32_t));

	////////// Count the symbols and write the initial LZ77 compressed data //////////
	// A uint32 mask holds the status of each subsequent byte (0 for literal, 1 for offset / length)
	// Literals are stored using a single byte for their value
	// Offset / length pairs are stored in the following manner:
	//   uint32 with   offset << 8 | (length - 2)
	// The number of bytes between uint32 masks is >=32 and <=128 (4*32)
	while (in_len > 0)
	{
		mask = 0;
		mask_out = (uint32_t*)out;
		out += sizeof(uint32_t);

		// Go through each bit
		for (i = 0; i < 32 && in_len > 0; ++i)
		{
			uint32_t len, off;
			mask >>= 1;
			d->Add(in);
			if (in_len >= 2 && (len = d->Find(in, &off)) >= 2)
			{
				if (len > in_len) { len = (uint32_t)in_len; }

				d->Add(in + 1, len - 1);

				in += len;
				in_len -= len;
				mask |= 0x80000000; // set the highest bit

				     if (repDistances[0] == off) { off = 0; } // X = R0, No Swap
				else if (repDistances[1] == off) { repDistances[1] = repDistances[0]; repDistances[0] = off; off = 1; } // X = R1, Swap R0 <=> R1
				else if (repDistances[2] == off) { repDistances[2] = repDistances[0]; repDistances[0] = off; off = 2; } // X = R2, Swap R0 <=> R2
				else { repDistances[2] = repDistances[1]; repDistances[1] = repDistances[0]; repDistances[0] = off; off += 2; } // R2 <- R1, R1 <- R0, R0 <- X

				// Write length and offset
				*(uint32_t*)out = (off << 8) | (len -= 2);
				out += 4;

				// Create a symbol from the offset and length
				if (len > 7) { ++length_counts[len - 7]; len = 7; }
				if (off >= 0x80000)
				{
					off = (off >> kNumLinearPosSlotBits) + 0x22;
				}
				else if (off >= kNumRepDistances) // TODO: better to remove this if
				{
					byte log2 = highbit(off);
					off = 2*log2 + ((off & (1 << (log2 - 1))) != 0); // TODO: maybe find faster equation // doesn't work for 0 and 1
				}
				++symbol_counts[((off << 3) | len) + 0x100];
			}
			else
			{
				// Write the literal value (which is the symbol)
				++symbol_counts[*out++ = *in++];
				--in_len;
			}
		}

		// Save mask
		*mask_out = mask;
	}
	
	// Set the total number of bytes read from in
	*mask_out = mask >> (32-i); // finish moving the value over

	// Return the number of bytes in the output
	return out - out_orig;
}
static bool lzx_write_table(OutputBitstream *bits, const_bytes lastLevels, const_bytes levels, uint32_t numSymbols)
{
	// Calculate run length encoded tree while getting pre-tree counts
	HuffmanEncoder<kNumHuffmanBits, kLevelTableSize> levelEncoder;
	uint32_t pre_tree_counts[kLevelTableSize];
	memset(pre_tree_counts, 0, sizeof(pre_tree_counts));
	byte tree[kMaxTableSize];
	uint32_t tree_pos = 0;
	for (uint32_t i = 0; i < numSymbols;)
	{
		if (levels[i] == 0)
		{
			uint32_t j;
			for (j = 1; i + j < numSymbols && levels[i+j] == 0; ++j);
			i += j;
			while (j >= kLevelSymbolZerosStartValue) // TODO: maybe divide up things better, for example: 54 breaks up to 51 and 3, but the 3 drops
			{
				byte z = (byte)(j < 51 ? j : 51), s = (z < kLevelSymbolZerosBigStartValue) ? kLevelSymbolZeros : kLevelSymbolZerosBig;
				++pre_tree_counts[s];
				tree[tree_pos++] = s;
				tree[tree_pos++] = z;
				j -= z;
			}
			pre_tree_counts[0] += j;
			memset(tree + tree_pos, 0, j);
			tree_pos += j;
		}
		else
		{
			byte s = (byte)((17 + lastLevels[i] - levels[i]) % (kNumHuffmanBits + 1));
			uint32_t j;
			for (j = 1; i + j < numSymbols && levels[i+j] == levels[i]; ++j);
			i += j;
			while (j >= kLevelSymbolSameStartValue) // TODO: maybe divide up things better, for example: 8 breaks up to 5 and 3, but the 3 drops
			{
				byte z = (byte)(j < 5 ? j : 5);
				++pre_tree_counts[kLevelSymbolSame];
				tree[tree_pos++] = kLevelSymbolSame;
				tree[tree_pos++] = z;
				tree[tree_pos++] = s;
				j -= z;
			}
			pre_tree_counts[s] += j;
			memset(tree + tree_pos, s, j);
			tree_pos += j;
		}
	}

	// Calculate pre-tree lengths and write them
	const_bytes pre_tree_lengths = levelEncoder.CreateCodes(pre_tree_counts);
	if (pre_tree_lengths == NULL) { return false; }
	for (uint32_t i = 0; i < kLevelTableSize; ++i)
		if (!bits->WriteBits(pre_tree_lengths[i], kNumBitsForPreTreeLevel)) { return false; }

	// Write the run length encoded tree using the pre-tree codes
	for (uint32_t i = 0; i < tree_pos; ++i)
	{
		if (!levelEncoder.EncodeSymbol(tree[i], bits)) { return false; }
		     if (tree[i] == kLevelSymbolZeros)    { if (!bits->WriteBits(tree[++i] - kLevelSymbolZerosStartValue,    kLevelSymbolZerosNumBits   )) { return false; } }
		else if (tree[i] == kLevelSymbolZerosBig) { if (!bits->WriteBits(tree[++i] - kLevelSymbolZerosBigStartValue, kLevelSymbolZerosBigNumBits)) { return false; } }
		else if (tree[i] == kLevelSymbolSame)
		{
			if (!bits->WriteBits(tree[++i] - kLevelSymbolSameStartValue, kLevelSymbolSameNumBits) || !levelEncoder.EncodeSymbol(tree[++i], bits)) { return false; }
		}
	}
	return true;
}
static const uint32_t OffsetMasks[18] = // (1 << n) - 1
{
	0x00000, 0x00001, 0x00003, 0x00007,
	0x0000F, 0x0001F, 0x0003F, 0x0007F,
	0x000FF, 0x001FF, 0x003FF, 0x007FF,
	0x00FFF, 0x01FFF, 0x03FFF, 0x07FFF,
	0x0FFFF, 0x1FFFF,
};
static bool lzx_compress_encode(const_bytes in, size_t in_len, OutputBitstream *bits, HuffmanEncoder<kNumHuffmanBits, kMainTableSize> *mainEncoder, HuffmanEncoder<kNumHuffmanBits, kNumLenSymbols> *lenEncoder)
{
	uint_fast16_t i;
	uint32_t mask;
	const_bytes end;

	// Write the encoded compressed data
	// This involves parsing the LZ77 compressed data and re-writing it with the Huffman code
	while (in_len > 0)
	{
		// Handle a fragment
		// Bit mask tells us how to handle the next 32 symbols, go through each bit
		for (i = 32, mask = *(uint32_t*)in, in += 4, in_len -= 4; mask && in_len > 0; --i, mask >>= 1)
		{
			if (mask & 1) // offset / length symbol
			{
				// Get the LZ77 offset and length
				uint32_t sym = *(uint32_t*)in;
				in += 4; in_len -= 4;

				byte len = sym & 0xFF;
				uint32_t off = sym >> 8;

				// Write the Huffman code then extra offset bits and length symbol
				byte O, n;
				if (off >= 0x80000)
				{
					O = (byte)((off >> kNumLinearPosSlotBits) + 0x22);
					n = kNumLinearPosSlotBits;
					off &= 0x1FFFF; // (1 << kNumLinearPosSlotBits) - 1
				}
				else if (off >= kNumRepDistances) 
				{
					byte log2 = highbit(off);
					O = 2*log2 + ((off & (1 << (log2 - 1))) != 0); // TODO: maybe find faster equation // doesn't work for 0 and 1
					n = (O >> 1) - 1; // doesn't work for 0 and 1
					off &= OffsetMasks[n]; // (1 << n) - 1
				}
				else
				{
					O = (byte)off;
					n = 0;
					off = 0;
				}
				// len is already -= 2
				sym = (uint_fast16_t)(((O << 3) | (len < 7 ? len : 7)) + 0x100);
				if (!mainEncoder->EncodeSymbol(sym, bits))					{ break; }
				if (len > 7 && !lenEncoder->EncodeSymbol(len - 7, bits))	{ break; }
				if (!bits->WriteBits(off, n))								{ break; }
			}
			else
			{
				// Write the literal symbol
				if (!mainEncoder->EncodeSymbol(*in, bits))					{ break; }
				++in; --in_len;
			}
		}
		if (in_len < i) { i = (byte)in_len; }

		// Write the remaining literal symbols
		for (end = in+i; in != end && mainEncoder->EncodeSymbol(*in, bits); ++in);
		if (in != end)														{ break; }
		in_len -= i;
	}

	// Write end of stream symbol and return insufficient buffer or the compressed size
	if (in_len > 0) { PRINT_ERROR("LZX Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return false; }
	return bits->Flush(); // make sure that the write stream is finished writing
}
template<typename LZXDictionary>
inline bool lzx_compress_chunk(const_bytes in, uint32_t in_len, bytes buf, OutputBitstream *bits, uint32_t numPosLenSlots, LZXDictionary *d,
	uint32_t repDistances[kNumRepDistances], const_bytes last_symbol_lens, const_bytes last_length_lens, const_bytes *symbol_lens, const_bytes *length_lens)
{
	HuffmanEncoder<kNumHuffmanBits, kMainTableSize> mainEncoder;
	HuffmanEncoder<kNumHuffmanBits, kNumLenSymbols> lenEncoder;
	//HuffmanEncoder<kNumHuffmanBits, kAlignTableSize> alignEncoder;
	uint32_t symbol_counts[kMainTableSize], length_counts[kNumLenSymbols];

	// LZ compress the data
	size_t buf_len = lzx_compress_lz77(in, in_len, buf, repDistances, symbol_counts, length_counts, d);
	
	// Write trees and encoded data
	return
		(*symbol_lens = mainEncoder.CreateCodes(symbol_counts)) != NULL &&
		(*length_lens = lenEncoder .CreateCodes(length_counts)) != NULL &&
		lzx_write_table(bits, last_symbol_lens, *symbol_lens, 0x100) &&
		lzx_write_table(bits, last_symbol_lens + 0x100, *symbol_lens + 0x100, numPosLenSlots) &&
		lzx_write_table(bits, last_length_lens, *length_lens, kNumLenSymbols) &&
		lzx_compress_encode(buf, buf_len, bits, &mainEncoder, &lenEncoder);
}

#endif
