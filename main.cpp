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
    if (argc < 3) {
        printf("gl3font fontpath.ttf pxheight [-chars=file] [-o=outname] [-data]\n");
        printf("    default char set given by standard punctuation, latin letters and arabic digits\n");
        return 1;
    }

    // Load Freetype
    FT_Library library;
    if (FT_Init_FreeType(&library)) {
        printf("Error initialising freetype2\n");
        return 1;
    }

    // Load font face
    FT_Face face;
    int err = FT_New_Face(library, argv[1], 0, &face);
    if (err == FT_Err_Unknown_File_Format) {
        printf("Unsupported file format\n");
        return 1;
    }
    else if (err) {
        printf("Failed to read font file\n");
        return 1;
    }

    // Set font size.
    int pxheight = atoi(argv[2]);
    if (FT_Set_Pixel_Sizes(face, 0, pxheight)) {
        printf("Failed to set font size to height=%d(%s)\n", pxheight, argv[2]);
        return 1;
    }

    // Determine characters to be processed.
    string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789¬!\"£$%^&*()_+{}:@~<>?|`-=[];'#\\,./ ";
    for (int i = 3; i < argc; i++) {
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
        offsets.push_back(tuple<int,int,int>(face->glyph->advance.x>>6, face->glyph->bitmap_left, face->glyph->bitmap_top));
    }

    // Output files.
    string oname = argv[1];
    for (int i = 3; i < argc; i++) {
        string arg = argv[i];
        if (arg.substr(0,3).compare("-o=") == 0)
            oname = arg.substr(3);
    }

    // Produce bin-packed bitmap (1-px margins)
    // Don't use any special algorithm, just pack into a grid for simplicity.
    int cnt = ceil(sqrt(chars.length()));
    int size = cnt*(pxheight);
    size = (pow(2,ceil(log(size)/log(2))));
    png::image<png::gray_pixel> image(size, size);
    int x = 0; int y = 0;
    for (auto c: chars) {
        FT_Load_Glyph(face, FT_Get_Char_Index(face, c), FT_LOAD_RENDER);
        auto m = face->glyph->bitmap;
        for (int iy = 0; iy < m.rows; iy++) {
        for (int ix = 0; ix < m.width; ix++) {
            image[iy+y*(pxheight)][ix+x*(pxheight)] = m.buffer[iy*m.pitch+ix];
        }}
        x++;
        if (x >= cnt) {
            x = 0;
            y++;
        }
    }
    image.write(oname+".png");

    // Data file output.
    bool output = false;
    for (int i = 3; i < argc; i++) {
        string arg = argv[i];
        if (arg.compare("-data")==0) output = true;
    }
    if (output) {
        bool fst;
        ofstream data;
        data.open(oname+".json", ios::out);
        data << "{\n";
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
    }

    return 0;
}
