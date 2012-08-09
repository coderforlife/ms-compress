ms-compress
===========

Open source implementations of Microsoft compression algorithms. The progress is listed below. "RTL" refers to the native RtlCompressBuffer and RtlUncompressBuffer functions.

* LZX: WIM-style, not CAB-style. Untested for accuracy. Untested against native Windows functions.
 * Compression only writes uncompressed blocks. Only support data of uncompressed size up to 32kb (WIM-style limit).
 * Decompression based on 7-zip code.
* LZNT1:
 * Compression is much faster than RTL (~50x), but takes more memory (avg ~1 MB total), and has a much nicer worst-case upper limit on size
 * Decompression is slightly faster than RTL (~1.4x) and gets faster with better compressed data (RTL is reversed)
* XPRESS:
 * Compression is slower than RTL (~0.6x) but has marginally better compression ratio and uses the same amount of memory
 * Decompression is almost as fast as RTL (~0.9x)
* XPRESS_HUFF:
 * Compression is slower than RTL (~0.1x) but has better compression ratio and uses about the same amount of memory
 * Decompression time not compared to RTL since decompression is broken
