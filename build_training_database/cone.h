#if !defined(cone_h)
#define cone_h

int fixup_cone_labels(unsigned char *outputrgb, int width, int height, bool print, int serial, float *label);
int find_orange_area(unsigned char *rgb, int width, int height, int *cx, int *cy, bool mark);

#endif
