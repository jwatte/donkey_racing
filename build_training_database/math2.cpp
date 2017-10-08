#include "math2.h"
#include <math.h>

float *m2_identity(float *dst) {
    dst[0] = 1;
    dst[1] = 0;
    dst[2] = 0;
    dst[3] = 0;
    dst[4] = 1;
    dst[5] = 0;
    return dst;
}

float *m2_translate(float const *left, float dx, float dy, float *right) {
    right[0] = left[0];
    right[1] = left[1];
    right[2] = left[2] + dx;
    right[3] = left[3];
    right[4] = left[4];
    right[5] = left[5] + dy;
    return right;
}

float *m2_translation(float dx, float dy, float *dst) {
    dst[0] = 1;
    dst[1] = 0;
    dst[2] = dx;
    dst[3] = 0;
    dst[4] = 1;
    dst[5] = dy;
    return dst;
}

float *m2_rotation(float dr, float *dst) {
    float s = sinf(dr);
    float c = cosf(dr);
    dst[0] = c;
    dst[1] = -s;
    dst[2] = 0;
    dst[3] = s;
    dst[4] = c;
    dst[5] = 0;
    return dst;
}

float *m2_mul(float const *left, float const *right, float *dst) {
    dst[0] = left[0] * right[0] + left[1] * right[3] + left[2];
    dst[1] = left[0] * right[1] + left[1] * right[4] + left[2];
    dst[2] = left[0] * right[2] + left[1] * right[5] + left[2];
    dst[3] = left[3] * right[0] + left[4] * right[3] + left[5];
    dst[4] = left[3] * right[1] + left[4] * right[4] + left[5];
    dst[5] = left[3] * right[2] + left[4] * right[5] + left[5];
    return dst;
}

float *m2_transform_vec(float const *mat, float const *vec, float *dst) {
    dst[0] = mat[0] * vec[0] + mat[1] * vec[1] + mat[2];
    dst[1] = mat[3] * vec[0] + mat[4] * vec[2] + mat[5];
    return dst;
}



