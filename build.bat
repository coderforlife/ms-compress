@echo off

:: This builds using MinGW-w64 for 32 and 64 bit (http://mingw-w64.sourceforge.net/)
:: Make sure both mingw-w32\bin and mingw-w64\bin are in the PATH

::-Werror
set FLAGS=-static-libgcc -static-libstdc++ -O3 -fno-tree-vectorize -march=core2 -Wall -s -D UNICODE -D _UNICODE -D COMPRESSION_API_EXPORT 
set FILES=compression.cpp lznt1.cpp lzx.cpp xpress.cpp xpress_huff.cpp Threaded.cpp
set OUT=MSCompression

echo Compiling 32-bit...
i686-w64-mingw32-g++ %FLAGS% -D COMPRESSION_API_DLL -shared %FILES% -o %OUT%.dll -Wl,--out-implib,lib%OUT%-dll.a

i686-w64-mingw32-g++ %FLAGS% -D COMPRESSION_API_LIB -c %FILES%
i686-w64-mingw32-ar rcs lib%OUT%.a *.o
del /F /Q *.o >NUL 2>&1

echo.

echo Compiling 64-bit...
x86_64-w64-mingw32-g++ %FLAGS% -D COMPRESSION_API_DLL -shared %FILES% -o %OUT%64.dll -Wl,--out-implib,lib%OUT%64-dll.a

x86_64-w64-mingw32-g++ %FLAGS% -D COMPRESSION_API_LIB -c %FILES%
x86_64-w64-mingw32-ar rcs lib%OUT%64.a *.o
del /F /Q *.o >NUL 2>&1

pause
