#pragma once

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

size_t xpress_compress(const_bytes in, size_t in_len, bytes out, size_t out_len);
#define xpress_max_compressed_size(in_len) (((size_t)(in_len)) + 4 * ((((size_t)(in_len)) + 31) / 32))

size_t xpress_decompress(const_bytes in, size_t in_len, bytes out, size_t out_len);
size_t xpress_uncompressed_size(const_bytes in, size_t in_len);
