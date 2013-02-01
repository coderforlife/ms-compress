# -Werror
FLAGS="-O3 -fno-tree-vectorize -march=core2 -Wall -s -D UNICODE -D _UNICODE -D COMPRESSION_API_EXPORT -D PTHREADS"
FILES="compression.cpp lznt1.cpp lzx.cpp xpress.cpp xpress_huff.cpp Threaded.cpp"
OUT="MSCompression"

echo Compiling dynamic shared library...
g++ ${FLAGS} -D COMPRESSION_API_DLL -fPIC -shared ${FILES} -o lib${OUT}.so

echo Compiling static shared library...
g++ ${FLAGS} -D COMPRESSION_API_LIB -c ${FILES}
ar -rcs lib${OUT}.a *.o
rm -f *.o
