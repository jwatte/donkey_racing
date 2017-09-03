#if !defined(iamge_h)
#define image_h

#include <stdint.h>
#include <stdlib.h>

/* what size image will I get out? the image will be in multi-plane float format */
void get_unwarp_info(size_t *osize, int *owidth, int *oheight, int *oplanes);
/* actually unwarp an input image in I420 format, into multi-plane float format */
void unwarp_image(void const *src, void *dst);

#endif  //  image_h

