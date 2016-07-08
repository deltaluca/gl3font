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
#include <sstream>
#include <cmath>
#include <set>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
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
void arg_charsfile(char* argv[], int argc, std::string& charset)
{
	for (size_t i = 1; i < (size_t)argc; ++i)
    {
        std::string arg = argv[i];
        if (arg.substr(0, 11).compare("-charsfile=") == 0)
        {
			std::ifstream in( arg.substr(11) );
			std::stringstream buffer;
			std::string to;
			if (in)
			{
				while(std::getline(in,to,'\n')){
					buffer << to;
				}
				in.close();
				charset = buffer.str();
			}
			else throw(errno);
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

struct Vec2
{
    float x, y;
};

struct Segment
{
    enum Type : uint8_t
    {
        START,
        LINETO,
        CURVETO
    } type;

    Vec2 end; // (start for START type, always first).
    Vec2 control; // defined for bezier type.
};

typedef std::vector<Segment> Outline;
typedef std::vector<Outline> OutlineSet;
bool contains(const Outline& outline, const Vec2& p);
std::vector<OutlineSet> generateOutlines(FT_Face& face, const Chars& chars, float pointSize)
{
    float scale = 1.f / (64 * pointSize);
    std::vector<OutlineSet> ret;
    for (size_t i = 0; i < chars.length; ++i)
    {
        auto c = chars[i];
        FT_Load_Glyph(face, FT_Get_Char_Index(face, c), FT_LOAD_RENDER);
        auto& outline = face->glyph->outline;

        struct Context
        {
            float scale;
            OutlineSet set;
            Outline current;
        } context;
        context.scale = scale;

        FT_Outline_Funcs interface = {
            [](const FT_Vector* to, void* user)
            {
                Context& ctx = *((Context*)user);

                float x = ((float)to->x) * ctx.scale;
                float y = ((float)to->y) * ctx.scale;

                if (!ctx.current.empty())
                {
                    ctx.set.push_back(ctx.current);
                    ctx.current.clear();
                }
                ctx.current.push_back({
                    Segment::Type::START,
                    { x, y },
                    { 0, 0 }
                });

                return 0;
            },
            [](const FT_Vector* to, void* user)
            {
                Context& ctx = *((Context*)user);

                float x = ((float)to->x) * ctx.scale;
                float y = ((float)to->y) * ctx.scale;

                ctx.current.push_back({
                    Segment::Type::LINETO,
                    { x, y },
                    { 0, 0 }
                });

                return 0;
            },
            [](const FT_Vector* control, const FT_Vector* to, void* user)
            {
                Context& ctx = *((Context*)user);

                float x = ((float)to->x) * ctx.scale;
                float y = ((float)to->y) * ctx.scale;
                float cx = ((float)control->x) * ctx.scale;
                float cy = ((float)control->y) * ctx.scale;

                ctx.current.push_back({
                    Segment::Type::CURVETO,
                    { x, y },
                    { cx, cy }
                });

                return 0;
            },
            [](const FT_Vector*, const FT_Vector*, const FT_Vector*, void*)
            {
                std::cerr << "Cannot handle cubic bezier curves in glyph. Failing!" << std::endl;
                return 1;
            },
            0, 0
        };

        if (FT_Outline_Decompose(&outline, &interface, (void*)(&context)) == 0)
        {
            if (!context.current.empty())
            {
                context.set.push_back(context.current);
            }
            ret.push_back(context.set);
        }
    }
    return ret;
}

bool contains(const Outline& outline, const Vec2& p)
{
    Vec2 prev;
    size_t count = 0;
    bool first = true;
    for (const auto& s: outline)
    {
        if (first)
        {
            assert(s.type == Segment::Type::START);
            prev = s.end;
            first = false;
            continue;
        }

        const Vec2& next = s.end;
        const Vec2& control = s.control;
        float a, b, c, d, t, t0, t1;
        switch (s.type)
        {
        case Segment::Type::LINETO:
            if (next.y != prev.y)
            {
                t = (p.y - prev.y) / (next.y - prev.y);
                if (t >= 0.f && t <= 1.f)
                {
                    if ((next.x - prev.x) * t >= (p.x - prev.x))
                    {
                        ++count;
                    }
                }
            }
            break;
        case Segment::Type::CURVETO:
            a = (prev.y + next.y - 2.f * control.y);
            b = 2.f * (control.y - prev.y);
            c = prev.y - p.y;
            d = b * b - 4.f * a * c;
            if (d >= 0.f && a != 0.f)
            {
                d = sqrt(d);
                a = 1.f / (2.f * a);
                t0 = (-b + d) * a;
                t1 = (-b - d) * a;
                if (t0 >= 0.f && t0 <= 1.f)
                {
                    if ((1.f - t0) * (1.f - t0) * prev.x + 2.f * (1.f - t0) * t0 * control.x + t0 * t0 * next.x >= p.x)
                    {
                        ++count;
                    }
                }
                if (t1 >= 0.f && t1 <= 1.f)
                {
                    if ((1.f - t1) * (1.f - t1) * prev.x + 2.f * (1.f - t1) * t1 * control.x + t1 * t1 * next.x >= p.x)
                    {
                        ++count;
                    }
                }
            }
            else if (a == 0.f && b != 0.f)
            {
                t = -c / b;
                if (t >= 0.f && t <= 1.f)
                {
                    if ((1.f - t) * (1.f - t) * prev.x + 2.f * (1.f - t) * t * control.x + t * t * next.x >= p.x)
                    {
                        ++count;
                    }
                }
            }
            break;
        default:
            assert(false);
        }
        prev = next;
    }
    return (count & 1) == 1;
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
    if (argc < 3)
    {
        std::cout << "ttfcompile fontpath.ttf pxheight gap searchsize outsize [-chars=file] [-o=outname]" << std::endl
                  << "ttfcompile -transform image.png searchsize outsize [-o=outname]" << std::endl
                  << "ttfcompile -vector fontpath.ttf [-chars=file] [-o=outname]" << std::endl;
        return 1;
    }

    if (std::string(argv[1]).compare("-transform") == 0)
    {
        std::string imagePath = argv[2];
        size_t searchSize     = atoi(argv[3]);
        size_t outSize        = atoi(argv[4]);

        std::string oname = imagePath;
        arg_output(argv, argc, oname);

        auto image = png::image<png::gray_pixel>(imagePath);
        auto dist = distanceImage(image, outSize, searchSize);
        dist.write(oname+".png");

        return 0;
    }
    else if (std::string(argv[1]).compare("-vector") != 0)
    {
        std::string fontPath = argv[1];
        size_t pxheight      = atoi(argv[2]);
        size_t gap           = atoi(argv[3]);
        size_t searchSize    = atoi(argv[4]);
        size_t outSize       = atoi(argv[5]);

        std::string oname = fontPath;
        std::string charset = ISO_7;
        arg_charsfile(argv, argc, charset);
        arg_chars(argv, argc, charset);
        arg_output(argv, argc, oname);

        FT_Face face;
        if (!loadFontFace(fontPath, face))
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

        auto dist = distanceImage(image, outSize, searchSize);
        dist.write(oname+".png");

        return 0;
    }
    else
    {
        std::string fontPath = argv[2];
        size_t pxheight = 16;

        std::string oname = fontPath;
        std::string charset = ISO_7;
        arg_charsfile(argv, argc, charset);
        arg_chars(argv, argc, charset);
        arg_output(argv, argc, oname);

        FT_Face face;
        if (!loadFontFace(fontPath, face))
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
        auto outlines = generateOutlines(face, chars, pxheight);

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
        }

        for (auto k: kerning)
        {
            data << ((float)k.first)
                 << ((uint32_t)k.second);
        }

        data.close();

        return 0;
    }
}
