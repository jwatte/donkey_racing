#include "image.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../calibrate_camera/table.h"



/* TODO: Currently only B/W images (Y channel) */

void get_unwarp_info(size_t *osize, int *owidth, int *oheight, int *oplanes) {
    *owidth = RECTIFIED_WIDTH;
    *oheight = RECTIFIED_HEIGHT;
    *oplanes = 1;
    *osize = RECTIFIED_WIDTH * RECTIFIED_HEIGHT * 1 * sizeof(float);
}

static const float BYTE_TO_FLOAT = 1.0f / 255.0f;
static const float FLOAT_TO_BYTE = 255.0f;

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
    return ((a * (1.0f - x1d) + b * x1d) * (1 - y1d) +
            (c * (1.0f - x1d) + d * x1d) * y1d) * BYTE_TO_FLOAT;
}

void unwarp_image(void const *src, void *dst) {
    unsigned char const *y = (unsigned char const *)src;

    float *dp1 = (float *)dst;
    TableInputCoord const *ticp = &sTableInputCoords[0][0];
    for (int yy = 0; yy < RECTIFIED_HEIGHT; ++yy) {
        for (int xx = 0; xx < RECTIFIED_WIDTH; ++xx) {
            float dp1v = sample_y(y, *ticp);
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

