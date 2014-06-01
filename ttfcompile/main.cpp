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

using std::size_t;

void pack_boxes(const std::vector<std::pair<size_t, size_t>>& sizes, size_t gap,
                std::map<size_t, std::pair<size_t, size_t>>& packing,
                std::pair<size_t, size_t>& size);

void distanceTransform(const png::image<png::gray_pixel>& image,
                       png::image<png::gray_pixel>& dist,
                       std::pair<size_t, size_t> insize,
                       std::pair<size_t, size_t> outsize,
                       size_t searchsize);

int main(int argc, char* argv[])
{
    if (argc < 5)
    {
        std::cout << "ttfcompile fontpath.ttf pxheight gap searchsize outsize [-chars=file] [-o=outname]" << std::endl
                  << "ttfcompile -transform image.png searchsize outsize [-o=outname]" << std::endl;
        return 1;
    }

    // Parse arguments
    std::string fontpath = argv[1];
    std::string oname = fontpath;
    std::string iso1 = u8"\t!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿ";
    std::string iso7s1 = u8"‘’₯―΄΅ΆΈΉΊΌΎΏΐΑΒΓΔΕΖΗΘΙΚΛΜΝΞΟΠΡΣΤΥΦΧΨΩΪΫάέήίΰαβγδεζηθικλμνξοπρςστυφχψωϊϋόύώ"; //iso7-iso1
    std::string charset = iso1 + iso7s1;
    for (size_t i = 0; i < (size_t)argc; ++i)
    {
        std::string arg = argv[i];
        if (arg.substr(0,7).compare("-chars=") == 0)
        {
            charset = arg.substr(7);
        }
        if (arg.substr(0,3).compare("-o=") == 0)
        {
            oname = arg.substr(3);
        }
    }

    size_t searchsize, outsize;

    std::pair<size_t, size_t> dimensions;
    png::image<png::gray_pixel> image;

    if (fontpath.compare("-transform") == 0)
    {
        searchsize = atoi(argv[3]);
        outsize    = atoi(argv[4]);
        image = png::image<png::gray_pixel>(argv[2]);
        dimensions.first  = image.get_width();
        dimensions.second = image.get_height();
    }
    else
    {
        size_t pxheight = atoi(argv[2]);
        size_t gap      = atoi(argv[3]);
        searchsize = atoi(argv[4]);
        outsize    = atoi(argv[5]);

        float baseScale = 1.f / pxheight;

        // Set up freetype and fonts
        FT_Library library;
        FT_Init_FreeType(&library);
        FT_Face face;
        int err = FT_New_Face(library, fontpath.c_str(), 0, &face);
        if (err == FT_Err_Unknown_File_Format)
        {
            std::cerr << "Unsupported file format" << std::endl;
            return 1;
        }
        else if (err)
        {
            std::cerr << "Failed to load font" << std::endl;
            return 1;
        }
        if (FT_Set_Pixel_Sizes(face, 0, pxheight))
        {
            std::cerr << "Failed to set font size" << std::endl;
            return 1;
        }

        // Retreive unicode code points, and define mapping.
        auto chars = icu::UnicodeString::fromUTF8(icu::StringPiece(charset));
        size_t charcnt = chars.countChar32();
        std::map<char32_t, size_t> idmap;
        for (size_t i = 0, id = 0; i < charcnt; ++i)
        {
            char32_t c = chars.char32At(i);
            idmap[c] = id++;
        }

        // Produce kerning table (compressed)
        float prev;
        size_t cnt = 0;
        std::vector<std::pair<float, int>> kerning;
        for (size_t i = 0; i < charcnt; ++i)
        {
            char32_t c = chars.char32At(i);
            for (size_t j = 0; j < charcnt; ++j)
            {
                char32_t d = chars.char32At(j);
                FT_Vector delta;
                FT_Get_Kerning(face,
                               FT_Get_Char_Index(face, c),
                               FT_Get_Char_Index(face, d),
                               FT_KERNING_DEFAULT,
                               &delta);
                if (delta.y != 0)
                {
                    std::cout << "Warning: Kerning y != 0 and ignored" << std::endl;
                }

                float curr = (delta.x >> 6) * baseScale;
                if (curr == prev && cnt != 0)
                {
                    ++cnt;
                }
                else
                {
                    if (cnt != 0)
                    {
                        kerning.push_back(std::pair<float, size_t>(prev, cnt));
                    }
                    prev = curr;
                    cnt = 1;
                }
            }
        }
        if (cnt != 0)
        {
            kerning.push_back(std::pair<float, size_t>(prev, cnt));
        }

        // Produce character sizes for packing, and offsets
        std::vector<std::pair<size_t, size_t>> sizes;  // width/height (texels)
        std::vector<std::tuple<float, float, float>> offsets;  // advance/left/top
        for (size_t i = 0; i < charcnt; ++i)
        {
            char32_t c = chars.char32At(i);
            FT_Load_Glyph(face, FT_Get_Char_Index(face, c), FT_LOAD_RENDER);
            if (face->glyph->advance.y != 0)
            {
                std::cout << "Warning: Advance y != 0 and ignored" << std::endl;
            }

            offsets.push_back(std::tuple<float, float, float>(
                (face->glyph->advance.x >> 6) * baseScale,
                (face->glyph->bitmap_left) * baseScale,
                (face->glyph->bitmap.rows-face->glyph->bitmap_top) * baseScale
            ));
            sizes.push_back(std::pair<size_t, size_t>(
                face->glyph->bitmap.width,
                face->glyph->bitmap.rows
            ));
        }

        // Perform bin packing.
        std::map<size_t, std::pair<size_t, size_t>> packing;
        pack_boxes(sizes, gap, packing, dimensions);

        // Pack glyphs, and produce uvwh details
        image = png::image<png::gray_pixel>(dimensions.first, dimensions.second);
        float uScale = 1.f / dimensions.first;
        float vScale = 1.f / dimensions.second;
        std::vector<std::tuple<float, float, float, float>> uvwh;
        for (size_t i = 0; i < charcnt; ++i)
        {
            char32_t c = chars.char32At(i);
            auto& size = sizes[idmap[c]];
            auto& xy = packing[idmap[c]];

            FT_Load_Glyph(face, FT_Get_Char_Index(face, c), FT_LOAD_RENDER);
            auto m = face->glyph->bitmap;

            for (size_t iy = 0; iy < m.rows; ++iy)
            {
                for (size_t ix = 0; ix < m.width; ++ix)
                {
                    image[xy.second + iy][xy.first + ix] = m.buffer[iy * m.pitch + ix];
                }
            }

            uvwh.push_back(std::tuple<float, float, float, float>(
                xy.first   * uScale, xy.second   * vScale,
                size.first * uScale, size.second * vScale
            ));
        }

        // Output descriptor data.
        std::ofstream data;
        auto _uint32 = [&](uint32_t x) { data.write((char*)&x, sizeof(uint32_t)); };
        auto _float  = [&](float x)    { data.write((char*)&x, sizeof(float)); };

        data.open(oname + ".dat", std::ios::binary);

        _uint32(charcnt);

        _float((face->size->metrics.height    >> 6) * baseScale);
        _float((face->size->metrics.ascender  >> 6) * baseScale);
        _float((face->size->metrics.descender >> 6) * baseScale);

        for (size_t i = 0; i < charcnt; ++i)
        {
            char32_t c = chars.char32At(i);
            _uint32(c);

            _float(std::get<0>(offsets[i]));
            _float(std::get<1>(offsets[i]));
            _float(std::get<2>(offsets[i]));

            _float(sizes[i].first  * baseScale);
            _float(sizes[i].second * baseScale);

            _float(std::get<0>(uvwh[i]));
            _float(std::get<1>(uvwh[i]));
            _float(std::get<2>(uvwh[i]));
            _float(std::get<3>(uvwh[i]));
        }

        for (auto k: kerning)
        {
            _float(k.first);
            _uint32(k.second);
        }

        data.close();
    }

    // Produce distance transform of image
    std::pair<size_t, size_t> distsize;
    if (dimensions.first > dimensions.second)
    {
        distsize.first = outsize;
        distsize.second = ceil(((float)dimensions.second * outsize) / dimensions.first);
    }
    else
    {
        distsize.second = outsize;
        distsize.first = ceil(((float)dimensions.first * outsize) / dimensions.second);
    }
    png::image<png::gray_pixel> dist (distsize.first, distsize.second);
    distanceTransform(image, dist, dimensions, distsize, searchsize);
    dist.write(oname+".png");

    return 0;
}
