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
//err = RtlCompressBuffer(FORMAT|MAX, in, in_len, out, out_len, 4096, &out_len, ***); // chunk size and temporary buffer arguments dropped
//err = ms_compress(FORMAT, in, in_len, out, &out_len); // out_len is both in and out
//
//err = RtlDecompressBuffer(FORMAT, out, out_len, in, in_len, &out_len); // switched order of in and out!
//err = ms_decompress(FORMAT, in, in_len, out, &out_len); // out_len is both in and out

// Additionally there are "stream" versions of the functions which allow you to generate the output
// chunk-by-chunk. This means that if you are (de)compressing a very large file it does not need to
// be all in memory at once.

#ifndef MSCOMPRESSION_H
#define MSCOMPRESSION_H
#include "mscomp/general.h"

EXTERN_C_START

MSCOMPAPI MSCompStatus ms_compress(MSCompFormat format, const_bytes in, size_t in_len, bytes out, size_t* out_len);
MSCOMPAPI MSCompStatus ms_decompress(MSCompFormat format, const_bytes in, size_t in_len, bytes out, size_t* out_len);
MSCOMPAPI size_t ms_max_compressed_size(MSCompFormat format, size_t in_len);

MSCOMPAPI MSCompStatus ms_deflate_init(MSCompFormat format, mscomp_stream* stream);
MSCOMPAPI MSCompStatus ms_deflate(mscomp_stream* stream, MSCompFlush flush);
MSCOMPAPI MSCompStatus ms_deflate_end(mscomp_stream* stream);

MSCOMPAPI MSCompStatus ms_inflate_init(MSCompFormat format, mscomp_stream* stream);
MSCOMPAPI MSCompStatus ms_inflate(mscomp_stream* stream);
MSCOMPAPI MSCompStatus ms_inflate_end(mscomp_stream* stream);

EXTERN_C_END

#endif
