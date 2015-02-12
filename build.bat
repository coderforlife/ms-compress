@echo off

:: This builds using MinGW-w64 for 32 and 64 bit (http://mingw-w64.sourceforge.net/)
:: Make sure both mingw-w32\bin and mingw-w64\bin are in the PATH

::-Werror
set FLAGS=-DMSCOMP_API_EXPORT -DMSCOMP_WITHOUT_XPRESS_HUFF -DMSCOMP_WITHOUT_LZX ^
  -static-libgcc -static-libstdc++ -O3 -fno-tree-vectorize -march=native -Wall -s
set FILES=mscomp.cpp ^
	lznt1_compress.cpp lznt1_decompress.cpp ^
	xpress_compress.cpp xpress_decompress.cpp ^
	xpress_huff_compress.cpp xpress_huff_decompress.cpp
set OUT=MSCompression

echo Compiling 32-bit...
i686-w64-mingw32-g++ %FLAGS% -DMSCOMP_API_DLL -shared %FILES% -o %OUT%.dll -Wl,--out-implib,lib%OUT%-dll.a
if ERRORLEVEL 1 goto END

i686-w64-mingw32-g++ %FLAGS% -DMSCOMP_API_LIB -c %FILES%
ar rcs lib%OUT%.a *.o
del /F /Q *.o >NUL 2>&1

echo.

echo Compiling 64-bit...
x86_64-w64-mingw32-g++ %FLAGS% -D MSCOMP_API_DLL -shared %FILES% -o %OUT%64.dll -Wl,--out-implib,lib%OUT%64-dll.a
if ERRORLEVEL 1 goto END

x86_64-w64-mingw32-g++ %FLAGS% -D MSCOMP_API_LIB -c %FILES%
ar rcs lib%OUT%64.a *.o
del /F /Q *.o >NUL 2>&1

:END
pause
