#include "yuv.h"

static inline unsigned char cl255(float f) {
    if (f < 0) return 0;
    if (f > 255) return 255;
    return (unsigned char)f;
}

YUV_EXTERN void yuv_comp_to_rgb(unsigned char y, unsigned char u, unsigned char v, unsigned char *rgb) {
    float b = y;
    float va = v *  1.140;
    float vb = v * -0.581;
    float ub = u * -0.395;
    float uc = u *  2.032;
    rgb[0] = cl255(b + va     );
    rgb[1] = cl255(b + vb + ub);
    rgb[2] = cl255(b      + uc);
}

void yuv_to_rgb(unsigned char const *yuv, unsigned char *rgb, int xs, int ys) {

    unsigned char const *u = yuv + xs * ys;
    unsigned char const *v = u + xs * ys / 4;

    for (int row = 0; row != ys; row += 2) {

        for (int col = 0; col != xs; col += 2) {

            float u0 = (float)u[0] - 128.0f;
            float v0 = (float)v[0] - 128.0f;
#if 0
            float y0 = yuv[0] - 16;
            float y1 = yuv[1] - 16;
            float y2 = yuv[xs] - 16;
            float y3 = yuv[xs+1] - 16;
            float va = v0 * 1.596f;     //   1.140?
            float vb = v0 * -0.813f;    //  -0.581?
            float ub = u0 * -0.391f;    //   0.395?
            float uc = u0 * 2.018f;     //   2.032?
#define Y_SCALE 1.164f *
#else
            float y0 = yuv[0];
            float y1 = yuv[1];
            float y2 = yuv[xs];
            float y3 = yuv[xs+1];
            float va = v0 *  1.140;
            float vb = v0 * -0.581;
            float ub = u0 * -0.395;
            float uc = u0 *  2.032;
#define Y_SCALE 
#endif

            float b = Y_SCALE y0;
            rgb[0] = cl255(b + va     );
            rgb[1] = cl255(b + vb + ub);
            rgb[2] = cl255(b      + uc);
            b = Y_SCALE y1;
            rgb[3] = cl255(b + va     );
            rgb[4] = cl255(b + vb + ub);
            rgb[5] = cl255(b      + uc);
            b = Y_SCALE y2;
            rgb[xs*3+0] = cl255(b + va     );
            rgb[xs*3+1] = cl255(b + vb + ub);
            rgb[xs*3+2] = cl255(b      + uc);
            b = Y_SCALE y3;
            rgb[xs*3+3] = cl255(b + va     );
            rgb[xs*3+4] = cl255(b + vb + ub);
            rgb[xs*3+5] = cl255(b      + uc);

            yuv += 2;
            rgb += 6;
            ++u;
            ++v;
        }

        yuv += xs;
        rgb += xs * 3;
    }
}

void rgb_to_yuv(unsigned char *oot, unsigned char const *d, int xs, int ys) {
    unsigned char *y = oot;
    unsigned char *u = y + xs * ys;
    unsigned char *v = u + xs * ys / 4;
    for (int r = 0; r < ys; r += 2) {
        for (int c = 0; c < xs; c += 2) {
            float y0 = d[0] * 0.299f + d[1] * 0.587f + d[2] * 0.114f;
            float u0 = 0.492f * (d[2] - y0);
            float v0 = 0.492f * (d[0] - y0);
            float y1 = d[3] * 0.299f + d[4] * 0.587f + d[5] * 0.114f;
            float u1 = 0.492f * (d[5] - y1);
            float v1 = 0.492f * (d[3] - y1);
            float y2 = d[xs*3+0] * 0.299f + d[xs*3+1] * 0.587f + d[xs*3+2] * 0.114f;
            float u2 = 0.492f * (d[xs*3+2] - y2);
            float v2 = 0.492f * (d[xs*3+0] - y2);
            float y3 = d[xs*3+3] * 0.299f + d[xs*3+4] * 0.587f + d[xs*3+5] * 0.114f;
            float u3 = 0.492f * (d[xs*3+5] - y3);
            float v3 = 0.492f * (d[xs*3+3] - y3);
            y[0] = cl255(y0);
            y[1] = cl255(y1);
            y[xs] = cl255(y2);
            y[xs+1] = cl255(y3);
            u[0] = cl255((u0 + u1 + u2 + u3) * 0.25f + 128);
            v[0] = cl255((v0 + v1 + v2 + v3) * 0.25f + 128);
            y += 2;
            u += 1;
            v += 1;
            d += 6;
        }
        y += xs;
        d += xs * 3;
    }
}


