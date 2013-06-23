#define IMPLEMENT_API
#define NEKO_COMPATIBLE
#include <hx/CFFI.h>
#include <png++/png.hpp>

struct Vector {
    void* dat;
    unsigned int size;
};

DECLARE_KIND(k_Vector)
DEFINE_KIND(k_Vector)

void finalise_Vector(value v) {
    Vector* vec = (Vector*)val_data(v);
    free(vec->dat);
}

//
// unsafe, but for performance.
//
// gray pixels, row stride = width + 8
//              and packed.
// rgba pixels, row stride = width*4 + 16
//              rgba elements are packed.
//              rgba are packed struct.
//              and rgba data is packed!
//

value hx_gl3font_png_grey(value path, value dims) {
    png::image<png::gray_pixel> image;
    image = png::image<png::gray_pixel>(val_string(path));
    int w = image.get_width();
    int h = image.get_height();

    Vector* ret = new Vector;
    ret->dat = malloc(ret->size = w*h);
    for (int y = 0; y < h; y++) memcpy(ret->dat + y*w, &image[y][0], w);

    value v = alloc_abstract(k_Vector, ret);
    val_gc(v, finalise_Vector);

    val_array_set_i(dims, 0, alloc_int(w));
    val_array_set_i(dims, 1, alloc_int(h));

    return v;
}
DEFINE_PRIM(hx_gl3font_png_grey, 2)

value hx_gl3font_png_rgba(value path, value dims) {
    png::image<png::rgba_pixel> image;
    image = png::image<png::rgba_pixel>(val_string(path));
    int w = image.get_width();
    int h = image.get_height();

    Vector* ret = new Vector;
    ret->dat = malloc(ret->size = w*h*4);
    for (int y = 0; y < h; y++) memcpy(ret->dat + y*w*4, &image[y][0], w*4);

    value v = alloc_abstract(k_Vector, ret);
    val_gc(v, finalise_Vector);

    val_array_set_i(dims, 0, alloc_int(w));
    val_array_set_i(dims, 1, alloc_int(h));

    return v;
}
DEFINE_PRIM(hx_gl3font_png_rgba, 2)

extern "C" void hx_gl3font_entry() {
    k_Vector = alloc_kind();
}
DEFINE_ENTRY_POINT(hx_gl3font_entry);
