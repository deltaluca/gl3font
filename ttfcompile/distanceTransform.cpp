#include <png++/png.hpp>
#include <map>
#include <string>
#include <vector>
#include <functional>
#include <set>
#include <cmath>

using namespace std;

void distanceTransform(const png::image<png::gray_pixel>& image,
                       png::image<png::gray_pixel>& dist,
                       pair<int,int> insize,
                       pair<int,int> outsize,
                       int searchsize) {
    float scaleX = ((float)insize.first) /outsize.first;
    float scaleY = ((float)insize.second)/outsize.second;

    // Raw distance map.
    float* hood = new float[outsize.first*outsize.second];
    float min = 1e20f;
    float max = -1e20f;

    auto lerp = [&](float x, float y) {
        if (x < 0 || y < 0 || x >= insize.first || y >= insize.second)
            return 1e20f;

        int x0 = (int)x;
        int y0 = (int)y;
        int x1 = x0+1; if (x1 >= insize.first) x1 = insize.first-1;
        int y1 = y0+1; if (y1 >= insize.second) y1 = insize.second-1;

        float fx = x-x0;
        float fy = y-y0;

        int g00 = image[y0][x0];
        int g10 = image[y0][x1];
        int g11 = image[y1][x1];
        int g01 = image[y1][x0];

        return ((1-fx)*(1-fy)*g00 + (1-fx)*fy*g01 + fx*(1-fy)*g10 + fx*fy*g11)-0x80;
    };

    vector<pair<float,vector<pair<float,float>>>> deltas;
    for (int i = 1; i <= searchsize*2; i++) {
        float r = ((float)i)/2;
        int N = 2*3.14159265358979323846*r;
        vector<pair<float,float>> ring;
        for (int j = 0; j < N; j++) {
            ring.push_back(pair<float,float>(
                r*cos(j*3.14159265358979323846*2/N), r*sin(j*3.14159265358979323846*2/N)
            ));
        }
        deltas.push_back(pair<float,vector<pair<float,float>>>(r, ring));
    }


    for (int iy = 0; iy < outsize.second; iy++) {
    for (int ix = 0; ix < outsize.first; ix++) {
        // Pixel location on src image.
        float bx = (ix+0.5)*scaleX;
        float by = (iy+0.5)*scaleY;

        float m = lerp(bx, by);
        bool inside = m >= 0;
        float minDistance = 1e20f;
        if (m != 0) {
            // Radial outwards search.
            for (auto ring: deltas) {
                for (auto delta: ring.second) {
                    function<float(int,float,float,float,float)> binary = [&](int N, float v0, float v1, float k0, float k1) {
                        float k = k0 + (k1-k0)*(0 - v0)/(v1 - v0);
                        float vk = lerp(bx + delta.first*k, by + delta.second*k);
                        if (vk*vk < 1e-5 || N > 20) return k;
                        if (v0*vk < 0) return binary(N+1,v0, vk, k0, k);
                        if (vk*v1 < 0) return binary(N+1,vk, v1, k, k1);
                    };
                    float n = lerp(bx + delta.first, by + delta.second);
                    if (n != 1e20f && n*m <= 0) {
                        float dist = ring.first;
                        if (n == 0) minDistance = dist;
                        else minDistance = dist * binary(0,m, n, 0, 1);
                    }
                }
                if (minDistance != 1e20f) break;
            }
        }
        else minDistance = 0;

        if (minDistance != 1e20f) {
            if (!inside) minDistance = -minDistance;
            if (minDistance < min) min = minDistance;
            if (minDistance > max) max = minDistance;
        }
        else if (!inside) minDistance = -minDistance;
        hood[iy*outsize.first+ix] = minDistance;
    }}

    // Perform scaling.
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
        dist[iy][ix] = (int)m;
    }}

    delete [] hood;
}

void distanceTraxnsform(const png::image<png::gray_pixel>& image,
                       png::image<png::gray_pixel>& dist,
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

        int g00 = image[y0][x0];
        int g10 = image[y0][x1];
        int g11 = image[y1][x1];
        int g01 = image[y1][x0];

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
        dist[iy][ix] = (int)m;
    }}

    delete [] hood;
}
