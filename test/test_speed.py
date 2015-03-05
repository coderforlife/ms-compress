#!/usr/bin/env python
"""
test_speed.py compressor decompressor format times directory

Runs compression speed tests. The compressor and decompressor must be one of rtl/rtl-max, rtl-std,
opensrc, dotnet/.net, copy, or copy-fast. The format must be one of none, lznt1, xpress, or
xpress-huffman and must be supported by both the compressor and decompressor. times is the number of
times to run each compression/decompression. The directory is the folder filled with files to use
for data.

This does not check accuracy of results at all, but instead times each compression and
decompression along with recording compression ratios. This does not support the streaming
compressors/decompressors.
"""
import sys
import os
import gc
from time import clock
import itertools

Rtl, OpenSrc, Net = None, None, None
from compressors import *

if len(sys.argv) != 6:
    print >> sys.stderr, 'Error: arguments'
    sys.exit()

compressors = {
    'rtl': Rtl, 'rtl-max': Rtl,
    'rtl-std': Rtl.Standard if Rtl is not None else None,
    'opensrc': OpenSrc,
    'dotnet': Net, '.net': Net,
    'copy': Copy, 'copy-fast': CopyFast,
    }
formats = {
    'none': 'NoCompression',
    'lznt1': 'LZNT1',
    'xpress': 'Xpress',
    'xpress-huffman': 'XpressHuffman',
    }

compressor = compressors.get(sys.argv[1].lower(), None)
decompressor = compressors.get(sys.argv[2].lower(), None)
format = formats.get(sys.argv[3].lower(), None)
path = sys.argv[5]
if (compressor is None or decompressor is None or format is None or
    not hasattr(compressor, format) or not hasattr(decompressor, format) or
    not sys.argv[4].isdigit() or not os.path.isdir(path)):
    print >> sys.stderr, 'Error: arguments'
    sys.exit()
compressor = getattr(compressor, format)
decompressor = getattr(decompressor, format)
times = int(sys.argv[4])

nfiles = 0
full_size = 0
compressed_size = 0
start_time = clock()
comp_time = 0
decomp_time = 0
gc.disable() # don't want this to interfere with times
for root, dirs, files in os.walk(path):
    ##mb_size = full_size / (1024.0 * 1024.0)
    ##if nfiles == 0:
    ##    data = (0, 100, 0, 0)
    ##else:
    ##    data = (nfiles, compressed_size * 100.0 / full_size, mb_size / comp_time, mb_size / decomp_time)
    ##print '%8.2f Folder: %s' % (clock() - start_time, root)
    ##print '         Files: %6d   CR: %.2f%%   Speed: %.2f MB/s   %.2f MB/s' % data
    for name in files:
        fullpath = os.path.join(root, name)
        try:
            if os.path.getsize(fullpath) == 0: continue
            with open(fullpath, 'rb') as f:
                input = f.read()
        except: continue
        try:
            size = len(input)
            compressed_buf = bytearray(max(int(size * 1.5), size + 1024))
            decompressed_buf = bytearray(size)
            
            a = clock()
            for i in itertools.repeat(None, times):
                compressed = compressor.Compress(input, compressed_buf)
            b = clock()
            comp_time += b - a
            
            a = clock()
            for i in itertools.repeat(None, times):
                decompressed = decompressor.Decompress(compressed, decompressed_buf)
            b = clock()
            decomp_time += b - a

            nfiles += times
            full_size += size * times
            compressed_size += len(compressed) * times
            
            ##if len(input) != len(decompressed):
            ##    print >> sys.stderr, 'Error: %s failed to compress/decompress (length %d != %d)' % (fullpath,len(input),len(decompressed))
            ##elif input != decompressed:
            ##    print >> sys.stderr, 'Error: %s failed to compress/decompress (content mismatch)' % fullpath
        except Exception as ex:
            print >> sys.stderr, 'Error: %s failed to compress/decompress (%s)' % (fullpath,ex.args[0])
        gc.collect() # manual collection when not timing Compress or Decompress
#print >> sys.stderr, '%8.3f Done!' % (clock() - start_time)
mb_size = full_size / (1024.0 * 1024.0)
print >> sys.stderr, '          Files: %5d   CR:%7.3f%%' % (nfiles, 100 if full_size == 0 else (compressed_size * 100.0 / full_size))
print >> sys.stderr, '          Compressed to   %10d bytes in %7.3f secs - %7.3f MB/s' % (compressed_size, comp_time, mb_size / comp_time)
print >> sys.stderr, '          Decompressed to %10d bytes in %7.3f secs - %7.3f MB/s' % (full_size, decomp_time, mb_size / decomp_time)
