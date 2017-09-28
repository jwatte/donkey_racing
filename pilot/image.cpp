#include "image.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../calibrate_camera/table.h"


/* how yellow is it? */
#define CENTER_U 128-32
#define CENTER_V 128+12
#define CENTER_Y 160

void get_unwarp_info(size_t *osize, int *owidth, int *oheight, int *oplanes) {
    *owidth = RECTIFIED_WIDTH;
    *oheight = RECTIFIED_HEIGHT;
    *oplanes = 2;
    *osize = RECTIFIED_WIDTH * RECTIFIED_HEIGHT * 2 * sizeof(float);
}

static const float BYTE_TO_FLOAT = 1.0f / 255.0f;
static const float CENTER_Y_FLOAT = ((CENTER_Y) * BYTE_TO_FLOAT);
static const float CENTER_U_FLOAT = ((CENTER_U) * BYTE_TO_FLOAT);
static const float CENTER_V_FLOAT = ((CENTER_V) * BYTE_TO_FLOAT);
float GAIN_Y = 2.0f;
float GAIN_U = 4.5f;
float GAIN_V = 4.5f;

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

static inline float sample_yuv(float y, unsigned char const *u, unsigned char const *v, TableInputCoord const tic) {
    float y1f = floorf(tic.sy * 0.5f);
    float y1d = tic.sy * 0.5f - y1f;
    int yc = (int)y1f;
    int x1f = floorf(tic.sx * 0.5f);
    float x1d = tic.sx  * 0.5f- x1f;
    int xc = (int)x1f;
    u += yc * (SOURCE_WIDTH / 2) + xc;
    int a = u[0];
    int b = u[1];
    int c = u[SOURCE_WIDTH / 2];
    int d = u[SOURCE_WIDTH / 2 + 1];
    float uret = ((a * (1.0f - x1d) + b * x1d) * (1 - y1d) +
            (c * (1.0f - x1d) + d * x1d) * y1d) * BYTE_TO_FLOAT;
    v += yc * (SOURCE_WIDTH / 2) + xc;
    a = v[0];
    b = v[1];
    c = v[SOURCE_WIDTH / 2];
    d = v[SOURCE_WIDTH / 2 + 1];
    float vret = ((a * (1.0f - x1d) + b * x1d) * (1 - y1d) +
            (c * (1.0f - x1d) + d * x1d) * y1d) * BYTE_TO_FLOAT;
    float ret = 1.25f - fabsf(y - CENTER_Y_FLOAT) * GAIN_Y - fabsf(uret - CENTER_U_FLOAT) * GAIN_U - fabsf(vret - CENTER_V_FLOAT) * GAIN_V;
    return ret > 0.999f ? 0.999f : ret < 0.001f ? 0.001f : ret;
}

void unwarp_image(void const *src, void *dst) {
    unsigned char const *y = (unsigned char const *)src;
    unsigned char const *u = (unsigned char const *)src + SOURCE_WIDTH * SOURCE_HEIGHT;
    unsigned char const *v = (unsigned char const *)u + (SOURCE_WIDTH/2)*(SOURCE_HEIGHT/2);

    float *dp1 = (float *)dst;
    float *dp2 = dp1 + RECTIFIED_WIDTH * RECTIFIED_HEIGHT;
    TableInputCoord const *ticp = &sTableInputCoords[0][0];
    for (int yy = 0; yy < RECTIFIED_HEIGHT; ++yy) {
        for (int xx = 0; xx < RECTIFIED_WIDTH; ++xx) {
            float dp1v = sample_y(y, *ticp);
            float dp2v = sample_yuv(dp1v, u, v, *ticp);
            *dp1++ = dp1v;
            *dp2++ = dp2v;
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
    unsigned char const *u = (unsigned char const *)up;
    unsigned char const *v = (unsigned char const *)vp;

    unsigned char *dp1 = (unsigned char *)dst;
    unsigned char *dp2 = dp1 + RECTIFIED_WIDTH * RECTIFIED_HEIGHT;
    TableInputCoord const *ticp = &sTableInputCoords[0][0];
    for (int yy = 0; yy < RECTIFIED_HEIGHT; ++yy) {
        for (int xx = 0; xx < RECTIFIED_WIDTH; ++xx) {
            TableInputCoord tic = transform(mat3x2, *ticp);
            float dp1v = sample_y(y, tic);
            float dp2v = sample_yuv(dp1v, u, v, tic);
            *dp1++ = (unsigned char)(FLOAT_TO_BYTE * dp1v);
            *dp2++ = (unsigned char)(FLOAT_TO_BYTE * dp2v);
            ++ticp;
        }
    }
}


/* the table of data is included once here */
#include "../calibrate_camera/table.cpp"

