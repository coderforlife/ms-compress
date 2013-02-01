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


#include "compression.h"

#include "lznt1.h"
#include "lzx.h"
#include "xpress.h"
#include "xpress_huff.h"

// no-compression compression function
size_t copy_data(const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	if (in_len > out_len)
	{
		PRINT_ERROR("Copy Data Error: Insufficient buffer\n");
		errno = E_INSUFFICIENT_BUFFER;
		return 0;
	}
	memcpy(out, in, in_len);
	return in_len;
}

typedef size_t (*compress_func)(const_bytes in, size_t in_len, bytes out, size_t out_len);

static compress_func compressors[] =
{
	copy_data,
	NULL, //lzx_compress,
	lznt1_compress,
	xpress_compress,
	xpress_huff_compress,
};

static compress_func decompressors[] =
{
	copy_data,
	NULL, //lzx_decompress,
	lznt1_decompress,
	xpress_decompress,
	xpress_huff_decompress,
};

size_t compress(CompressionFormat format, const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	if ((unsigned)format >= ARRAYSIZE(compressors) || !compressors[format])
	{
		PRINT_ERROR("Compression Error: Illegal format (%d)\n", format);
		errno = E_ILLEGAL_FORMAT;
		return 0;
	}
	return compressors[format](in, in_len, out, out_len);
}

size_t decompress(CompressionFormat format, const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	if ((unsigned)format >= ARRAYSIZE(decompressors) || !decompressors[format])
	{
		PRINT_ERROR("Decompression Error: Illegal format (%d)\n", format);
		errno = E_ILLEGAL_FORMAT;
		return 0;
	}
	return decompressors[format](in, in_len, out, out_len);
}
