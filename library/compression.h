// Compression and Decompression Functions

// These mimic RtlCompressBuffer and RtlDecompressBuffer from NTDLL.DLL. They provide access to
// LZX (WIM), LZNT1, Xpress (LZ), and Xpress Huffman algorithms. They attempt to provide results
// similar to the RTL functions in terms of speed and compression rate. The RTL functions do not
// accept the LZX format. Also, the provided functions always use the 'maximum engine'. For
// differences in the specific algorithms see their header files.

// Here is how to convert between the RTL functions and these functions:

//err = RtlCompressBuffer(FORMAT|MAX, in, in_len, out, out_len, 4096, &size, ***);
//size = compress(FORMAT, in, in_len, out, out_len); if (size == 0) err = errno;

//err = RtlDecompressBuffer(FORMAT, out, out_len, in, in_len, &size); // (switched order of in and out!)
//size = decompress(FORMAT, in, in_len, out, out_len); if (size == 0) err = errno;

#ifndef COMPRESSION_H
#define COMPRESSION_H
#include "compression-api.h"

typedef enum _CompressionFormat {
	COMPRESSION_NONE		= 0, // COMPRESSION_FORMAT_NONE
	COMPRESSION_LZX			= 1, // !!! COMPRESSION_FORMAT_DEFAULT
	COMPRESSION_LZNT1		= 2, // COMPRESSION_FORMAT_LZNT1
	COMPRESSION_XPRESS		= 3, // COMPRESSION_FORMAT_XPRESS
	COMPRESSION_XPRESS_HUFF	= 4, // COMPRESSION_FORMAT_XPRESS_HUFF
} CompressionFormat;

EXTERN_C {

COMPAPI size_t compress(CompressionFormat format, const_bytes in, size_t in_len, bytes out, size_t out_len);
COMPAPI size_t decompress(CompressionFormat format, const_bytes in, size_t in_len, bytes out, size_t out_len);

}

#endif
