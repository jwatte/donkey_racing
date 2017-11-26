#if !defined(glesgui_h)
#define glesgui_h

#include <unistd.h> /* for size_t */
#include <stdint.h>

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

void gg_get_viewport(int *owidth, int *oheight);

struct Program {
    /* uniforms locations */
    unsigned int vshader;
    unsigned int fshader;
    unsigned int program;
    unsigned int v_pos;
    unsigned int v_tex;
    unsigned int v_color;
    unsigned int g_transform;
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
    unsigned int dbl_texture;
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
void gg_update_texture(Texture *tex, unsigned int left, unsigned int width, unsigned int top, unsigned int height);
/* dispose an allocated texture */
void gg_clear_texture(Texture *tex);
/* load a texture on disk, and return texture pointer. Uses cache of loaded objects. Loaded 
 * textures are NOT modifiable at runtime.
 */
Texture const *gg_load_named_texture(char const *name, char *error, size_t esize);
/* blow away the loaded texture cache, invalidating all the textures loaded */
void gg_clear_named_textures();



struct Mesh {
    unsigned int vertexbuf;
    unsigned int dbl_vertexbuf;
    unsigned int vertexsize;
    unsigned int numvertices;
    unsigned int indexbuf;
    unsigned int dbl_indexbuf;
    unsigned int numindices;
    unsigned char desc_tex;
    unsigned char desc_color;
    unsigned char _desc_r;
    unsigned char flags;
};

#define MESH_FLAG_DYNAMIC 0x1
#define MESH_FLAG_COLOR_BYTES 0x2
#define MESH_FLAG_MAX 0x4

void gg_allocate_mesh(void const *mdata, unsigned int vertexbytes, unsigned int numvertices, unsigned short const *indices, unsigned int numindices, unsigned int tex_offset, unsigned int color_offset, Mesh *oMesh, unsigned int flags);
void gg_set_mesh(Mesh *mesh, void const *mdata, unsigned int numvertices, unsigned short const *indices, unsigned int numindices);
void gg_clear_mesh(Mesh *mesh);
Mesh const *gg_load_named_mesh(char const *name, char *error, size_t esize);
void gg_clear_named_meshes();

enum PrimitiveKind {
    PK_Triangles,
    PK_Lines
};
struct MeshDrawOp {
    Program const *program;
    Texture const *texture;
    Mesh const *mesh;
    float const *transform;
    float color[4];
    PrimitiveKind primitive;
};
void gg_set_program_transform(Program const *program, float const *transform);
void gg_set_named_program_transforms(float const *transform);
void gg_draw_mesh(MeshDrawOp const *draw);
void gg_draw_text(float x, float y, float size, char const *text, uint32_t color);
void gg_draw_box(float left, float bottom, float right, float top, uint32_t color);
void gg_draw_line(float x1, float y1, float x2, float y2, uint32_t color);
void gg_get_gui_transform(float *oMatrix);
float const *gg_gui_transform();
void gg_get_gl_errors(void (*func)(char const *error, void *cookie), void *cookie);
void gg_break_gl_error(void (*func)(char const *error, void *cookie), void *cookie);

void gg_init_color(float *d, float r, float g, float b, float a);

#if defined(__cplusplus)
namespace color {
    extern uint32_t const white;
    extern uint32_t const midgray;
    extern uint32_t const black;

    extern uint32_t const textgray;
    extern uint32_t const textyellow;
    extern uint32_t const textblue;
    extern uint32_t const textred;
    extern uint32_t const textgreen;

    extern uint32_t const bggray;
    extern uint32_t const bgyellow;
    extern uint32_t const bgblue;
    extern uint32_t const bgred;
    extern uint32_t const bggreen;
}

}
#endif

#endif  //  glesgui_h

