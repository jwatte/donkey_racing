#if !defined(truetype_h)
#define truetype_h

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct TTVertex {
    float x;
    float y;
    float u;
    float v;
    uint32_t c;
} TTVertex;

/* Load a given font into a bitmap. Note: character data is stored in statics, 
 * so only one font can be loaded at a time!
 */
int init_truetype(char const *path);
/* Return the bitmap data of a loaded font. */
void get_truetype_bitmap(unsigned char const **odata, unsigned int *owidth, unsigned int *oheight);
/* Render to some vertex buffer. Return number of vertices actually consumed. */
int draw_truetype(char const *str, float *x, float *y, TTVertex *vert, int maxvert);

#if defined(__cplusplus)
}
#endif

#endif  //  truetype_h
