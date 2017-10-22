
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


void print_rgb(int q) {
    float r = ((q >>16) & 0xff);
    float g = ((q >> 8) & 0xff);
    float b = (q & 0xff);
    float y = r * 0.299f + g * 0.587f + b * 0.114f;
    float u = 0.492f * (b - y);
    float v = 0.492f * (r - y);
    fprintf(stdout, "%.1f %.1f %.1f\n", y, u, v);
}

int main(int argc, char const *argv[]) {
    if (argc == 2 && strlen(argv[1]) == 6) {
        char *s;
        int r = strtol(argv[1], &s, 16);
        print_rgb(r);
        return 0;
    }
    if (argc == 4) {
        int r = atoi(argv[1]);
        int g = atoi(argv[2]);
        int b = atoi(argv[3]);
        print_rgb((r << 16) | (g << 8) | b);
        return 0;
    }
    fprintf(stderr, "usage: c_rgb_yuv rrggbb | c_rgb_yuv r g b\n");
    return 1;
}


