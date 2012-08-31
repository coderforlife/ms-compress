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


// Compression and Decompression Functions
//
// These mimic RtlCompressBuffer and RtlDecompressBuffer from NTDLL.DLL. They provide access to
// LZX (WIM), LZNT1, Xpress (LZ), and Xpress Huffman algorithms. They attempt to provide results
// similar to the RTL functions in terms of speed and compression rate. The RTL functions do not
// accept the LZX format. Also, the provided functions always use the 'maximum engine'. For
// differences in the specific algorithms see their header files.

// Here is how to convert between the RTL functions and these functions:
//
//err = RtlCompressBuffer(FORMAT|MAX, in, in_len, out, out_len, 4096, &size, ***); // chunk size and temporary buffer arguments dropped
//size = compress(FORMAT, in, in_len, out, out_len); if (size == 0) err = errno;
//
//err = RtlDecompressBuffer(FORMAT, out, out_len, in, in_len, &size); // switched order of in and out!
//size = decompress(FORMAT, in, in_len, out, out_len); if (size == 0) err = errno;

#ifndef COMPRESSION_H
#define COMPRESSION_H
#include "compression-api.h"

typedef enum _CompressionFormat {
	COMPRESSION_NONE		= 0, // COMPRESSION_FORMAT_NONE
	COMPRESSION_RESERVED	= 1, // !!! COMPRESSION_FORMAT_DEFAULT
	COMPRESSION_LZNT1		= 2, // COMPRESSION_FORMAT_LZNT1
	COMPRESSION_XPRESS		= 3, // COMPRESSION_FORMAT_XPRESS
	COMPRESSION_XPRESS_HUFF	= 4, // COMPRESSION_FORMAT_XPRESS_HUFF
} CompressionFormat;

EXTERN_C_START

COMPAPI size_t compress(CompressionFormat format, const_bytes in, size_t in_len, bytes out, size_t out_len);
COMPAPI size_t decompress(CompressionFormat format, const_bytes in, size_t in_len, bytes out, size_t out_len);

EXTERN_C_END

#endif
