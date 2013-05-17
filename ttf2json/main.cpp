#include <png++/png.hpp>
#include <cstdio>
#include <cstdlib>
#include <ft2build.h>
#include <map>
#include <string>
#include <vector>
#include <functional>
#include <fstream>
#include <iostream>
#include <cmath>
#include FT_FREETYPE_H
#include FT_GLYPH_H

using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 6) {
        printf("gl3font fontpath.ttf pxheight gap searchsize outsize [-chars=file] [-o=outname]\n");
        return 1;
    }

    string fontpath = argv[1];
    int pxheight = atoi(argv[2]);
    int gap = atoi(argv[3]);
    int searchsize = atoi(argv[4]);
    int outsize = atoi(argv[5]);

    // Load Freetype
    FT_Library library;
    if (FT_Init_FreeType(&library)) {
        printf("Error initialising freetype2\n");
        return 1;
    }

    // Load font face
    FT_Face face;
    int err = FT_New_Face(library, fontpath.c_str(), 0, &face);
    if (err == FT_Err_Unknown_File_Format) {
        printf("Unsupported file format\n");
        return 1;
    }
    else if (err) {
        printf("Failed to read font file\n");
        return 1;
    }

    // Set font size.
    if (FT_Set_Pixel_Sizes(face, 0, pxheight)) {
        printf("Failed to set font size to height=%d\n", pxheight);
        return 1;
    }

    // Determine characters to be processed.
    string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!\"$%^&*()_+{}:@~<>?|`-=[];'#\\,./ ";
    for (int i = 5; i < argc; i++) {
        string arg = argv[i];
        if (arg.substr(0,7).compare("-chars=") == 0)
            chars = arg.substr(7);
    }

    // Map characters to an id for processing and output.
    map<char,int> idmap;
    vector<char> charmap;
    int id = 0;
    for (auto c: chars) {
        idmap[c] = id++;
        charmap.push_back(c);
    }

    // Produce kerning table.
    vector<vector<int>> kerning;
    for (auto c: chars) {
        int cind = FT_Get_Char_Index(face, c);
        vector<int> kern;
        for (auto d: chars) {
            int dind = FT_Get_Char_Index(face, d);
            FT_Vector delta;
            FT_Get_Kerning(face, cind, dind, FT_KERNING_DEFAULT, &delta);
            kern.push_back(delta.x>>6);
        }
        kerning.push_back(kern);
    }

    // Produce character rendering offsets.
    vector<tuple<int,int,int>> offsets;
    map<char, FT_Glyph> glyphmap;
    for (auto c: chars) {
        FT_Load_Glyph(face, FT_Get_Char_Index(face, c), FT_LOAD_RENDER);
        offsets.push_back(tuple<int,int,int>(face->glyph->advance.x>>6, face->glyph->bitmap_left, -(pxheight-face->glyph->bitmap.rows) - face->glyph->bitmap_top));
    }

    // Output files.
    string oname = fontpath;
    for (int i = 5; i < argc; i++) {
        string arg = argv[i];
        if (arg.substr(0,3).compare("-o=") == 0)
            oname = arg.substr(3);
    }

    // Produce bin-packed bitmap (1-px margins)
    // Don't use any special algorithm, just pack into a grid for simplicity.
    int cnt = ceil(sqrt(chars.length()));
    int size = cnt*(pxheight+gap);
    png::image<png::rgb_pixel> image(size, size);

    double isize = 1.0/size;
    vector<tuple<int,int,double,double,double,double>> uvwh;
    int x = 0; int y = 0;
    for (auto c: chars) {
        FT_Load_Glyph(face, FT_Get_Char_Index(face, c), FT_LOAD_RENDER);
        auto m = face->glyph->bitmap;
        for (int iy = 0; iy < m.rows; iy++) {
        for (int ix = 0; ix < m.width; ix++) {
            int g = m.buffer[iy*m.pitch+ix];
            image[iy+y*(pxheight+gap)][ix+x*(pxheight+gap)] = png::rgb_pixel(g,g,g);
        }}

        uvwh.push_back(tuple<int,int,double,double,double,double>(
            m.width, m.rows,
            x*(pxheight+gap)*isize, y*(pxheight+gap)*isize, m.width*isize, m.rows*isize)
        );

        x++;
        if (x >= cnt) {
            x = 0;
            y++;
        }
    }

    png::image<png::rgb_pixel> dist(outsize, outsize);
    double scale = ((double)size)/outsize;
    auto lerp = [&](double x, double y) {
        if (x < 0 || y < 0 || x >= size || y >= size) {
            return 1e100;
        }

        int x0 = (int)x;
        int y0 = (int)y;
        int x1 = x0+1; if (x1 >= size) x1 = size-1;
        int y1 = y0+1; if (y1 >= size) y1 = size-1;

        double fx = x-x0;
        double fy = y-y0;

        int g00 = image[y0][x0].red;
        int g10 = image[y0][x1].red;
        int g11 = image[y1][x1].red;
        int g01 = image[y1][x0].red;

        return ((1-fx)*(1-fy)*g00 + (1-fx)*fy*g01 + fx*(1-fy)*g10 + fx*fy*g11)-0x80;
    };

    double* hood = new double[outsize*outsize];
    double min = 1e100;
    double max = -1e100;

    for (int iy = 0; iy < outsize; iy++) {
    for (int ix = 0; ix < outsize; ix++) {
        double m = lerp((ix+0.5)*scale, (iy+0.5)*scale);
        bool inside = m >= 0;

        double minDistance = 1e100;
        for (int y = -searchsize; y <= searchsize; y++) {
            double ay = ((iy+0.5)*scale) + y;
            for (int x = -searchsize; x <= searchsize; x++) {
                double ax = ((ix+0.5)*scale) + x;

                double n = lerp(ax, ay);
                if (n > 0x100) continue; // outside
                if (n*m <= 0) {
                    double dist = sqrt(x*x+y*y);
                    if (dist < minDistance) minDistance = dist;
                }
            }
        }
        if (minDistance != 1e100) {
            if (!inside) minDistance = -minDistance;
            if (minDistance < min) min = minDistance;
            if (minDistance > max) max = minDistance;
        }
        else {
            if (!inside) minDistance = -minDistance;
        }
        hood[iy*outsize+ix] = minDistance;
    }}

    double mm = fmax(fabs(min), fabs(max));
    min = -mm;
    max = mm;
    double det = 255.0/(max-min);
    for (int iy = 0; iy < outsize; iy++) {
    for (int ix = 0; ix < outsize; ix++) {
        double m = hood[iy*outsize+ix];
        if (m == 1e100) m = 255;
        else if (m == -1e100) m = 0;
        else
            m = (m - min)*det;
        int g = (int)m;
        dist[iy][ix] = png::rgb_pixel(g,g,g);
    }}
    dist.write(oname+".png");

    delete[] hood;

    // Data file output.
    bool fst;
    ofstream data;
    data.open(oname+".json", ios::out);
    data << "{\n";
    data << "    \"pxheight\":" << pxheight << ",\n";
    data << "    \"idmap\": {";
        fst = true;
        for (auto c: chars) {
            if (!fst) data << ",";
            fst = false;
            data << "\"";
            if (c == '"' || c == '\\') data << "\\";
            data << c << "\":" << idmap[c];
        }
        data << "},\n";
    data << "    \"uvwh\": [";
        fst = true;
        for (auto x: uvwh) {
            if (!fst) data << ",";
            fst = false;
            data << "{\"u\":"
                 << std::get<2>(x)
                 << ",\"v\":"
                 << std::get<3>(x)
                 << ",\"w\":"
                 << std::get<4>(x)
                 << ",\"h\":"
                 << std::get<5>(x)
                 << ",\"sizeu\":"
                 << std::get<0>(x)
                 << ",\"sizev\":"
                 << std::get<1>(x)
                 << "}";
        }
        data << "],\n";
    data << "    \"offsets\": [";
        fst = true;
        for (auto o: offsets) {
            if (!fst) data << ",";
            fst = false;
            data << "{\"advance\":"
                 << std::get<0>(o)
                 << ",\"left\":"
                 << std::get<1>(o)
                 << ",\"top\":"
                 << std::get<2>(o)
                 << "}";
        }
        data << "],\n";
    data << "    \"kerning\": [\n";
        fst = true;
        for (auto k: kerning) {
            if (!fst) data << ",\n";
            fst = false;
            data << "        [";
            bool fst2 = true;
            for (auto i: k) {
                if (!fst2) data << ",";
                fst2 = false;
                data << i;
            }
            data << "]";
        }
    data << "\n    ]\n";
    data << "}\n";
    data.close();

    return 0;
}
