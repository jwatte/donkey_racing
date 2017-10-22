#include "image.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../calibrate_camera/table.h"
#include "yuv.h"
#include "../stb/stb_image_write.h"


//  215.5 -57.8 7.1
//
#define CENTER_Y 215
#define CENTER_U -58
#define CENTER_V 7

/* TODO: Currently only B/W images (Y channel) */

void get_unwarp_info(size_t *osize, int *owidth, int *oheight, int *oplanes) {
    *owidth = RECTIFIED_WIDTH;
    *oheight = RECTIFIED_HEIGHT;
    *oplanes = 1;
    *osize = RECTIFIED_WIDTH * RECTIFIED_HEIGHT * 1 * sizeof(float);
}

static const float BYTE_TO_FLOAT = 1.0f / 255.0f;
static const float FLOAT_TO_BYTE = 255.0f;

static inline float munge(int i) {
    float f = (((float)i) - 20) * 0.9f;
    return (f < 0) ? 0 : (f > 255) ? 255 : f;
}

static inline float munge(float f) {
    return (f < 0) ? 0 : (f > 255) ? 255 : f;
}

static inline float sample_yuv(unsigned char const *y, unsigned char const *u, unsigned char const *v, TableInputCoord const tic) {
    float y1f = floorf(tic.sy);
    float y1d = tic.sy - y1f;
    int yc = (int)y1f;
    int x1f = floorf(tic.sx);
    float x1d = tic.sx - x1f;
    int xc = (int)x1f;
    y += yc * SOURCE_WIDTH + xc;
    int a = y[0];
    int b = y[1];
    int c = y[SOURCE_WIDTH];
    int d = y[SOURCE_WIDTH+1];
    float yy = ((a * (1.0f - x1d) + b * x1d) * (1 - y1d) +
            (c * (1.0f - x1d) + d * x1d) * y1d);
    int uy = yc / 2;
    int ux = xc / 2;
    float uu = u[uy * (SOURCE_WIDTH/2) + ux]-128;
    float vv = v[ux]-128;
    return munge(255 + yy*1.5f - (fabsf(yy-CENTER_Y) + fabsf(uu-CENTER_U) + fabsf(vv-CENTER_V)) * 0.65f) * BYTE_TO_FLOAT;
}

static inline float sample_y(unsigned char const *y, TableInputCoord const tic) {
    float y1f = floorf(tic.sy);
    float y1d = tic.sy - y1f;
    int yc = (int)y1f;
    int x1f = floorf(tic.sx);
    float x1d = tic.sx - x1f;
    int xc = (int)x1f;
    y += yc * SOURCE_WIDTH + xc;
    int a = y[0];
    int b = y[1];
    int c = y[SOURCE_WIDTH];
    int d = y[SOURCE_WIDTH+1];
    return munge(((a * (1.0f - x1d) + b * x1d) * (1 - y1d) +
            (c * (1.0f - x1d) + d * x1d) * y1d)) * BYTE_TO_FLOAT;
}

void unwarp_image(void const *src, void *dst) {
    unsigned char const *y = (unsigned char const *)src;
    unsigned char const *u = y + SOURCE_WIDTH * SOURCE_HEIGHT;
    unsigned char const *v = u + (SOURCE_WIDTH/2) * (SOURCE_HEIGHT/2);

    float *dp1 = (float *)dst;
    TableInputCoord const *ticp = &sTableInputCoords[0][0];
    for (int yy = 0; yy < RECTIFIED_HEIGHT; ++yy) {
        for (int xx = 0; xx < RECTIFIED_WIDTH; ++xx) {
            float dp1v = sample_yuv(y, u, v, *ticp);
            *dp1++ = dp1v;
            ++ticp;
        }
    }
}

static inline TableInputCoord transform(float const *mat3x2, TableInputCoord const &ti) {
    TableInputCoord ret = {
        ti.sx * mat3x2[3] + ti.sy * mat3x2[4] + mat3x2[5],
        ti.sx * mat3x2[0] + ti.sy * mat3x2[1] + mat3x2[2]
    };
    return ret;
}

void unwarp_transformed_bytes(void const *yp, void const *up, void const *vp, float const *mat3x2, void *dst) {
    unsigned char const *y = (unsigned char const *)yp;

    unsigned char *dp1 = (unsigned char *)dst;
    TableInputCoord const *ticp = &sTableInputCoords[0][0];
    for (int yy = 0; yy < RECTIFIED_HEIGHT; ++yy) {
        for (int xx = 0; xx < RECTIFIED_WIDTH; ++xx) {
            TableInputCoord tic = transform(mat3x2, *ticp);
            float dp1v = sample_y(y, tic);
            *dp1++ = (unsigned char)(FLOAT_TO_BYTE * dp1v);
            ++ticp;
        }
    }
}

/* the table of data is included once here */
#include "../calibrate_camera/table.cpp"

