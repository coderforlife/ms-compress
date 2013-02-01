ms-compress
===========
Open source implementations of Microsoft compression algorithms. The progress is listed below. "RTL" refers to the
native RtlCompressBuffer and RtlUncompressBuffer functions from Windows 8's ntdll.dll. Comparisons are made against the
max compression engine for RTL functions. The compression speeds and ratios are when compressing a collection of 42
files totaling 67 MB composed from:
* [Calgary Corpus](http://corpus.canterbury.ac.nz/descriptions/#calgary)
* [Canterbury Corpus](http://corpus.canterbury.ac.nz/descriptions/#cantrbry)
* [Canterbury Large Corpus](http://corpus.canterbury.ac.nz/descriptions/#large)
* [Maximum Compression's single file tests](http://www.maximumcompression.com)

LZNT1
-----
Used for NTFS file compression, Windows 2000 hibernation file, Active Directory, File Replication Service, Windows Vista SuperFetch Files, and Windows Vista/7 bootmgr.

_Status: fully mature_ - no more significant changes likely

* Compression:
 * Much faster than RTL (average ~32x)
 * Slightly better compression ratio (only when last chunk is better off uncompressed, otherwise identical, ~2MB in 26GB)
 * Uses more memory than RTL (average ~1 MB total)
* Decompression:
 * Slightly faster than RTL (average ~1.2x)
 * Gets faster with better compressed data (RTL is reversed)

Xpress
------
Used for Windows XP and newer hibernation file, Directory Replication Service (LDAP/RPC/AD), Windows Update Services, and Windows CE.

MSDN article [MS-XCA]: http://msdn.microsoft.com/library/hh554002(v=prot.10).aspx  
The pseudo-code is available in that document: [Compression](http://msdn.microsoft.com/library/hh554053%28v=PROT.10%29.aspx)
and [Decompression](http://msdn.microsoft.com/library/hh536411%28v=PROT.10%29.aspx)

_Status: working_ - but needs major speed improvements

* Compression:
 * Slower than RTL (average ~0.26x)
 * Has a better compression ratio (average ~2% better)
 * Uses the same amount of memory
 * RTL does not work with uncompressed inputs of 7 bytes or less
 * RTL requires at least 24 extra bytes in the compression buffer
* Decompression is almost as fast as RTL (average ~0.92x)

Xpress Huffman
--------------
Xpress algorithm with Huffman encoding, used in WIM files, Distributed File System Replication, Windows 7 SuperFetch, and Windows 8 bootmgr.

MSDN article [MS-XCA]: http://msdn.microsoft.com/library/hh554002(v=prot.10).aspx  
The psuedo-code is found in that document, mostly referencing the LZ version of the Xpress algorithm.

_Status: working_ - but needs major speed improvements and does not create optional chunk boundary spanning matches

* Compression:
 * Much slower than RTL (average ~0.22x, range 0.07x to 1.04x)
 * Has a better compression ratio (average ~0.9% better, range -0.1% to 2.6%)
 * Uses about the same amount of memory
 * RTL requires at least 24 extra bytes in the compression buffer
 * BUG: GCC compiled code with -ftree-vectorize (included in -O3) causes "access violation reading 0x00000000"
* Decompression:
 * To be tested
 * RTL does not allow the output buffer to be anything besides the exact size of the uncompressed data

LZX
---
LZX compression used in WIM and CAB files with some minor differences between them.

Microsoft document about the CAB LZX format: http://msdn.microsoft.com/en-us/library/bb417343.aspx#lzxdatacompressionformat

Untested against native Windows functions (not part of RTL, need to test against CABINET.DLL (FCI/FDI) and WIMGAPI.DLL).

_Status: in development_ - works in some cases, but fails frequently

* Compression:
 * WIM: Mostly works
 * CAB: Potentially has issues with non-32k window size, does not seem to create length/offset pairs, memory issues abound
 * BOTH: Speed untested
 * BOTH: Does not support creation of aligned offset blocks but all other features implemented
* Decompression based on [7-zip](http://www.7-zip.org/) code
 * WIM: Mostly works
 * CAB: Should work for all valid compressed data <= 0x8000 in size (needs a refit like the compression functions to work on larger)
 * BOTH: Speed untested

Todo
====
* LZX Compression: Check and test speed
* LZX Compression: Add aligned offset blocks
* LZX Decompression: Check and test speed
* Xpress Huffman Compression: Allow matches to cross chunk boundaries
* Xpress Huffman Compression: Improve speed
* Xpress Compression: Improve speed
* All: Check edge cases (invalid compressed data to decompressor, output buffer not large enough)
