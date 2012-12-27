ms-compress
===========
Open source implementations of Microsoft compression algorithms. The progress is listed below. "RTL" refers to the native RtlCompressBuffer and RtlUncompressBuffer functions.

LZX
---
LZX compression used in WIM and CAB files with some minor differences between them.

Microsoft document about the CAB LZX format: http://msdn.microsoft.com/en-us/library/bb417343.aspx#lzxdatacompressionformat

WIM version tested for accuracy. Untested against native Windows functions.

* Compression:
 * WIM: Works
 * CAB: Potentially has issues with non-32k window size, does not seem to create length/offset pairs, memory issues abound
 * BOTH: Speed untested
 * BOTH: Does not support creation of aligned offset blocks but all other features implemented
* Decompression based on [7-zip](http://www.7-zip.org/) code
 * WIM: Works
 * CAB: Should work for all valid compressed data <= 0x8000 in size (needs a refit like the compression functions to work on larger)
 * BOTH: Speed untested

LZNT1
-----
Used for NTFS file compression, Windows 2000 hibernation file, Active Directory, File Replication Service, Windows Vista SuperFetch Files, and Windows Vista/7 bootmgr.

* Compression:
 * Much faster than RTL (average ~52x, range 30x to 99x)
 * Slightly better compression ratio (only when last chunk is better off uncompressed, otherwise identical)
 * Uses more memory than RTL (average ~1 MB total)
* Decompression:
 * Slightly faster than RTL (average ~1.76x, range 0.88x to 2.33x)
 * Gets faster with better compressed data (RTL is reversed)

Xpress
------
Used for Windows XP and newer hibernation file, Directory Replication Service (LDAP/RPC/AD), Windows Update Services, and Windows CE.

MSDN article [MS-XCA]: http://msdn.microsoft.com/library/hh554002(v=prot.10).aspx  
The pseudo-code is available in that document: [Compression](http://msdn.microsoft.com/library/hh554053%28v=PROT.10%29.aspx)
and [Decompression](http://msdn.microsoft.com/library/hh536411%28v=PROT.10%29.aspx)

* Compression:
 * Slower than RTL (average ~0.42x, range 0.25x to 0.76x)
 * Has a better compression ratio (average ~0.7% better, range 0.0% to 3.1%)
 * Uses the same amount of memory
* Decompression is almost as fast as RTL (~0.8x)

Xpress Huffman
--------------
Xpress algorithm with Huffman encoding, used in WIM files, Distributed File System Replication, Windows 7 SuperFetch, and Windows 8 bootmgr.

MSDN article [MS-XCA]: http://msdn.microsoft.com/library/hh554002(v=prot.10).aspx  
The psuedo-code is found in that document, mostly referencing the LZ version of the Xpress algorithm.

* Compression:
 * Much slower than RTL (average ~0.22x, range 0.07x to 1.04x)
 * Has a better compression ratio (average ~0.9% better, range -0.1% to 2.6%)
 * Uses about the same amount of memory
* Decompression actually works (RTL Xpress Huffman decompression is broken)
* BUG: GCC compiled code causes "access violation reading 0x00000000"

Todo
====
* LZX Compression: Check and test speed
* LZX Compression: Add aligned offset blocks
* LZX Decompression: Check and test speed
* Xpress Huffman Compression: Allow matches to cross chunk boundaries
* Xpress Huffman Compression: Improve speed
* Xpress Compression: Improve speed
* Check all for edge cases (invalid compressed data, output buffer not large enough)
