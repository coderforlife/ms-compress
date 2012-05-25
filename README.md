ms-compress
===========

Open source implementations of Microsoft compression algorithms. The progress is listed below. "RTL" refers to the native RtlCompressBuffer and RtlUncompressBuffer functions.

* LZX: This is currently just the 7zip code with some minor tweaks. Untested against native Windows functions.
* LZNT1:
 * Compression is much faster than RTL (~50x), but takes more memory (avg ~1 MB total), and has a much nicer worst-case upper limit on size
 * Decompression is slightly faster than RTL (~1.4x) and gets faster with better compressed data (RTL is reversed)
* XPRESS:
 * Compression is slower than RTL (~0.6x) but has marginally better compression ratio and uses the same amount of memory
 * Decompression is almost as fast as RTL (~0.9x)
* XPRESS_HUFF:
 * Has a similar compression ratio to WIMGAPI
 * Time not tested, and not compared to RTL since RTL-XPRESS-HUFF decompression is broken
