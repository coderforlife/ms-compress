// Xpress Huffman Compression and Decompression Functions
//
// This is the Xpress compression used in WIM files not the hibernation file.
//
// A mostly complete pseudo-code decompression implementation is given at: http://msdn.microsoft.com/en-us/library/dd644740(PROT.13).aspx
// The decompression code is similar to the given pseudo-code but has been
// optimized, can handle 64kb chunked data, and detects the end-of-stream
// instead of requiring the decompressed size to be known.
//
// The compression code is completely new and performs similar to the WIMGAPI
// compression ratio (time not tested).

#ifndef XPRESS_HUFF_H
#define XPRESS_HUFF_H
#include "compression-api.h"

EXTERN_C {

COMPAPI size_t xpress_huff_compress(const_bytes in, size_t in_len, bytes out, size_t out_len);
COMPAPI size_t xpress_huff_decompress(const_bytes in, size_t in_len, bytes out, size_t out_len);

}

#endif
