CFLAGS=`pkg-config --libs freetype2 icu-uc icu-io --cflags` -std=c++11 -g
LFLAGS=`pkg-config --libs freetype2 icu-uc icu-io libpng`

all:
	g++ packing.cpp main.cpp distanceTransform.cpp -o ttfcompile ${CFLAGS} ${LFLAGS}
