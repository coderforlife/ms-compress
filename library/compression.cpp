#include "compression.h"

#include "lznt1.h"
#include "lzx.h"
#include "xpress.h"
#include "xpress_huff.h"

#ifdef __cplusplus_cli
#pragma unmanaged
#endif

#ifndef ARRAYSIZE
#define ARRAYSIZE(x) sizeof(x)/sizeof(x[0])
#endif

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
	lzx_compress,
	lznt1_compress,
	xpress_compress,
	xpress_huff_compress,
};

static compress_func decompressors[] =
{
	copy_data,
	lzx_decompress,
	lznt1_decompress,
	xpress_decompress,
	xpress_huff_decompress,
};

size_t compress(CompressionFormat format, const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	if (format >= ARRAYSIZE(compressors) || !compressors[format])
	{
		PRINT_ERROR("Compression Error: Illegal format (%d)\n", format);
		errno = E_ILLEGAL_FORMAT;
		return 0;
	}
	return compressors[format](in, in_len, out, out_len);
}

size_t decompress(CompressionFormat format, const_bytes in, size_t in_len, bytes out, size_t out_len)
{
	if (format >= ARRAYSIZE(decompressors) || !decompressors[format])
	{
		PRINT_ERROR("Decompression Error: Illegal format (%d)\n", format);
		errno = E_ILLEGAL_FORMAT;
		return 0;
	}
	return decompressors[format](in, in_len, out, out_len);
}
