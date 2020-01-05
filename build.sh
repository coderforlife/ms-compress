CXXFLAGS="${CXXFLAGS} -DMSCOMP_API_EXPORT -DMSCOMP_WITHOUT_LZX -O3 -march=native -mtune=generic -Wall -fno-exceptions -fno-rtti -fomit-frame-pointer"
FILES="src/*.cpp"
OUT="MSCompression"

if [ "${CXX}" = "" ]; then
	CXX=c++
fi

echo Compiling dynamic shared library...
${CXX} ${CXXFLAGS} -DMSCOMP_API_DLL -fPIC -shared ${FILES} -o lib${OUT}.so

echo Compiling static shared library...
${CXX} ${CXXFLAGS} -c ${FILES}
ar -rcs lib${OUT}.a *.o
rm -f *.o
