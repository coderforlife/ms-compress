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


// Xpress Compression and Decompression Functions
//
// This is the LZ version of the Xpress algorithm, used for Windows XP and newer hibernation file,
// Directory Replication Service (LDAP/RPC/AD), Windows Update Services, and Windows CE.
//
// The algorithm is documented in the MSDN article [MS-XCA]:
// http://msdn.microsoft.com/library/hh554002(v=prot.10).aspx
//
// The pseudo-code is available in that document, specifically at:
// Compression: http://msdn.microsoft.com/library/hh554053(v=PROT.10).aspx
// Decompression: http://msdn.microsoft.com/library/hh536411(v=PROT.10).aspx
//
// Compression is slower than RtlCompressBuffer (~0.6x as fast) but has a marginally better compression ratio and uses the same amount of memory
// Decompression is almost as fast as RtlDecompressBuffer (~0.9x as fast)

#ifndef XPRESS_H
#define XPRESS_H
#include "compression-api.h"

EXTERN_C_START

COMPAPI size_t xpress_compress(const_bytes in, size_t in_len, bytes out, size_t out_len);
#ifdef COMPRESSION_API_EXPORT
COMPAPI size_t xpress_max_compressed_size(size_t in_len);
#else
#define xpress_max_compressed_size(in_len) (((size_t)(in_len)) + 4 * ((((size_t)(in_len)) + 31) / 32))
#endif

COMPAPI size_t xpress_decompress(const_bytes in, size_t in_len, bytes out, size_t out_len);
COMPAPI size_t xpress_uncompressed_size(const_bytes in, size_t in_len);

EXTERN_C_END

#endif
