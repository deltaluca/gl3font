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

void arg_chars(char* argv[], int argc, std::string& charset)
{
    for (size_t i = 1; i < (size_t)argc; ++i)
    {
        std::string arg = argv[i];
        if (arg.substr(0, 7).compare("-chars=") == 0)
        {
            charset = arg.substr(7);
        }
    }
}

void arg_output(char* argv[], int argc, std::string& out)
{
    for (size_t i = 1; i < (size_t)argc; ++i)
    {
        std::string arg = argv[i];
        if (arg.substr(0, 3).compare("-o=") == 0)
        {
            out = arg.substr(3);
        }
    }
}

const std::string ASCII = u8"\t!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ ";
const std::string ISO_1 = ASCII + u8"¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿ";
const std::string ISO_7 = ISO_1 + u8"‘’₯―΄΅ΆΈΉΊΌΎΏΐΑΒΓΔΕΖΗΘΙΚΛΜΝΞΟΠΡΣΤΥΦΧΨΩΪΫάέήίΰαβγδεζηθικλμνξοπρςστυφχψωϊϋόύώ";

bool loadFontFace(const std::string& path, FT_Face& face)
{
    FT_Library library;
    FT_Init_FreeType(&library);

    auto err = FT_New_Face(library, path.c_str(), 0, &face);
    if (err == FT_Err_Unknown_File_Format)
    {
        std::cerr << "Unsupported file format" << std::endl;
        return false;
    }
    else if (err)
    {
        std::cerr << "Failed to load font (FT errc = " << err << ")" << std::endl;
        return false;
    }

    return true;
}

struct Chars
{
    icu::UnicodeString chars;
    size_t length;
    char32_t operator[](size_t i) const
    {
        return chars.char32At(i);
    }
};
Chars unicode_map(const std::string& charset)
{
    Chars ret;
    ret.chars = icu::UnicodeString::fromUTF8(icu::StringPiece(charset));
    ret.length = ret.chars.countChar32();
    return ret;
}

// produce RLE kerning table. Ignores possibility of vertical kernings.
std::vector<std::pair<float, int>> generateKerningTable(FT_Face& face, const Chars& chars, float pointSize)
{
    float prev;
    size_t cnt = 0;
    std::vector<std::pair<float, int>> table;
    for (size_t i = 0; i < chars.length; ++i)
    {
        for (size_t j = 0; j < chars.length; ++j)
        {
            FT_Vector delta;
            FT_Get_Kerning(face,
                           FT_Get_Char_Index(face, chars[i]),
                           FT_Get_Char_Index(face, chars[j]),
                           FT_KERNING_DEFAULT,
                           &delta);
            if (delta.y != 0)
            {
                std::cout << "Warning: Kerning y != 0 and is ignored" << std::endl;
            }

            float curr = (delta.x / 64.f) / pointSize;
            if (curr == prev && cnt != 0)
            {
                ++cnt;
            }
            else
            {
                if (cnt != 0)
                {
                    table.emplace_back(prev, cnt);
                }
                prev = curr;
                cnt = 1;
            }
        }
    }
    if (cnt != 0)
    {
        table.emplace_back(prev, cnt);
    }
    return table;
}

// generate list of glyph sizes and offsets
struct GlyphMeta
{
    float width, height;
    float xAdvance;
    float offsetX, offsetY;
};
std::vector<GlyphMeta> computeGlyphMeta(FT_Face& face, const Chars& chars, float pointSize)
{
    std::vector<GlyphMeta> meta;
    for (size_t i = 0; i < chars.length; ++i)
    {
        auto c = chars[i];
        FT_Load_Glyph(face, FT_Get_Char_Index(face, c), FT_LOAD_RENDER);
        if (face->glyph->advance.y != 0)
        {
            std::cout << "Warning: Advance y != 0 and is ignored" << std::endl;
        }
        const auto& metrics = face->glyph->metrics;
        meta.push_back({
            (metrics.width        / 64.f) / pointSize,
            (metrics.height       / 64.f) / pointSize,
            (metrics.horiAdvance  / 64.f) / pointSize,
            (metrics.horiBearingX / 64.f) / pointSize,
            (metrics.horiBearingY / 64.f) / pointSize
        });
    }
    return meta;
}

// perform bin-packing and generate tex rectangles and image for font
struct TexRectangle
{
    float x, y, w, h;
};
png::image<png::gray_pixel> packGlyphs(FT_Face& face, const Chars& chars, const std::vector<GlyphMeta>& meta, size_t gap,
                                       std::vector<TexRectangle>& uvwh)
{
    std::vector<std::pair<size_t, size_t>> integerSizes;
    for (size_t i = 0; i < chars.length; ++i)
    {
        auto c = chars[i];
        FT_Load_Glyph(face, FT_Get_Char_Index(face, c), FT_LOAD_RENDER);
        auto m = face->glyph->bitmap;
        integerSizes.emplace_back(m.width, m.rows);
    }

    std::pair<size_t, size_t> dimensions;
    std::map<size_t, std::pair<size_t, size_t>> packing;
    pack_boxes(integerSizes, gap, packing, dimensions);

    png::image<png::gray_pixel> image (dimensions.first, dimensions.second);
    float uScale = 1.f / dimensions.first;
    float vScale = 1.f / dimensions.second;
    for (size_t i = 0; i < chars.length; ++i)
    {
        auto c = chars[i];
        auto& xy = packing[i];

        FT_Load_Glyph(face, FT_Get_Char_Index(face, c), FT_LOAD_RENDER);
        auto m = face->glyph->bitmap;

        for (size_t iy = 0; iy < m.rows; ++iy)
        {
            for (size_t ix = 0; ix < m.width; ++ix)
            {
                image[xy.second + iy][xy.first + ix] = m.buffer[iy * m.pitch + ix];
            }
        }

        uvwh.push_back({
            xy.first * uScale, xy.second * vScale,
            m.width * uScale, m.rows * vScale
        });
    }

    return image;
}

png::image<png::gray_pixel> distanceImage(const png::image<png::gray_pixel>& src, size_t outSize, size_t searchSize)
{
    auto width = src.get_width();
    auto height = src.get_height();
    if (width > height)
    {
        height = ceil(((float)height * outSize) / width);
        width = outSize;
    }
    else
    {
        width = ceil(((float)width * outSize) / height);
        height = outSize;
    }

    png::image<png::gray_pixel> dst (width, height);
    distanceTransform(src, dst, { src.get_width(), src.get_height() }, { width, height }, searchSize);
    return dst;
}

struct OutStream
{
    std::ofstream file;
    bool open(const std::string& path)
    {
        file.open(path, std::ios::binary);
        return (bool)file;
    }

    template <typename T>
    OutStream& operator<<(T x)
    {
        file.write((char*)&x, sizeof(T));
        return *this;
    }

    void close()
    {
        file.close();
    }
};

int main(int argc, char* argv[])
{
    if (argc < 5)
    {
        std::cout << "ttfcompile fontpath.ttf pxheight gap searchsize outsize [-chars=file] [-o=outname]" << std::endl
                  << "ttfcompile -transform image.png searchsize outsize [-o=outname]" << std::endl;
        return 1;
    }

    if (std::string(argv[1]).compare("-transform") == 0)
    {
        std::string imagePath = argv[1];
        size_t searchSize     = atoi(argv[2]);
        size_t outSize        = atoi(argv[3]);

        std::string oname = imagePath;
        arg_output(argv, argc, oname);

        auto dist = distanceImage(image, outsize, searchsize);
        dist.write(oname+".png");

        return 0;
    }
    else
    {
        std::string fontPath = argv[1];
        size_t pxheight      = atoi(argv[2]);
        size_t gap           = atoi(argv[3]);
        size_t searchSize    = atoi(argv[4]);
        size_t outSize       = atoi(argv[5]);

        std::string oname = fontPath;
        std::string charset = ISO_7;
        arg_chars(argv, argc, charset);
        arg_output(argv, argc, oname);

        FT_Face face;
        if (!loadFontFace(fontpath, face))
        {
            return 1;
        }
        if (FT_Set_Pixel_Sizes(face, 0, pxheight))
        {
            std::cerr << "Failed to set font size" << std::endl;
            return 1;
        }

        auto chars = unicode_map(charset);
        auto kerning = generateKerningTable(face, chars, pxheight);
        auto meta = computeGlyphMeta(face, chars, pxheight);

        std::vector<TexRectangle> uvwh;
        auto image = packGlyphs(face, chars, meta, gap, uvwh);

        OutStream data;
        if (!data.open(oname + ".dat"))
        {
            std::cerr << "failed to open output file" << std::endl;
            return 1;
        }

        data << ((uint32_t)chars.length);

        data << ((float)((face->size->metrics.height    / 64.f) / pxheight))
             << ((float)((face->size->metrics.ascender  / 64.f) / pxheight))
             << ((float)((face->size->metrics.descender / 64.f) / pxheight));

        for (size_t i = 0; i < chars.length; ++i)
        {
            data << ((uint32_t)chars[i]);

            data << ((float)(meta[i].xAdvance))
                 << ((float)(meta[i].offsetX))
                 << ((float)(meta[i].offsetY))
                 << ((float)(meta[i].width))
                 << ((float)(meta[i].height));

            data << ((float)uvwh[i].x)
                 << ((float)uvwh[i].y)
                 << ((float)uvwh[i].w)
                 << ((float)uvwh[i].h);
        }

        for (auto k: kerning)
        {
            data << ((float)k.first)
                 << ((uint32_t)k.second);
        }

        data.close();

        auto dist = distanceImage(image, outsize, searchsize);
        dist.write(oname+".png");

        return 0;
    }
}
