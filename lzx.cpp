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

#ifdef __cplusplus_cli
#pragma unmanaged
#endif

#if defined(_MSC_VER) && defined(NDEBUG)
#pragma optimize("t", on)
#endif

#define MIN(a, b) (((a) < (b)) ? (a) : (b)) // Get the minimum of 2

/////////////////// WIM Functions ////////////////////////////
#ifdef COMPRESSION_API_EXPORT
size_t lzx_wim_max_compressed_size(size_t in_len) { return in_len + (in_len == 0x8000 ? 2 : 4) + kNumRepDistances * sizeof(uint32_t); }
#endif

size_t lzx_wim_compress(const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	if (in_len > 0x8000) { PRINT_ERROR("LZX Compression Error: Illegal argument: WIM-style maximum input size is 0x8000 bytes\n"); errno = EINVAL; return 0; }
	
	LZX_COMPRESSION_INIT();
	uint32_t len = (uint32_t)in_len;
	uint32_t uncomp_len = len + (len == 0x8000 ? 14 : (16 + (len & 1)));
	byte buf[0x10800];

	// Write header
	if (!bits.WriteBits(kBlockTypeVerbatim, kNumBlockTypeBits)) { PRINT_ERROR("LZX Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
	if (len == 0x8000) { if (!bits.WriteBit(1)) { PRINT_ERROR("LZX Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; } }
	else if (!bits.WriteBit(0) || !bits.WriteBits(len, 16)) { PRINT_ERROR("LZX Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }

	if (len >= 6)
	{
		bytes in_buf = (bytes)_alloca(len);
		memcpy(in_buf, in, len);
		lzx_compress_translate_block(in_buf, in_buf, in_buf + len - 6, 12000000);
		in = in_buf;
	}
	LZXDictionaryWIM d(in);

	// Compress the chunk
	if (!lzx_compress_chunk(in, len, buf, &bits, 30 * kNumLenSlots, &d, repDistances, last_symbol_lens, last_length_lens, &symbol_lens, &length_lens) ||
		bits.RawPosition() >= uncomp_len)
	{
		if (uncomp_len > out_len) { PRINT_ERROR("LZX Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
		
		// Write uncompressed when data is better off uncompressed
		if (len == 0x8000)
		{
			*(uint16_t*)out = (1 << kNumBlockTypeBits) | kBlockTypeUncompressed;
			out += sizeof(uint16_t);
		}
		else
		{
			*(uint16_t*)out = (uint16_t)((len << (1 + kNumBlockTypeBits)) | kBlockTypeUncompressed);
			out += sizeof(uint16_t);
			*(uint16_t*)out = (uint16_t)(len >> (16 - (1 + kNumBlockTypeBits)));
			out += sizeof(uint16_t);
		}
		memset(out, 0, kNumRepDistances * sizeof(uint32_t));
		out += kNumRepDistances * sizeof(uint32_t);
		memcpy(out, in, len);
		if ((len & 1) != 0) { out[len] = 0; }
		return uncomp_len;
	}
	else
	{
		return bits.Finish();
	}
}
size_t lzx_wim_decompress(const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	LZX_DECOMPRESS_INIT();
	LZX_DECOMPRESS_READ_BLOCK_TYPE();
	uint32_t size = bits.ReadBit() ? 0x8000 : bits.ReadBits(16);
	LZX_DECOMPRESS_CHECK_SIZE();
	if (size == 0) { return 0; }
	if (blockType == kBlockTypeUncompressed)
	{
		uint32_t off = (size == 0x8000 ? 2 : 4) + kNumRepDistances * sizeof(uint32_t);
		LZX_DECOMPRESS_CHECK_UNCOMPRESSED_SIZE(in_len < off + size);
		memcpy(out, in + off, size); // TODO: multi-byte copy
	}
	else if (!lzx_decompress_chunk(&bits, out, size, repDistances, mainLevels, lenLevels, 30 * kNumLenSlots, blockType == kBlockTypeAligned)) { return 0; }
	if (size >= 6) { lzx_decompress_translate_block(out, out, out + size - 6, 12000000); }
	return size;
}
size_t lzx_wim_uncompressed_size(const_bytes in, size_t in_len)
{
	if (!in_len) { return 0; }
	byte type = *in & 0x7;
	return (type == kBlockTypeVerbatim || type == kBlockTypeAligned || type == kBlockTypeUncompressed) ?
		((*in & 0x8) ? 0x8000 :
		(in_len < 3 ? 0 : (((in[2] << 12) | (in[1] << 4) | (*in >> 4)) & 0x7FFF))) : 0;
}


/////////////////// CAB Functions ////////////////////////////
#ifdef COMPRESSION_API_EXPORT
size_t lzx_cab_max_compressed_size(size_t in_len, unsigned int numDictBits) { return in_len + 4 + ((in_len + (1u << numDictBits) - 1) >> (numDictBits - 4)); }
#endif

size_t lzx_cab_compress(const_bytes in, size_t in_len, bytes out, size_t out_len, unsigned int numDictBits)
{
	LZX_COMPRESSION_INIT();
	const_bytes in_end = in + in_len;
	const uint32_t numPosLenSlots = GetNumPosSlots(numDictBits) * kNumLenSlots, windowSize = 1u << numDictBits;
	if (numPosLenSlots == 0) { PRINT_ERROR("LZX Compression Error: Invalid Argument: Invalid number of dictionary bytes\n"); errno = EINVAL; }

	// Write translation header
	if (!bits.WriteBit(0)) { PRINT_ERROR("LZX Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }

	bytes out_buf = (bytes)malloc(((((MIN(windowSize, in_len) + 1) >> 1) + 31) >> 5) * 132);	// for every 32 bytes in "in" we need up to 36 bytes in the temp buffer [completely uncompressed] or
																								// for every 32 bytes in "in/2" we need 132 bytes [everything compressed with length 2]
	LZXDictionaryCAB d(in, windowSize);

	while (in < in_end)
	{
		uint32_t len = (uint32_t)MIN(in_end - in, windowSize);
		uint32_t uncomp_len = 16 + len + (len & 1);
		
		// Write header
		OutputBitstream bits2 = bits;
		if (!bits.WriteBits(kBlockTypeVerbatim, kNumBlockTypeBits)) { break; }
		if (!bits.WriteManyBits(len, kUncompressedBlockSizeNumBits)) { break; }

		// Compress the chunk
		if (!lzx_compress_chunk(in, len, out_buf, &bits, numPosLenSlots, &d, repDistances, last_symbol_lens, last_length_lens, &symbol_lens, &length_lens) ||
			(bits.RawPosition() - bits2.RawPosition()) >= uncomp_len)
		{
			// Write uncompressed when data is better off uncompressed
			bits = bits2;
			if (!bits.WriteBits(kBlockTypeUncompressed, kNumBlockTypeBits)) { break; }
			if (!bits.WriteManyBits(len, kUncompressedBlockSizeNumBits)) { break; }
			bytes b = bits.Get16BitAlignedByteStream(len + len & 1 + 12);
			if (b == NULL) { break; }
			for (uint32_t i = 0; i < kNumRepDistances; ++i) { SET_UINT32(b, repDistances[i]); b += sizeof(uint32_t); }
			memcpy(b, in, len);
			if (len & 1) { b[len] = 0; }
		}
		else
		{
			// Copy symbols to last used
			memcpy(last_symbol_lens, symbol_lens, 0x100 + numPosLenSlots);
			memcpy(last_length_lens, length_lens, kNumLenSymbols);
		}
		
		in += len;
	}

	free(out_buf);

	if (in < in_end) { PRINT_ERROR("LZX Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
	return bits.Finish();
}
size_t lzx_cab_compress2(const_bytes in, size_t in_len, bytes out, size_t out_len, unsigned int numDictBits, uint32_t translation_size)
{
	LZX_COMPRESSION_INIT();
	size_t in_pos = 0;
	const uint32_t numPosLenSlots = GetNumPosSlots(numDictBits) * kNumLenSlots, windowSize = 1u << numDictBits;
	if (numPosLenSlots == 0) { PRINT_ERROR("LZX Compression Error: Invalid Argument: Invalid number of dictionary bytes\n"); errno = EINVAL; }

	// Write translation header
	if (!bits.WriteBit(1) || !bits.WriteUInt32(translation_size)) { PRINT_ERROR("LZX Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; };

	bytes _in_buf = (bytes)malloc(MIN(2*windowSize, in_len));
	bytes out_buf = (bytes)malloc(((((MIN(windowSize, in_len) + 1) >> 1) + 31) >> 5) * 132);	// for every 32 bytes in "in" we need up to 36 bytes in the temp buffer [completely uncompressed] or
																								// for every 32 bytes in "in/2" we need 132 bytes [everything compressed with length 2]
	LZXDictionaryCAB d(_in_buf, windowSize);

	while (in_pos < in_len)
	{
		uint32_t len = (uint32_t)MIN(in_len - in_pos, windowSize);
		uint32_t uncomp_len = 16 + len + (len & 1);
		bytes in_buf = _in_buf + (in_pos & windowSize);
		
		// Write header
		OutputBitstream bits2 = bits;
		if (!bits.WriteBits(kBlockTypeVerbatim, kNumBlockTypeBits)) { break; }
		if (!bits.WriteManyBits(len, kUncompressedBlockSizeNumBits)) { break; }

		// Translate
		memcpy(in_buf, in, len);
		lzx_compress_translate(in_buf - in_pos, in_buf, len, translation_size);

		// Compress the chunk
		if (!lzx_compress_chunk(in_buf, len, out_buf, &bits, numPosLenSlots, &d, repDistances, last_symbol_lens, last_length_lens, &symbol_lens, &length_lens) ||
			(bits.RawPosition() - bits2.RawPosition()) >= uncomp_len)
		{
			// Write uncompressed when data is better off uncompressed
			bits = bits2;
			if (!bits.WriteBits(kBlockTypeUncompressed, kNumBlockTypeBits)) { break; }
			if (!bits.WriteManyBits(len, kUncompressedBlockSizeNumBits)) { break; }
			bytes b = bits.Get16BitAlignedByteStream(len + len & 1 + 12);
			if (b == NULL) { break; }
			for (uint32_t i = 0; i < kNumRepDistances; ++i) { SET_UINT32(b, repDistances[i]); b += sizeof(uint32_t); }
			memcpy(b, in, len);
			if (len & 1) { b[len] = 0; }
		}
		else
		{
			// Copy symbols to last used
			memcpy(last_symbol_lens, symbol_lens, 0x100 + numPosLenSlots);
			memcpy(last_length_lens, length_lens, kNumLenSymbols);
		}
		
		in_pos += len;
	}
	
	free(_in_buf);
	free(out_buf);

	if (in_pos < in_len) { PRINT_ERROR("LZX Compression Error: Insufficient buffer\n"); errno = E_INSUFFICIENT_BUFFER; return 0; }
	return bits.Finish();
}

size_t lzx_cab_decompress(const_bytes in, size_t in_len, bytes out, size_t out_len, unsigned int numDictBits)
{
	LZX_DECOMPRESS_INIT();
	const bytes out_orig = out;
	const uint32_t numPosLenSlots = GetNumPosSlots(numDictBits) * kNumLenSlots;
	if (numPosLenSlots == 0) { PRINT_ERROR("LZX Decompression Error: Invalid Argument: Invalid number of dictionary bytes\n"); errno = EINVAL; }

	bool translationMode = bits.ReadBit();
	uint32_t translationSize;
	if (!translationMode) { translationSize = 0; }
	else
	{
		if (bits.RemainingBytes() < sizeof(uint32_t)) { PRINT_ERROR("LZX Decompression Error: Invalid Data: Unable to read translation size\n"); errno = E_INVALID_DATA; return 0; }
		translationSize = bits.ReadUInt32();
	}

	while (bits.RemainingRawBytes())
	{
		LZX_DECOMPRESS_READ_BLOCK_TYPE();
		uint32_t size = bits.ReadManyBits(kUncompressedBlockSizeNumBits);
		LZX_DECOMPRESS_CHECK_SIZE();
		if (size == 0) { break; }
		if (blockType == kBlockTypeUncompressed)
		{
			const_bytes in = bits.Get16BitAlignedByteStream(kNumRepDistances * sizeof(uint32_t) + size);
			LZX_DECOMPRESS_CHECK_UNCOMPRESSED_SIZE(!in);
			for (uint32_t i = 0; i < kNumRepDistances; i++) { repDistances[i] = GET_UINT32(in) - 1; in += sizeof(uint32_t); }
			memcpy(out, in, size); // TODO: multi-byte copy
		}
		else if (!lzx_decompress_chunk(&bits, out, size, repDistances, mainLevels, lenLevels, numPosLenSlots, blockType == kBlockTypeAligned)) { return 0; }
		out += size;
		out_len -= size;
	}

	if (translationMode)
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
