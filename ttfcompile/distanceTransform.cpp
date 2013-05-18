#include <png++/png.hpp>
#include <map>
#include <string>
#include <vector>
#include <functional>
#include <set>
#include <cmath>

using namespace std;

void distanceTransform(const png::image<png::rgb_pixel>& image,
                       png::image<png::rgb_pixel>& dist,
                       pair<int,int> insize,
                       pair<int,int> outsize,
                       int searchsize) {
   float scaleX = ((float)insize.first) /outsize.first;
   float scaleY = ((float)insize.second)/outsize.second;

   auto lerp = [&](float x, float y) {
        if (x < 0 || y < 0 || x >= insize.first || y >= insize.second)
            return 1e20f;

        int x0 = (int)x;
        int y0 = (int)y;
        int x1 = x0+1; if (x1 >= insize.first) x1 = insize.first-1;
        int y1 = y0+1; if (y1 >= insize.second) y1 = insize.second-1;

        float fx = x-x0;
        float fy = y-y0;

        int g00 = image[y0][x0].red;
        int g10 = image[y0][x1].red;
        int g11 = image[y1][x1].red;
        int g01 = image[y1][x0].red;

        return ((1-fx)*(1-fy)*g00 + (1-fx)*fy*g01 + fx*(1-fy)*g10 + fx*fy*g11)-0x80;
    };

    float* hood = new float[outsize.first*outsize.second];
    float min = 1e20f;
    float max = -1e20f;

    for (int iy = 0; iy < outsize.second; iy++) {
    for (int ix = 0; ix < outsize.first; ix++) {
        float m = lerp((ix+0.5)*scaleX, (iy+0.5)*scaleY);
        bool inside = m >= 0;

        float minDistance = 1e20f;
        for (int y = -searchsize; y <= searchsize; y++) {
            float ay = ((iy+0.5)*scaleY) + y;
            for (int x = -searchsize; x <= searchsize; x++) {
                float ax = ((ix+0.5)*scaleX) + x;

                float n = lerp(ax, ay);
                if (n == 1e20f) continue; // outside
                if (n*m <= 0) {
                    float dist = sqrt(x*x+y*y);
                    if (dist < minDistance) minDistance = dist;
                }
            }
        }
        if (minDistance != 1e20f) {
            if (!inside) minDistance = -minDistance;
            if (minDistance < min) min = minDistance;
            if (minDistance > max) max = minDistance;
        }
        else {
            if (!inside) minDistance = -minDistance;
        }
        hood[iy*outsize.first+ix] = minDistance;
    }}

    float mm = fmax(fabs(min), fabs(max));
    min = -mm;
    max = mm;
    float det = 255.0/(max-min);
    for (int iy = 0; iy < outsize.second; iy++) {
    for (int ix = 0; ix < outsize.first; ix++) {
        float m = hood[iy*outsize.first+ix];
        if (m == 1e20f) m = 255;
        else if (m == -1e20f) m = 0;
        else
            m = (m - min)*det;
        int g = (int)m;
        dist[iy][ix] = png::rgb_pixel(g,g,g);
    }}

    delete [] hood;
}
