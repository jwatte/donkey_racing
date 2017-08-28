#if !defined(glesgui_h)
#define glesgui_h

#include <unistd.h> /* for size_t */

#if defined(__cplusplus)
extern "C" {
#endif

/* initialize and take over the screen */
int gg_setup(unsigned int width, unsigned int height, int left, int top, char const *name);
/* buttons is a bitmask of buttons currently down */
void gg_onmousemove(void (*mousemovefn)(int x, int y, unsigned int buttons));
/* button is 0 for left, 1 for right, 2 for middle */
void gg_onmousebutton(void (*mousebuttonfn)(int x, int y, int button, int down));
/* do your GL thing */
void gg_ondraw(void (*drawfn)());
/* do whatever processing you want */
void gg_run(void (*idlefn)());
/* set the quit flag so gg_run returns */
void gg_set_quit_flag();

struct Program {
    unsigned int vshader;
    unsigned int fshader;
    unsigned int program;
    unsigned int v_pos;
    unsigned int v_tex;
    unsigned int v_color;
    unsigned int g_color;
    unsigned int g_tex;
};

/* load program from memory, return < 0 on error */
int gg_compile_program(char const *vshader, char const *pshader, Program *oProg, char *error, size_t esize);
/* clear a particular compiled program */
void gg_clear_program(Program *iProc);
/* load program from data directory, return NULL on error */
Program const *gg_compile_named_program(char const *name, char *error, size_t esize);
/* recompile a particular program */
Program const *gg_recompile_named_program(char const *name, char *error, size_t esize);
/* clear all loaded programs */
void gg_clear_named_programs();

struct Texture {
    unsigned int texture;
    unsigned int miplevels;
    unsigned int width;
    unsigned int height;
    unsigned int format;
    void **mipdata;
};

/* Given some bits in RAM, allocate a texture (perhaps generating mipmaps) and return it.
 * The data, if present, is copied into the allocated texture. Else it's allocated as 
 * all black. Texture size is rounded up to power of two in each direction. The texture
 * is assumed to be in BGRA (8 bits per component) format in RAM.
 */
void gg_allocate_texture(void const *data, unsigned int width, unsigned int height, unsigned int mipmaps, unsigned int format, Texture *oTex);
/* given texture data for the top mipmap, update the texture images */
void gg_update_texture(Texture *tex, unsigned int rowbytes, unsigned int left, unsigned int width, unsigned int top, unsigned int height);
/* dispose an allocated texture */
void gg_clear_texture(Texture *tex);
/* load a texture on disk, and return texture pointer. Uses cache of loaded objects. Loaded 
 * textures are NOT modifiable at runtime.
 */
Texture const *gg_load_named_texture(char const *name, char *error, size_t esize);
/* blow away the loaded texture cache, invalidating all the textures loaded */
void gg_clear_named_textures();

#if defined(__cplusplus)
}
#endif

#endif  //  glesgui_h

