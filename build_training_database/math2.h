#if !defined(math2_h)
#define math2_h

float *m2_identity(float *dst);
float *m2_translate(float const *left, float dx, float dy, float *right);
float *m2_translation(float dx, float dy, float *dst);
float *m2_rotation(float dr, float *dst);
float *m2_mul(float const *left, float const *right, float *dst);
float *m2_transform_vec(float const *mat, float const *vec, float *dst);

#endif  //  math2_h
