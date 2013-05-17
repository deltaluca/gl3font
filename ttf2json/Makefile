CFLAGS=`pkg-config --libs freetype2 --cflags` -std=c++11 -O3
LFLAGS=`pkg-config --libs freetype2 libpng`

all:
	g++ main.cpp -o gl3font ${CFLAGS} ${LFLAGS}
