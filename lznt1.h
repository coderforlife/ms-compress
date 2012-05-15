#pragma once

// LZNT1 Compression and Decompression Functions
//
// This algorithm is used for NTFS file compression, Windows 2000 hibernation file, Active
// Directory, File Replication Service, Windows Vista SuperFetch Files, and Windows Vista/7 bootmgr
//
// Compression is much faster than RtlCompressBuffer (~50x faster)
// Decompression is slightly faster than RtlDecompressBuffer (~1.4x faster)
//
// Calculating Uncompressed size takes about half the time of decompression
//
// Assumptions based on RtlCompressBuffer output on NT 3.51, NT 4 SP1, XP SP2, Win 7 SP1:
//   All flags besides the compressed flag are always 011 (binary)
//   Actual chunk size is 4096 bytes (regardless of requested chunk size)
//
// Differences between these and RtlDecompressBuffer and RtlCompressBuffer:
//   Higher memory usage for compression (variable, from 512 KB to several megabytes)
//   Decompression gets faster with better compression ratios
//   Compressed size has a much nicer worst-case upper limit

size_t lznt1_compress(const_bytes in, size_t in_len, bytes out, size_t out_len);
#define lznt1_max_compressed_size(in_len) (((size_t)(in_len)) + 3 + 2 * (((size_t)(in_len)) / 4096))

size_t lznt1_decompress(const_bytes in, size_t in_len, bytes out, size_t out_len);
size_t lznt1_uncompressed_size(const_bytes in, size_t in_len);
