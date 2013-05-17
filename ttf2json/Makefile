CFLAGS=`pkg-config --libs freetype2 --cflags` -std=c++11 -g
LFLAGS=`pkg-config --libs freetype2 libpng`

all:
	g++ main.cpp -o gl3font ${CFLAGS} ${LFLAGS}
	./gl3font /usr/share/fonts/truetype/freefont/FreeSans.ttf 96 -o=freesans -data
	./gl3font /usr/share/fonts/truetype/freefont/FreeSans.ttf 48 -o=freesans2
	./gl3font /usr/share/fonts/truetype/freefont/FreeSans.ttf 24 -o=freesans4
	./gl3font /usr/share/fonts/truetype/freefont/FreeSans.ttf 12 -o=freesans8
	./gl3font /usr/share/fonts/truetype/freefont/FreeSans.ttf 6 -o=freesans16
