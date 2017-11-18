#include "cone.h"
#include "../stb/stb_image.h"
#include "../stb/stb_image_write.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>



int main(int argc, char const *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: conefind input.png output.png\n");
        exit(1);
    }
    int x, y, n;
    unsigned char *img = stbi_load(argv[1], &x, &y, &n, 3);
    if (!img) {
        fprintf(stderr, "%s: could not load\n", argv[1]);
        exit(2);
    }
    int cx = 0;
    int cy = 0;
    int npx = find_orange_area(img, x, y, &cx, &cy, true);
    if (npx < 100) {
        fprintf(stderr, "%s: no cone\n", argv[1]);
        exit(3);
    }
    int w = (int)(sqrtf(npx)/1.5f);
    for (int i = -w; i <= w; ++i) {
        if (cx+i >= 0 && cx+i < x && cy+i >= 0 && cy+i < y) {
            img[(cy+i)*x*3 + (cx+i)*3] = 255;
            img[(cy+i)*x*3 + (cx+i)*3 + 1] = 255;
            img[(cy+i)*x*3 + (cx+i)*3 + 2] = 255;
        }
        if (cx+i >= 0 && cx+i < x && cy-i >= 0 && cy-i < y) {
            img[(cy-i)*x*3 + (cx+i)*3] = 255;
            img[(cy-i)*x*3 + (cx+i)*3 + 1] = 255;
            img[(cy-i)*x*3 + (cx+i)*3 + 2] = 255;
        }
    }
    fprintf(stderr, "%s: %d pixels at %d,%d\n", argv[2], npx, cx, cy);
    stbi_write_png(argv[2], x, y, 3, img, 0);
    return 0;
}
