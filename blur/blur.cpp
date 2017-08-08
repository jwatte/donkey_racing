#include "../stb/stb_image.h"
#include "../stb/stb_image_write.h"
#include "../stb/stb_image.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>


bool is_pink(unsigned char const *src) {
    return src[0] == 255 && src[1] == 128 && src[2] == 255;
}

bool is_blur_pixel(unsigned char const *src, int r, int c, int w, int h) {
    int npink = 0;
    int target = 0;
    for (int dr = -2; dr <= 2; ++dr) {
        if (dr + r < 0 || dr + r >= h) {
            continue;
        }
        for (int dc = -2; dc <= 2; ++dc) {
            if (dc + c < 0 || dc + c >= w) {
                continue;
            }
            ++target;
            if (is_pink(src + dc * 3 + dr * w * 3)) {
                ++npink;
            }
        }
    }
    return npink != target && npink != 0;
}

void put_blur_pixel(unsigned char const *src, int r, int c, int w, int h, unsigned char *d) {
    int target = 0;
    int red = 0;
    int green = 0;
    int blue = 0;
    for (int dr = -2; dr <= 2; ++dr) {
        if (dr + r < 0 || dr + r >= h) {
            continue;
        }
        for (int dc = -2; dc <= 2; ++dc) {
            if (dc + c < 0 || dc + c >= w) {
                continue;
            }
            ++target;
            unsigned char const *s = src + dr * w * 3 + dc * 3;
            red += s[0];
            green += s[1];
            blue += s[2];
        }
    }
    d[0] = red / target;
    d[1] = green / target;
    d[2] = blue / target;
}

int main(int argc, char const *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: blur inplace.png mask.png\n");
        fprintf(stderr, "blurs the inplace image at the edges of the mask, where \n");
        fprintf(stderr, "the mask is pink where to blur\n");
        exit(1);
    }
    if (!strstr(argv[1], ".png")) {
        fprintf(stderr, "%s: must end in .png\n", argv[1]);
        exit(1);
    }
    int x0, y0, n0;
    int x1, y1, n1;
    unsigned char *i0 = stbi_load(argv[1], &x0, &y0, &n0, 3);
    if (!i0) {
        fprintf(stderr, "%s: not found\n", argv[1]);
        exit(1);
    }
    unsigned char *i1 = stbi_load(argv[2], &x1, &y1, &n1, 3);
    if (!i1) {
        fprintf(stderr, "%s: not found\n", argv[2]);
        exit(1);
    }
    if (x0 != x1 || y0 != y1) {
        fprintf(stderr, "size %dx%d is different from %dx%d\n", x0, y0, x1, y1);
        exit(1);
    }
    size_t sz = x0 * y0 * 3;
    unsigned char *dst = (unsigned char *)malloc(sz);
    memset(dst, 0, sz);
    unsigned char const *s = i0;
    unsigned char const *m = i1;
    int nblur = 0;
    for (int r = 0; r != y1; ++r) {
        unsigned char *d = dst + x0 * 3 * r;
        for (int c = 0; c != x1; ++c) {
            if (is_blur_pixel(m, r, c, x1, y1)) {
                ++nblur;
                put_blur_pixel(s, r, c, x0, y0, d);
            } else {
                d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
            }
            d += 3;
            s += 3;
            m += 3;
        }
    }
    fprintf(stderr, "%s: %d blurred pixels\n", argv[1], nblur);
    char *path;
    if (asprintf(&path, "%s.tmp", argv[1]) < 0) {
        fprintf(stderr, "allocation failed\n");
        exit(2);
    }
    if (!stbi_write_png(path, x0, y0, 3, dst, 0)) {
        fprintf(stderr, "Could not write to %s\n", path);
        exit(1);
    }
    if (rename(path, argv[1]) < 0) {
        fprintf(stderr, "could not rename %s to %s\n", path, argv[1]);
        exit(1);
    }
    return 0;
}

