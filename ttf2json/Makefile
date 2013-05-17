CFLAGS=`pkg-config --libs freetype2 --cflags` -std=c++11 -g
LFLAGS=`pkg-config --libs freetype2 libpng`

all:
	g++ main.cpp -o gl3font ${CFLAGS} ${LFLAGS}
