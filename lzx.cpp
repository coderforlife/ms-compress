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

// Decompression code is adapted from 7-zip. See http://www.7-zip.org/.

#include "lzx.h"

#include "Bitstream.h"
#include "LZXCompressionCore.h"
#include "LZXDecompressionCore.h"
#include "LZXDictionaryWIM.h"
#include "LZXDictionaryCAB.h"

#include <malloc.h>

#define MIN(a, b) (((a) < (b)) ? (a) : (b)) // Get the minimum of 2

// The Huffman trees take up at least 41 bytes (the pre-trees are fixed to 30 bytes themselves) so anything under that is known to be better uncompressed (this is for size 0x8000 windows, more bytes for larger windows)
// Due to uint16_t alignments and the actual data needing to follow, lets assume 48 bytes is the minimum size that will actually reduce the number of bytes
// This is currently unused, but could possibly be used
#define MIN_COMPRESSIBLE_SIZE 48

/////////////////// WIM Functions ////////////////////////////
#ifdef COMPRESSION_API_EXPORT
size_t lzx_wim_max_compressed_size(size_t in_len) { return in_len + (in_len == 0x8000 ? 2 : 4) + kNumRepDistances * sizeof(uint32_t); }
#endif

#ifdef LARGE_STACK
#define ALLOC(n)	_alloca(n)
#define FREE(x)		
#else
#define ALLOC(n)	malloc(n)
#define FREE(x)		free(x)
#endif

uint32_t lzx_wim_compress(const_bytes in, uint32_t in_len, bytes out, size_t out_len)
{
	if (in_len > 0x8000 || in_len == 0) { PRINT_ERROR("LZX Compression Error: Illegal argument: WIM-style input size must be 1 to 0x8000 bytes (inclusive)\n"); errno = EINVAL; return 0; }
	if (out_len < 4) { PRINT_ERROR("LZX Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
	OutputBitstream bits(out, out_len);
	uint32_t repDistances[kNumRepDistances] = { 1, 1, 1 };
	byte last_symbol_lens[kMainTableSize], last_length_lens[kNumLenSymbols];
	const_bytes symbol_lens, length_lens;
	memset(last_symbol_lens, 0, kMainTableSize);
	memset(last_length_lens, 0, kNumLenSymbols);

	uint32_t uncomp_len = (in_len == 0x8000) ? (0x8000 + 14) : (in_len + 16 + (in_len & 1));
#ifdef LARGE_STACK
	byte buf[0x10800];
#else
	bytes buf = (bytes)malloc(0x10800);
#endif

	// Write header
	if (!bits.WriteBits(kBlockTypeVerbatim, kNumBlockTypeBits)) { PRINT_ERROR("LZX Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
	if (in_len == 0x8000) { if (!bits.WriteBit(1)) { PRINT_ERROR("LZX Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; } }
	else if (!bits.WriteBit(0) || !bits.WriteBits(in_len, 16)) { PRINT_ERROR("LZX Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }

	bytes in_buf;
	if (in_len > kMinTranslationLength)
	{
		in_buf = (bytes)ALLOC(in_len);
		memcpy(in_buf, in, in_len);
		lzx_compress_translate_block(in_buf, in_buf, in_buf + in_len - kMinTranslationLength, kWIMTranslationSize);
		in = in_buf;
	}
	else { in_buf = NULL; }
	LZXDictionaryWIM d(in, in_len);
	
	// Compress the chunk
	if (!lzx_compress_chunk(in, in_len, buf, &bits, 30 * kNumLenSlots, &d, repDistances, last_symbol_lens, last_length_lens, &symbol_lens, &length_lens) ||
		bits.RawPosition() >= uncomp_len)
	{
		FREE(buf);
		if (uncomp_len > out_len) { FREE(in_buf); PRINT_ERROR("LZX Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
		
		// Write uncompressed when data is better off uncompressed
		if (in_len == 0x8000)
		{
			*(uint16_t*)out = (kBlockTypeUncompressed << (16 - kNumBlockTypeBits)) | (1 << (16 - kNumBlockTypeBits - 1));
			out += sizeof(uint16_t);
		}
		else
		{
			*(uint16_t*)out = (kBlockTypeUncompressed << (16 - kNumBlockTypeBits)) | (uint16_t)(in_len >> (kNumBlockTypeBits + 1));
			out += sizeof(uint16_t);
			*(uint16_t*)out = (uint16_t)(in_len << (16 - (kNumBlockTypeBits + 1)));
			out += sizeof(uint16_t);
		}
		memset(out, 0, kNumRepDistances * sizeof(uint32_t));
		out += kNumRepDistances * sizeof(uint32_t);
		memcpy(out, in, in_len);
		if ((in_len & 1) != 0) { out[in_len] = 0; }
		FREE(in_buf);
		return uncomp_len;
	}
	else
	{
		FREE(buf);
		FREE(in_buf);
		return (uint32_t)bits.Finish();
	}
}
uint32_t lzx_wim_decompress(const_bytes in, uint32_t in_len, bytes out, size_t out_len)
{
	LZX_DECOMPRESS_INIT();
	uint32_t repDistances[kNumRepDistances] = { 0, 0, 0 };
	LZX_DECOMPRESS_READ_BLOCK_TYPE();
	uint32_t size = bits.ReadBit() ? 0x8000 : bits.ReadBits(16);
	LZX_DECOMPRESS_CHECK_SIZE();
	if (size == 0) { return 0; }
	if (blockType == kBlockTypeUncompressed)
	{
		uint32_t off = (size == 0x8000 ? 2 : 4) + kNumRepDistances * sizeof(uint32_t);
		LZX_DECOMPRESS_CHECK_UNCOMPRESSED_SIZE(in_len < off + size);
		memcpy(out, in + off, size);
	}
	else if (!lzx_decompress_chunk(&bits, out, size, repDistances, mainLevels, lenLevels, 30 * kNumLenSlots, blockType == kBlockTypeAligned)) { return 0; }
	if (size > kMinTranslationLength) { lzx_decompress_translate_block(out, out, out + size - kMinTranslationLength, kWIMTranslationSize); }
	return size;
}
uint32_t lzx_wim_uncompressed_size(const_bytes in, uint32_t in_len)
{
	if (!in_len) { return 0; }
	byte type = *in & 0x7;
	return (type == kBlockTypeVerbatim || type == kBlockTypeAligned || type == kBlockTypeUncompressed) ?
		((*in & 0x8) ? 0x8000 :
		(in_len < 3 ? 0 : (((in[2] << 12) | (in[1] << 4) | (*in >> 4)) & 0x7FFF))) : 0;
}


/////////////////// CAB Functions ////////////////////////////
WARNINGS_PUSH()
WARNINGS_IGNORE_ASSIGNMENT_OPERATOR_NOT_GENERATED()
struct _lzx_cab_state
{
	const uint32_t translation_size;

	bool first_block;
	size_t pos;

	const uint32_t num_pos_len_slots, window_size;

	uint32_t repDistances[kNumRepDistances];
	byte last_symbol_lens[kMainTableSize], last_length_lens[kNumLenSymbols];

	bytes in_buf;
	LZXDictionaryCAB d;

	byte out_buf[0x10800]; // for every 32 bytes in "in" we need up to 36 bytes in the temp buffer [completely uncompressed] or
						   // for every 32 bytes in "in/2" we need 132 bytes [everything compressed with length 2]

	_lzx_cab_state(unsigned int num_dict_bits, uint32_t translation_size = 0) :
		translation_size(translation_size), first_block(true), pos(0),
		num_pos_len_slots(GetNumPosSlots(num_dict_bits) * kNumLenSlots), window_size(1u << num_dict_bits), in_buf((bytes)malloc(2*window_size)), d(in_buf, window_size)
	{
		this->repDistances[0] = 1;
		this->repDistances[1] = 1;
		this->repDistances[2] = 1;
		memset(this->last_symbol_lens, 0, kMainTableSize);
		memset(this->last_length_lens, 0, kNumLenSymbols);
	}
	~_lzx_cab_state() { free(this->in_buf); }
};
WARNINGS_POP()

#ifdef COMPRESSION_API_EXPORT
#ifdef _WIN64
size_t lzx_cab_max_compressed_size(size_t in_len, unsigned int num_dict_bits) { return in_len + 4 + ((in_len + (1ull << num_dict_bits) - 1) >> (num_dict_bits - 4)); }
#else
size_t lzx_cab_max_compressed_size(size_t in_len, unsigned int num_dict_bits) { return in_len + 4 + ((in_len + (1u << num_dict_bits) - 1) >> (num_dict_bits - 4)); }
#endif
#endif

lzx_cab_state* lzx_cab_compress_start(unsigned int num_dict_bits, uint32_t translation_size)
{
	lzx_cab_state* state = new lzx_cab_state(num_dict_bits, translation_size);
	if (state->num_pos_len_slots == 0) { PRINT_ERROR("LZX Compression Error: Invalid Argument: Invalid number of dictionary bytes\n"); errno = EINVAL; delete state; return NULL; }
	return state;
}
uint32_t lzx_cab_compress_block(const_bytes in, uint32_t in_len, bytes out, uint32_t out_len, lzx_cab_state* state)
{
	if (in_len == 0 || in_len > 0x8000) { PRINT_ERROR("LZX Compression Error: Illegal argument: in_len\n"); errno = EINVAL; return 0; }

	size_t window_pos = state->pos & ((state->window_size << 1) - 1);
	bytes in_buf = state->in_buf + window_pos;

	OutputBitstream bits(out, out_len);

	if (state->first_block)
	{
		// Write translation header
		if (state->translation_size)
		{
			if (!bits.WriteBit(1) || !bits.WriteUInt32(state->translation_size)) { PRINT_ERROR("LZX Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; };
		}
		else if (!bits.WriteBit(0)) { PRINT_ERROR("LZX Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
	}

	// Fill buffer
	memcpy(in_buf, in, in_len);

	// Translate
	if (state->translation_size && in_len > kMinTranslationLength && state->pos < 0x40000000) { lzx_compress_translate_block(in_buf - state->pos, in_buf, in_buf + in_len - kMinTranslationLength, state->translation_size); }

	// Write header
	OutputBitstream bits2 = bits;
	if (!bits.WriteBits(kBlockTypeVerbatim, kNumBlockTypeBits) || !bits.WriteManyBits(in_len, kUncompressedBlockSizeNumBits)) { PRINT_ERROR("LZX Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }

	// Compress the chunk
	uint32_t comp_len, uncomp_len = in_len + (in_len & 1);
	const_bytes symbol_lens, length_lens;
	if (!lzx_compress_chunk(in_buf, in_len, state->out_buf, &bits, state->num_pos_len_slots, &state->d, state->repDistances, state->last_symbol_lens, state->last_length_lens, &symbol_lens, &length_lens) ||
		(bits.RawPosition() - bits2.RawPosition()) >= (4 + uncomp_len))
	{
		// Write uncompressed when data is better off uncompressed
		if (!bits2.WriteBits(kBlockTypeUncompressed, kNumBlockTypeBits) || !bits2.WriteManyBits(in_len, kUncompressedBlockSizeNumBits)) { PRINT_ERROR("LZX Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
		bytes b = bits2.Get16BitAlignedByteStream(uncomp_len);
		if (b == NULL) { PRINT_ERROR("LZX Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
		for (uint32_t i = 0; i < kNumRepDistances; ++i) { SET_UINT32(b, state->repDistances[i]); b += sizeof(uint32_t); }
		memcpy(b, in_buf, in_len);
		if (in_len & 1) { b[in_len] = 0; }
		state->d.Add(in_buf, in_len);
		comp_len = (uint32_t)(b - out + uncomp_len);
	}
	else
	{
		comp_len = (uint32_t)bits.Finish();

		// Copy symbols to last used
		memcpy(state->last_symbol_lens, symbol_lens, 0x100 + state->num_pos_len_slots);
		memcpy(state->last_length_lens, length_lens, kNumLenSymbols);
	}

	state->first_block = false;
	state->pos += in_len;

	return comp_len;
}
void lzx_cab_compress_end(lzx_cab_state* state) { delete state; }

size_t lzx_cab_decompress(const_bytes in, size_t in_len, bytes out, size_t out_len, unsigned int numDictBits)
{
	LZX_DECOMPRESS_INIT();
	uint32_t repDistances[kNumRepDistances] = { 0, 0, 0 };
	const bytes out_orig = out;
	const uint32_t numPosLenSlots = GetNumPosSlots(numDictBits) * kNumLenSlots;
	if (numPosLenSlots == 0) { PRINT_ERROR("LZX Decompression Error: Invalid Argument: Invalid number of dictionary bytes\n"); errno = EINVAL; }

	uint32_t translationSize;
	if (bits.ReadBit())
	{
		if (bits.RemainingBytes() < sizeof(uint32_t)) { PRINT_ERROR("LZX Decompression Error: Invalid Data: Unable to read translation size\n"); errno = E_INVALID_DATA; return 0; }
		translationSize = bits.ReadUInt32();
	}
	else
	{
		translationSize = 0;
	}

	while (bits.RemainingRawBytes())
	{
		LZX_DECOMPRESS_READ_BLOCK_TYPE();
		uint32_t size = bits.ReadManyBits(kUncompressedBlockSizeNumBits);
		LZX_DECOMPRESS_CHECK_SIZE();
		if (size == 0) { return 0; }
		if (blockType == kBlockTypeUncompressed)
		{
			const_bytes in = bits.Get16BitAlignedByteStream(kNumRepDistances * sizeof(uint32_t) + size);
			LZX_DECOMPRESS_CHECK_UNCOMPRESSED_SIZE(!in);
			for (uint32_t i = 0; i < kNumRepDistances; i++) { repDistances[i] = GET_UINT32(in) - 1; in += sizeof(uint32_t); }
			memcpy(out, in, size);
		}
		else if (!lzx_decompress_chunk(&bits, out, size, repDistances, mainLevels, lenLevels, numPosLenSlots, blockType == kBlockTypeAligned)) { return 0; }
		out += size;
		out_len -= size;
	}

	if (translationSize)
		lzx_decompress_translate(out_orig, out - out_orig, translationSize);
	return out - out_orig;
}
size_t lzx_cab_uncompressed_size(const_bytes in, size_t in_len, unsigned int numDictBits)
{
	LZX_DECOMPRESS_INIT();
	size_t out_pos = 0;
	const uint32_t numPosLenSlots = GetNumPosSlots(numDictBits) * kNumLenSlots;
	if (numPosLenSlots == 0) { errno = EINVAL; return 0; }

	if (bits.ReadBit()) // has translation size, which we don't need when just determining the size
	{
		if (bits.RemainingBytes() < sizeof(uint32_t)) { errno = E_INVALID_DATA; return 0; }
		bits.ReadUInt32();
	}
	
	while (bits.RemainingRawBytes())
	{
		LZX_DECOMPRESS_READ_BLOCK_TYPE();
		uint32_t size = bits.ReadManyBits(kUncompressedBlockSizeNumBits);
		if (size == 0xFFFFFFFF) { PRINT_ERROR("LZX Decompression Error: Invalid Data: Illegal block size\n"); errno = E_INVALID_DATA; return 0; }
		if (size == 0) { break; }
		if (blockType == kBlockTypeUncompressed)
		{
			LZX_DECOMPRESS_CHECK_UNCOMPRESSED_SIZE(!bits.Get16BitAlignedByteStream(kNumRepDistances * sizeof(uint32_t) + size));
		}
		else if (!lzx_decompress_chunk_dry_run(&bits, size, mainLevels, lenLevels, numPosLenSlots, blockType == kBlockTypeAligned)) { return 0; }
		out_pos += size;
	}

	return out_pos;
}
