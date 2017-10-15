#include "truetype.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb/stb_truetype.h"

#include <stdio.h>


#define FTEX_WIDTH 256
#define FTEX_HEIGHT 256

static unsigned char gray[FTEX_HEIGHT*FTEX_WIDTH];
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
    if (ttfsize != (long)fread(ttfbuf, 1, ttfsize, f)) {
        free(ttfbuf);
        fclose(f);
        return -1;
    }

    memset(gray, 0, sizeof(gray));
    stbtt_BakeFontBitmap((unsigned char *)ttfbuf, 0, 40.0f, gray, FTEX_WIDTH, FTEX_HEIGHT, 32, 96, chardata);
    free(ttfbuf);

    return 0;
}

int draw_truetype(char const *str, float *x, float *y, TTVertex *vert, int maxvert) {
    int ret = 0;
    float ydel = *y * 2;
    while (*str) {
        if (maxvert < 4) {
            break;
        }
        if (*str >= 32 && *str < 128) {
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(chardata, FTEX_WIDTH, FTEX_HEIGHT, *str-32, x, y, &q, 1);
            vert->u = q.s0; vert->v = q.t0; vert->x = q.x0; vert->y = ydel - q.y0; ++vert;
            vert->u = q.s1; vert->v = q.t0; vert->x = q.x1; vert->y = ydel - q.y0; ++vert;
            vert->u = q.s1; vert->v = q.t1; vert->x = q.x1; vert->y = ydel - q.y1; ++vert;
            vert->u = q.s0; vert->v = q.t1; vert->x = q.x0; vert->y = ydel - q.y1; ++vert;
            maxvert -= 4;
            ret += 4;
        }
        ++str;
    }
    return ret;
}

void get_truetype_bitmap(unsigned char const **odata, unsigned int *owidth, unsigned int *oheight) {
    *odata = gray;
    *owidth = FTEX_WIDTH;
    *oheight = FTEX_HEIGHT;
}

