#include "truetype.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb/stb_truetype.h"

#include <stdio.h>


static unsigned char gray[1024*1024];
static stbtt_bakedchar chardata[96];

int init_truetype(char const *path) {

    long ttfsize = 0;
    char *ttfbuf = NULL;

    FILE *f = fopen(path, "rb");

    if (!f) {
        return -1;
    }
    fseek(f, 0, 2);
    ttfsize = ftell(f);
    rewind(f);

    ttfbuf = malloc(ttfsize);
    if (ttfsize != fread(ttfbuf, 1, ttfsize, f)) {
        free(ttfbuf);
        fclose(f);
        return -1;
    }

    memset(gray, 0, sizeof(gray));
    stbtt_BakeFontBitmap((unsigned char *)ttfbuf, 0, 40.0f, gray, 1024, 1024, 32, 96, chardata);
    free(ttfbuf);

    return 0;
}

int draw_truetype(char const *str, float *x, float *y, TTVertex *vert, int maxvert) {
    int ret = 0;
    while (*str) {
        if (maxvert < 4) {
            break;
        }
        if (*str >= 32 && *str < 128) {
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(chardata, 1024, 1024, *str-32, x, y, &q, 1);
            vert->u = q.s0; vert->v = q.t1; vert->x = q.x0; vert->y = q.y0; ++vert;
            vert->u = q.s1; vert->v = q.t1; vert->x = q.x1; vert->y = q.y0; ++vert;
            vert->u = q.s1; vert->v = q.t0; vert->x = q.x1; vert->y = q.y1; ++vert;
            vert->u = q.s0; vert->v = q.t0; vert->x = q.x0; vert->y = q.y1; ++vert;
            maxvert -= 4;
        }
        ++str;
    }
    return ret;
}

void get_truetype_bitmap(unsigned char const **odata, unsigned int *owidth, unsigned int *oheight) {
    *odata = gray;
    *owidth = 1024;
    *oheight = 1024;
}

