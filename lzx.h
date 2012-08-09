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


// Implements the LZX decompression algorithm used in WIM files.
// This is very similar to LZX decompression for CAB files with some minor differences.
//
// The original Microsoft document about the format: http://msdn.microsoft.com/en-us/library/bb417343.aspx#lzxdatacompressionformat
// A PDF of the document which I found easier to read: http://hackipedia.org/File%20formats/Containers/CAB,%20Microsoft%20CABinet%20compressed%20archive%20(archive)/LZXFMT.rtf.pdf
//
// The differences from the CAB LZX are:
//   The window is fixed to a 32KB size (NUM_POSITION_SLOTS = 8)
//   There is no header in front of all blocks, the translation mode is always on and has a size of 12000000
//   The uncompressed size entry for verbatim or aligned offset blocks is not 24 bits, it is:
//     1 bit if that bit has a value of 1, indicating the block is 32 KB (32768 bytes) when uncompressed
//     17 bits if the first bit is 0, and is the size of the block when uncompressed
//   For aligned offset blocks, the aligned offset tree precedes the main pre-tree 
//   Each block is treated independent of every other block (there is no delta compression of the trees and translation offset resets in each block)

#ifndef LZX_H
#define LZX_H
#include "compression-api.h"

EXTERN_C_START

COMPAPI size_t lzx_compress(const_bytes in, size_t in_len, bytes out, size_t out_len);
#ifdef COMPRESSION_API_EXPORT
COMPAPI size_t lzx_max_compressed_size(size_t in_len);
#else
#define lzx_max_compressed_size(in_len) (((size_t)(in_len)) + 20)
#endif

COMPAPI size_t lzx_decompress(const_bytes in, size_t in_len, bytes out, size_t out_len);
COMPAPI size_t lzx_uncompressed_size(const_bytes in, size_t in_len); // instant, not based on size

EXTERN_C_END

#endif
