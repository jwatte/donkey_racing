#if !defined(YUV_H)
#define YUV_H

#if !defined(YUV_EXTERN) 
 #if defined(__cplusplus)
  #define YUV_EXTERN extern "C"
 #else
  #define YUV_EXTERN
 #endif
#endif

YUV_EXTERN void yuv_to_rgb(unsigned char const *yuv, unsigned char *rgb, int xs, int ys);
//  u and v are in 128-is-center space
YUV_EXTERN void yuv_comp_to_rgb(unsigned char y, unsigned char u, unsigned char v, unsigned char *rgb);
YUV_EXTERN void rgb_to_yuv(unsigned char *oot, unsigned char const *d, int xs, int ys);

#endif  //  YUV_H
