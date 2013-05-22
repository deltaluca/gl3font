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
#include <set>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include <unicode/utf.h>
#include <unicode/unistr.h>
#include <unicode/stringpiece.h>

using namespace std;

void pack_boxes(const vector<pair<int,int>>& sizes, int gap,
                map<int,pair<int,int>>* _packing,
                pair<int,int>* _size);

void distanceTransform(const png::image<png::gray_pixel>& image,
                       png::image<png::gray_pixel>& dist,
                       pair<int,int> insize,
                       pair<int,int> outsize,
                       int searchsize);

int main(int argc, char* argv[]) {
    if (argc < 5) {
        printf("ttf2json fontpath.ttf pxheight gap searchsize outsize [-chars=file] [-o=outname]\n");
        printf("ttf2json -transform image.png searchsize outsize [-o=outname]\n");
        return 1;
    }

    // Parse arguments
    string fontpath = argv[1];
    string oname;
    string charset = u8"!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿ";
    for (int i = 0; i < argc; i++) {
        string arg = argv[i];
        if (arg.substr(0,7).compare("-chars=") == 0)
            charset = arg.substr(7);
        if (arg.substr(0,3).compare("-o=") == 0)
            oname = arg.substr(3);
    }

    int searchsize, outsize;

    pair<int,int> dimensions;
    png::image<png::gray_pixel> image;

    if (fontpath.compare("-transform")==0) {
        searchsize   = atoi(argv[3]);
        outsize      = atoi(argv[4]);
        image = png::image<png::gray_pixel>(argv[2]);
        dimensions.first = image.get_width();
        dimensions.second = image.get_height();
    }
    else {
        oname = fontpath;
        int pxheight = atoi(argv[2]);
        int gap      = atoi(argv[3]);
        searchsize   = atoi(argv[4]);
        outsize      = atoi(argv[5]);

        float baseScale = 1.0/pxheight;

        // Set up freetype and fonts
        FT_Library library;
        FT_Init_FreeType(&library);
        FT_Face face;
        int err = FT_New_Face(library, fontpath.c_str(), 0, &face);
        if (err == FT_Err_Unknown_File_Format) {
            printf("Unsupported file format\n");
            return 1;
        }
        else if (err) {
            printf("Failed to load font\n");
            return 1;
        }
        if (FT_Set_Pixel_Sizes(face, 0, pxheight)) {
            printf("Failed to set font size\n");
            return 1;
        }

        // Retreive unicode code points, and define mapping.
        auto chars = icu::UnicodeString::fromUTF8(icu::StringPiece(charset));
        int charcnt = chars.countChar32();
        map<char32_t, int> idmap; int id = 0;
        for (int i = 0; i < charcnt; i++) { char32_t c = chars.char32At(i);
            idmap[c] = id++;
        }

        // Produce kerning table (compressed)
        float prev; int cnt = 0;
        vector<pair<float,int>> kerning;
        for (int i = 0; i < charcnt; i++) { char32_t c = chars.char32At(i);
        for (int j = 0; j < charcnt; j++) { char32_t d = chars.char32At(j);
            FT_Vector delta;
            FT_Get_Kerning(
                face,
                FT_Get_Char_Index(face, c),
                FT_Get_Char_Index(face, d),
                FT_KERNING_DEFAULT,
                &delta
            );
            if (delta.y != 0)
                printf("Warning: Kerning y<>0 and ignored\n");
            float curr = (delta.x>>6) * baseScale;
            if (curr == prev && cnt != 0) cnt++;
            else {
                if (cnt != 0) kerning.push_back(pair<float,int>(prev,cnt));
                prev = curr; cnt = 1;
            }
        }}
        if (cnt != 0) kerning.push_back(pair<float,int>(prev,cnt));

        // Produce character sizes for packing, and offsets
        vector<pair<int,int>> sizes;  // width/height (texels)
        vector<tuple<float,float,float>> offsets;  // advance/left/top
        for (int i = 0; i < charcnt; i++) { char32_t c = chars.char32At(i);
            FT_Load_Glyph(face, FT_Get_Char_Index(face, c), FT_LOAD_RENDER);
            if (face->glyph->advance.y != 0)
                printf("Warning: Advance y<>0 and ignored\n");
            offsets.push_back(tuple<float,float,float>(
                (face->glyph->advance.x>>6) * baseScale,
                (face->glyph->bitmap_left) * baseScale,
                (face->glyph->bitmap.rows-face->glyph->bitmap_top) * baseScale
            ));
            sizes.push_back(pair<int,int>(
                face->glyph->bitmap.width,
                face->glyph->bitmap.rows
            ));
        }

        // Perform bin packing.
        map<int, pair<int,int>> packing;
        pack_boxes(sizes, gap, &packing, &dimensions);

        // Pack glyphs, and produce uvwh details
        image = png::image<png::gray_pixel>(dimensions.first, dimensions.second);
        float uScale = 1.0/dimensions.first;
        float vScale = 1.0/dimensions.second;
        vector<tuple<float,float,float,float>> uvwh;
        for (int i = 0; i < charcnt; i++) { char32_t c = chars.char32At(i);
            pair<int,int> size = sizes[idmap[c]];
            pair<int,int> xy = packing[idmap[c]];

            FT_Load_Glyph(face, FT_Get_Char_Index(face, c), FT_LOAD_RENDER);
            auto m = face->glyph->bitmap;

            for (int iy = 0; iy < m.rows; iy++) {
            for (int ix = 0; ix < m.width; ix++) {
                image[xy.second + iy][xy.first + ix] = m.buffer[iy*m.pitch+ix];
            }}

            uvwh.push_back(tuple<float,float,float,float>(
                xy.first   * uScale, xy.second   * vScale,
                size.first * uScale, size.second * vScale
            ));
        }

        // Output descriptor data.
        ofstream data;
        auto _uint32 = [&](unsigned int x) { data.write((char*)&x, sizeof(unsigned int)); };
        auto _float  = [&](float x)        { data.write((char*)&x, sizeof(float)); };
        data.open(oname+".dat", ios::binary);
        _uint32(charcnt);
        _float((face->size->metrics.height   >>6) * baseScale);
        _float((face->size->metrics.ascender >>6) * baseScale);
        _float((face->size->metrics.descender>>6) * baseScale);
        for (int i = 0; i < charcnt; i++) { char32_t c = chars.char32At(i);
            _uint32(c);
            _float(get<0>(offsets[i]));
            _float(get<1>(offsets[i]));
            _float(get<2>(offsets[i]));
            _float(sizes[i].first  * baseScale);
            _float(sizes[i].second * baseScale);
            _float(get<0>(uvwh[i]));
            _float(get<1>(uvwh[i]));
            _float(get<2>(uvwh[i]));
            _float(get<3>(uvwh[i]));
        }
        for (auto k: kerning) {
            _float(k.first);
            _uint32(k.second);
        }

        data.close();
    }

    // Produce distance transform of image
    pair<int,int> distsize;
    if (dimensions.first > dimensions.second) {
        distsize.first = outsize;
        distsize.second = ceil(((float)dimensions.second * outsize)/dimensions.first);
    }
    else {
        distsize.second = outsize;
        distsize.first = ceil(((float)dimensions.first * outsize)/dimensions.second);
    }
    png::image<png::gray_pixel> dist (distsize.first, distsize.second);
    distanceTransform(image, dist, dimensions, distsize, searchsize);
    dist.write(oname+".png");

    return 0;
}
