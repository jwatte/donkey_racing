#if !defined(iamge_h)
#define image_h

#include <stdint.h>
#include <stdlib.h>

/* what size image will I get out? the image will be in multi-plane float format */
void get_unwarp_info(size_t *osize, int *owidth, int *oheight, int *oplanes);
/* actually unwarp an input image in I420 format, into multi-plane float format */
void unwarp_image(void const *src, void *dst);
/* unwarp an image, using an additional transform matrix (slower) */
void unwarp_transformed_bytes(void const *y, void const *u, void const *v, float const *mat3x2, unsigned char *dst);
void unwarp_transformed_rgb(void const *y, void const *u, void const *v, float const *mat3x2, unsigned char *dst);

#endif  //  image_h

