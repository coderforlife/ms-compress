# -Werror
FLAGS="-DMSCOMP_API_EXPORT -DMSCOMP_WITHOUT_XPRESS_HUFF -DMSCOMP_WITHOUT_LZX -O3 -fno-tree-vectorize -march=core2 -Wall -s"
FILES="lznt1_compress.cpp lznt1_decompress.cpp xpress_compress.cpp xpress_decompress.cpp xpress_huff_compress.cpp xpress_huff_decompress.cpp"
OUT="MSCompression"

echo Compiling dynamic shared library...
g++ ${FLAGS} -DMSCOMP_API_DLL -fPIC -shared ${FILES} -o lib${OUT}.so

echo Compiling static shared library...
g++ ${FLAGS} -DMSCOMP_API_LIB -c ${FILES}
ar -rcs lib${OUT}.a *.o
rm -f *.o
