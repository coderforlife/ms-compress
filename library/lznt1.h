// LZNT1 Compression and Decompression Functions
//
// This algorithm is used for NTFS file compression, Windows 2000 hibernation file, Active
// Directory, File Replication Service, Windows Vista SuperFetch Files, and Windows Vista/7 bootmgr
//
// Compression is much faster than RtlCompressBuffer (~50x faster)
// Decompression is slightly faster than RtlDecompressBuffer (~1.4x faster)
//
// Calculating uncompressed size takes about half the time of decompression
//
// Assumptions based on RtlCompressBuffer output on NT 3.51, NT 4 SP1, XP SP2, Win 7 SP1:
//   All flags besides the compressed flag are always 011 (binary)
//   Actual chunk size is 4096 bytes (regardless of requested chunk size)
//
// Differences between these and RtlDecompressBuffer and RtlCompressBuffer:
//   Higher memory usage for compression (variable, from 512 KB to several megabytes)
//   Decompression gets faster with better compression ratios
//   Compressed size has a much nicer worst-case upper limit

#ifndef LZNT1_H
#define LZNT1_H
#include "compression-api.h"

EXTERN_C {

COMPAPI size_t lznt1_compress(const_bytes in, size_t in_len, bytes out, size_t out_len);
#ifdef COMPRESSION_API_EXPORT
COMPAPI size_t lznt1_max_compressed_size(size_t in_len);
#else
#define lznt1_max_compressed_size(in_len) (((size_t)(in_len)) + 3 + 2 * (((size_t)(in_len)) / 4096))
#endif

COMPAPI size_t lznt1_decompress(const_bytes in, size_t in_len, bytes out, size_t out_len);
COMPAPI size_t lznt1_uncompressed_size(const_bytes in, size_t in_len);

}

#endif
