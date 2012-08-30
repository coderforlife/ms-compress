ms-compress
===========

Open source implementations of Microsoft compression algorithms. The progress is listed below. "RTL" refers to the native RtlCompressBuffer and RtlUncompressBuffer functions.

* LZX: WIM-style, not CAB-style. Untested for accuracy. Untested against native Windows functions.
 * Compression only writes uncompressed blocks. Only support data of uncompressed size up to 32kb (WIM-style limit).
 * Decompression based on 7-zip code.
* LZNT1:
 * Compression:
  * Much faster than RTL (average ~50x, range 30x to 100x)
  * Slightly better compression ratio (only when last chunk is better off uncompressed, otherwise identical)
  * Uses more memory than RTL (average ~1 MB total)
 * Decompression:
  * Slightly faster than RTL (average ~1.7x, range 1.01x to 2.25x)
  * Gets faster with better compressed data (RTL is reversed)
* XPRESS:
 * Compression:
  * Slower than RTL (average ~0.4x, range 0.2x to 0.66x)
  * Has a better compression ratio (average ~0.7% better, range 0.0% to 3.1%)
  * Uses the same amount of memory
 * Decompression is almost as fast as RTL (~0.9x)
* XPRESS_HUFF:
 * Compression:
  * Much slower than RTL (average ~0.2x, range 0.07x to 0.99x)
  * Has a better compression ratio (average ~0.9% better, range -0.1% to 2.6%)
  * Uses about the same amount of memory
 * Decompression actually works (RTL Xpress Huffman decompression is broken)
