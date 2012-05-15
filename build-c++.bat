@echo off

:: This builds using MinGW-w64 for 32 and 64 bit (http://mingw-w64.sourceforge.net/)
:: Make sure both mingw-w32\bin and mingw-w64\bin are in the PATH

::-Werror
set FLAGS=-mconsole -static-libgcc -static-libstdc++ -O3 -march=core2 -Wall -s
set FILES=compression.cpp Dictionary.cpp Bitstream.cpp lznt1.cpp lzx.cpp xpress.cpp xpress_huff.cpp test.cpp
set OUT=compression

echo Compiling 32-bit...
i686-w64-mingw32-g++ %FLAGS% %FILES% -o %OUT%.exe

echo.

echo Compiling 64-bit...

x86_64-w64-mingw32-g++ %FLAGS% %FILES% -o %OUT%64.exe

pause
