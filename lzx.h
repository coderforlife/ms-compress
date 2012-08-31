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


// Implements the LZX decompression algorithm
//
// The original Microsoft document about the format: http://msdn.microsoft.com/en-us/library/bb417343.aspx#lzxdatacompressionformat
//
// The differences in the WIM LZX compared to the CAB LZX are:
//   The window is fixed to a 32KB size (NUM_POSITION_SLOTS = 8)
//   There is no header in front of all blocks, the translation mode is always on and has a size of 12000000
//   Each block is treated independent of every other block (there is no delta compression of the trees and translation offset resets in each block)
//   The uncompressed size entry for blocks is not 24 bits, it is:
//     1 bit if that bit has a value of 1, indicating the block is 32 KB (32768 bytes) when uncompressed
//     17 bits if the first bit is 0, and is the size of the block when uncompressed

#ifndef LZX_H
#define LZX_H
#include "compression-api.h"

EXTERN_C_START


// WIM style
// Note: the uncompressed size is at most 32kb
COMPAPI size_t lzx_wim_compress(const_bytes in, size_t in_len, bytes out, size_t out_len);
#ifdef COMPRESSION_API_EXPORT
COMPAPI size_t lzx_wim_max_compressed_size(size_t in_len);
#else
#define lzx_wim_max_compressed_size(in_len) (((size_t)(in_len)) + 20)
#endif

COMPAPI size_t lzx_wim_decompress(const_bytes in, size_t in_len, bytes out, size_t out_len);
COMPAPI size_t lzx_wim_uncompressed_size(const_bytes in, size_t in_len); // instant, not based on size


// CAB style
COMPAPI size_t lzx_cab_compress(const_bytes in, size_t in_len, bytes out, size_t out_len, unsigned int numDictBits);
#ifdef COMPRESSION_API_EXPORT
COMPAPI size_t lzx_cab_max_compressed_size(size_t in_len, unsigned int numDictBits);
#else
#ifdef _WIN64
#define lzx_cab_max_compressed_size(in_len, numDictBits) ((size_t)(in_len)) + 4 + 16 * ((((size_t)(in_len)) + (1ull << (numDictBits)) - 1) / (1ull << (numDictBits)))
#else
#define lzx_cab_max_compressed_size(in_len, numDictBits) ((size_t)(in_len)) + 4 + 16 * ((((size_t)(in_len)) + (1u << (numDictBits)) - 1) / (1u << (numDictBits)))
#endif
#endif

COMPAPI size_t lzx_cab_decompress(const_bytes in, size_t in_len, bytes out, size_t out_len, unsigned int numDictBits);
COMPAPI size_t lzx_cab_uncompressed_size(const_bytes in, size_t in_len, unsigned int numDictBits);


EXTERN_C_END

#endif
