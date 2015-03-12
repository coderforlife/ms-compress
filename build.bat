@echo off

:: This builds using MinGW-w64 for 32 and 64 bit (http://mingw-w64.sourceforge.net/)
:: Make sure both mingw-w32\bin and mingw-w64\bin are in the PATH

set FLAGS=-DMSCOMP_API_EXPORT -DMSCOMP_WITHOUT_LZX -static-libgcc -static-libstdc++ -O3 -march=native -Wall -s
set FILES=src/*.cpp
set OUT=MSCompression

echo Compiling 32-bit...
i686-w64-mingw32-g++ %FLAGS% -DMSCOMP_API_DLL -shared %FILES% -o %OUT%.dll -Wl,--out-implib,lib%OUT%-dll.a
if ERRORLEVEL 1 goto END

i686-w64-mingw32-g++ %FLAGS% -c %FILES%
ar rcs lib%OUT%.a *.o
del /F /Q *.o >NUL 2>&1

echo.

echo Compiling 64-bit...
x86_64-w64-mingw32-g++ %FLAGS% -DMSCOMP_API_DLL -shared %FILES% -o %OUT%64.dll -Wl,--out-implib,lib%OUT%64-dll.a
if ERRORLEVEL 1 goto END

x86_64-w64-mingw32-g++ %FLAGS% -c %FILES%
ar rcs lib%OUT%64.a *.o
del /F /Q *.o >NUL 2>&1

:END
SetLocal EnableDelayedExpansion
set dblclkcmdline=%ComSpec% /c "%0 "
if !cmdcmdline! == !dblclkcmdline! ( pause )
