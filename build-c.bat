@echo off

:: This builds using MinGW-w64 for 32 and 64 bit (http://mingw-w64.sourceforge.net/)
:: Make sure both mingw-w32\bin and mingw-w64\bin are in the PATH

echo Compiling 32-bit...
i686-w64-mingw32-gcc -mconsole -static-libgcc -O3 -march=core2 -o compression.exe -s ^
	compression.c Dictionary.c Bitstream.c lznt1.c lzx.c xpress.c xpress_huff.c test.c ^

echo.

echo Compiling 64-bit...

x86_64-w64-mingw32-gcc -mconsole -static-libgcc -O3 -march=core2 -o compression64.exe -s ^
	compression.c Dictionary.c Bitstream.c lznt1.c lzx.c xpress.c xpress_huff.c test.c

pause
