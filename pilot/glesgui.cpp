#include "glesgui.h"
#include "truetype.h"
#include "../stb/stb_image.h"
#include "../stb/stb_image_write.h"
#include "metrics.h"
#include "lapwatch.h"

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>

#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <set>

#if defined(__arm__)
#include "bcm_host.h"

#include "GLES2/gl2.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"

#define check() _check_gl_error(__FILE__, __LINE__, __PRETTY_FUNCTION__)

#define READMOUSE 0
#define READX 1

#define GL_CHECK_ERRORS 0

#if READX
#include <X11/Xlib.h>
#endif

//  Define this to convert all texture formats to RGBA 
//  to work around bugs.
// #define ALWAYS_RGBA 1

//  Define this to always use glTexImage2D() even when 
//  glTexSubImage2D() would do.
// #define ALWAYS_TEXIMAGE 1

//  Define this to always use the highest MIP level
#define ALWAYS_MIPLEVEL_0 1

struct Context {
    uint32_t screen_width;
    uint32_t screen_height;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;

    EGL_DISPMANX_WINDOW_T nativewindow;

#if READMOUSE
    int mousefd;
#endif
#if READX
    Display *xdisplay;
    Window rootwindow;
#endif
    unsigned int mousebuttons;
    unsigned int mousex;
    unsigned int mousey;
    unsigned int prevbuttons;
    unsigned int prevx;
    unsigned int prevy;
};

static Context gCtx;
static char tmplog[1024];
static std::map<std::string, Program> gNamedPrograms;
static void (*cb_mousemove)(int, int, unsigned int);
static void (*cb_mousebutton)(int, int, int, int);
static void (*cb_draw)();
static Texture fontTexture;
static Mesh fontMesh;
static Program const *fontProgram;
static Mesh boxMesh;
static Program const *boxProgram;
static Mesh lineMesh;
static float guiTransform[16];
static std::map<std::string, Texture> gNamedTextures;
static std::map<std::string, Mesh> gNamedMeshes;
static unsigned int boundProgram = (unsigned int)-1;
static unsigned int boundTexture = (unsigned int)-1;
static bool g_running;
static void (*gl_error_break)(char const *, void *);
static void *gl_error_cookie;
static std::set<std::string> errors;




char const *gl_error_str(GLuint ui) {
    switch (ui) {
        case GL_NO_ERROR: return "GL_NO_ERROR";
        case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
        case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
        default: return "???";
    }
}

static bool _check_gl_error(char const *file, int line, char const *function) {
#if CHECK_GL_ERRORS
    GLuint err = glGetError();
    bool ok = true;
    while (err != 0) {
        Graphics_GlError.increment();
        char buf[1024];
        snprintf(buf, 1024, "%s:%d: %s: %s (0x%x)", file, line, function, gl_error_str(err), err);
        buf[1023] = 0;
        std::string s(buf);
        bool is_new = errors.insert(s).second;
        if (gl_error_break) {
            gl_error_break(buf, gl_error_cookie);
        } else {
            fprintf(stderr, "%s\n", buf);
            usleep(is_new ? 500000 : 50000);
        }
        ok = false;
        err = glGetError();
    }
    return ok;
#else
    return true;
#endif
}

void gg_get_gl_errors(void (*func)(char const *err, void *cookie), void *cookie) {
    for (auto const &str : errors) {
        func(str.c_str(), cookie);
    }
}

void gg_break_gl_error(void (*func)(char const *error, void *cookie), void *cookie) {
    gl_error_break = func;
    gl_error_cookie = cookie;
}


#if READX
static int x_error(Display *, XErrorEvent *e) {
    fprintf(stderr, "x_error: display error: type %d; error_code %d\n",
        e->type, e->error_code);
    exit(1);
}
#endif

static bool init_ogl(Context *ctx, unsigned int width, unsigned int height) {

    bcm_host_init();

#if ALWAYS_MIPLEVEL_0
    Graphics_AlwaysMiplevel0.set();
#endif
#if ALWAYS_TEXIMAGE
    Graphics_AlwaysTeximage.set();
#endif
#if ALWAYS_RGBA
    Graphics_AlwaysRgba.set();
#endif

    memset(ctx, 0, sizeof(*ctx));
#if READMOUSE
    ctx->mousefd = -1;
#endif
#if READX
    ctx->xdisplay = nullptr;
    memset(&ctx->rootwindow, 0, sizeof(ctx->rootwindow));
#endif
    ctx->screen_width = width;
    ctx->screen_height = height;

    if (init_truetype("/usr/share/fonts/truetype/roboto/hinted/Roboto-Regular.ttf") < 0) {
        fprintf(stderr, "Could not load font\n");
        return false;
    }

    int32_t success = 0;
    EGLBoolean result;
    EGLint num_config;

    DISPMANX_ELEMENT_HANDLE_T dispman_element;
    DISPMANX_DISPLAY_HANDLE_T dispman_display;
    DISPMANX_UPDATE_HANDLE_T dispman_update;
    VC_RECT_T dst_rect;
    VC_RECT_T src_rect;

    static const EGLint attribute_list[] =
    {
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_NONE
    };

    static const EGLint context_attributes[] = 
    {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
    };
    EGLConfig config;

    // get an EGL display connection
    ctx->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    assert(ctx->display!=EGL_NO_DISPLAY);
    check();

    // initialize the EGL display connection
    result = eglInitialize(ctx->display, NULL, NULL);
    assert(EGL_FALSE != result);
    check();

    // get an appropriate EGL frame buffer configuration
    result = eglChooseConfig(ctx->display, attribute_list, &config, 1, &num_config);
    assert(EGL_FALSE != result);
    check();

    // get an appropriate EGL frame buffer configuration
    result = eglBindAPI(EGL_OPENGL_ES_API);
    assert(EGL_FALSE != result);
    check();

    // create an EGL rendering context
    ctx->context = eglCreateContext(ctx->display, config, EGL_NO_CONTEXT, context_attributes);
    assert(ctx->context!=EGL_NO_CONTEXT);
    check();

    // create an EGL window surface
    success = graphics_get_display_size(0 /* LCD */, &ctx->screen_width, &ctx->screen_height);
    assert( success >= 0 );

    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.width = ctx->screen_width;
    dst_rect.height = ctx->screen_height;
      
    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = ctx->screen_width << 16;
    src_rect.height = ctx->screen_height << 16;        

    dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
    dispman_update = vc_dispmanx_update_start( 0 );
         
    dispman_element = vc_dispmanx_element_add ( dispman_update, dispman_display,
      0/*layer*/, &dst_rect, 0/*src*/,
      &src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 0/*clamp*/, (DISPMANX_TRANSFORM_T)0 /*transform*/);
      
    ctx->nativewindow.element = dispman_element;
    ctx->nativewindow.width = ctx->screen_width;
    ctx->nativewindow.height = ctx->screen_height;
    vc_dispmanx_update_submit_sync( dispman_update );
      
    check();

    ctx->surface = eglCreateWindowSurface( ctx->display, config, &ctx->nativewindow, NULL );
    assert(ctx->surface != EGL_NO_SURFACE);
    check();

    // connect the context to the surface
    result = eglMakeCurrent(ctx->display, ctx->surface, ctx->surface, ctx->context);
    assert(EGL_FALSE != result);
    check();

    // Set background color and clear buffers
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    check();

#if READMOUSE
    ctx->mousefd = ::open("/dev/input/mice", O_RDONLY | O_NONBLOCK);
    if (ctx->mousefd < 0) {
        perror("/dev/input/mice");
        return false;
    }
#endif
#if READX
    ctx->xdisplay = XOpenDisplay("localhost:0.0");
    if (!ctx->xdisplay) {
        ctx->xdisplay = XOpenDisplay(":0");
    }
    if (!ctx->xdisplay) {
        perror("XOpenDisplay()");
        return false;
    }
    XSetErrorHandler(x_error);
    ctx->rootwindow = XRootWindow(ctx->xdisplay, 0);
#endif
    ctx->mousex = width / 2;
    ctx->mousey = height / 2;

    unsigned char const *data;
    unsigned int bmwidth;
    unsigned int bmheight;
    get_truetype_bitmap(&data, &bmwidth, &bmheight);
    gg_allocate_texture(data, bmwidth, bmheight, 0, 1, &fontTexture);
    check();
    gg_allocate_mesh(NULL, 20, 0, NULL, 0, 8, 16, &fontMesh, MESH_FLAG_DYNAMIC | MESH_FLAG_COLOR_BYTES);
    check();
    gg_get_gui_transform(guiTransform);
    char err[128];
    if (!(fontProgram = gg_compile_named_program("gui-color-texture", err, 128))) {
        fprintf(stderr, "Can't load gui-color-texture program: %s\n", err);
        return false;
    }
    check();
    gg_allocate_mesh(NULL, 12, 0, NULL, 0, 0, 8, &boxMesh, MESH_FLAG_DYNAMIC | MESH_FLAG_COLOR_BYTES);
    gg_allocate_mesh(NULL, 12, 0, NULL, 0, 0, 8, &lineMesh, MESH_FLAG_DYNAMIC | MESH_FLAG_COLOR_BYTES);
    if (!(boxProgram = gg_compile_named_program("gui-color", err, 128))) {
        fprintf(stderr, "Can't load gui-color program: %s\n", err);
        return false;
    }
    check();

    return true;
}


int gg_compile_program(char const *vshader, char const *fshader, Program *oProg, char *error, size_t esize) {
    
    memset(oProg, 0, sizeof(*oProg));
    *error = 0;

    oProg->vshader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(oProg->vshader, 1, (GLchar const **)&vshader, 0);
    glCompileShader(oProg->vshader);
    GLuint err = glGetError();
    GLint status = 0;
    GLsizei length = sizeof(tmplog);
    glGetShaderiv(oProg->vshader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE || err != GL_NO_ERROR) {
        glGetShaderInfoLog(oProg->vshader, sizeof(tmplog), &length, tmplog);
        snprintf(error, esize, "vertex shader error: (%x) %.*s", err, (int)length, tmplog);
        return -1;
    }

    oProg->fshader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(oProg->fshader, 1, (GLchar const **)&fshader, 0);
    glCompileShader(oProg->fshader);
    err = glGetError();
    status = 0;
    glGetShaderiv(oProg->fshader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE || err != GL_NO_ERROR) {
        glGetShaderInfoLog(oProg->fshader, sizeof(tmplog), &length, tmplog);
        snprintf(error, esize, "fragment shader error: (%x) %.*s", err, (int)length, tmplog);
        return -1;
    }

    oProg->program = glCreateProgram();
    glAttachShader(oProg->program, oProg->vshader);
    glAttachShader(oProg->program, oProg->fshader);
    glBindAttribLocation(oProg->program, 0, "v_pos");
    glBindAttribLocation(oProg->program, 1, "v_tex");
    glBindAttribLocation(oProg->program, 2, "v_color");
    glLinkProgram(oProg->program);
    err = glGetError();
    status = 0;
    glGetProgramiv(oProg->program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE || err != GL_NO_ERROR) {
        glGetProgramInfoLog(oProg->program, sizeof(tmplog), &length, tmplog);
        snprintf(error, esize, "program error: (%x) %.*s", err, (int)length, tmplog);
        return -1;
    }
    glUseProgram(oProg->program);
    boundProgram = oProg->program;

    oProg->v_pos = glGetAttribLocation(oProg->program, "v_pos");
    oProg->v_tex = glGetAttribLocation(oProg->program, "v_tex");
    oProg->v_color = glGetAttribLocation(oProg->program, "v_color");
    oProg->g_transform = glGetUniformLocation(oProg->program, "g_transform");
    oProg->g_color = glGetUniformLocation(oProg->program, "g_color");
    oProg->g_tex = glGetUniformLocation(oProg->program, "g_tex");
    check();
    if (oProg->g_tex != (unsigned int)-1) {
        glUniform1i(oProg->g_tex, 0);
        check();
    }

    return oProg->program;
}

void gg_clear_program(Program *iProc) {
    assert(iProc->program);
    glDeleteProgram(iProc->program);
    glDeleteShader(iProc->vshader);
    glDeleteShader(iProc->fshader);
    memset(iProc, 0, sizeof(*iProc));
    boundProgram = (unsigned int)-1;
}


Program const * gg_compile_named_program(char const *name, char *error, size_t esize) {
    std::string nn(name);
    auto ptr(gNamedPrograms.find(nn));
    if (ptr != gNamedPrograms.end() && (*ptr).second.program > 0) {
        *error = 0;
        return &(*ptr).second;
    }
    return gg_recompile_named_program(name, error, esize);
}

static bool load_text(std::string const &name, std::string *data) {
    FILE *f = fopen(name.c_str(), "rb");
    if (!f) {
        return false;
    }
    fseek(f, 0, 2);
    std::vector<char> buf;
    buf.resize(ftell(f) + 1);
    rewind(f);
    if (fread(&buf[0], 1, buf.size()-1, f) != buf.size()-1) {
        fclose(f);
        return false;
    }
    fclose(f);
    data->assign(&buf[0]);
    return true;
}

Program const * gg_recompile_named_program(char const *name, char *error, size_t esize) {
    std::string nn(name);
    std::string vname(std::string("data/") + nn + ".vertex.glsl");
    std::string fname(std::string("data/") + nn + ".fragment.glsl");
    std::string vtext;
    std::string ftext;
    if (!load_text(vname, &vtext)) {
        snprintf(error, esize, "vertex shader: could not load %s", vname.c_str());
        return NULL;
    }
    if (!load_text(fname, &ftext)) {
        snprintf(error, esize, "fragment shader: could not load %s", fname.c_str());
        return NULL;
    }
    auto ptr(gNamedPrograms.find(nn));
    if (ptr != gNamedPrograms.end()) {
        gg_clear_program(&(*ptr).second);
    }
    Program prog;
    int i = gg_compile_program(vtext.c_str(), ftext.c_str(), &prog, error, esize);
    if (i >= 0) {
        gNamedPrograms[nn] = prog;
        return &gNamedPrograms[nn];
    }
    return NULL;
}

void gg_clear_named_programs() {
    for (auto &ptr : gNamedPrograms) {
        gg_clear_program(&ptr.second);
    }
}


void gg_allocate_texture(void const *data, unsigned int width, unsigned int height, unsigned int mipmaps, unsigned int format, Texture *oTex) {

    if (format != 1 && format != 3 && format != 4) {
        assert(!"unknown format in allocate_texture");
        return;
    }

    /* calculate rounded-up power-of-two sizes, and mip levels */
    unsigned int w = 4;
    while (w < width) {
        w <<= 1;
    }
    unsigned int h = 4;
    while (h < height) {
        h <<= 1;
    }
    unsigned int d = 0;
    for (unsigned int ww = w, hh = h; ww > 1 || hh > 1; ww >>= 1, hh >>= 1) {
        ++d;
    }
    if (mipmaps > d || mipmaps == 0) {
        mipmaps = d;
    } else {
        d = mipmaps;
    }
    oTex->width = w;
    oTex->height = h;
#if ALWAYS_MIPLEVEL_0
    d = mipmaps = 1;
#endif
    oTex->miplevels = d;
    oTex->format = format;
    oTex->mipdata = NULL;

    /* allocate VRAM */
    glGenTextures(1, &oTex->texture);
    glBindTexture(GL_TEXTURE_2D, oTex->texture);
    boundTexture = (unsigned int)-1;
#if ALWAYS_RGBA
    unsigned int iformat = GL_RGBA;
    unsigned int xformat = GL_RGBA;
#else
    unsigned int iformat = format == 1 ? GL_ALPHA : format == 3 ? GL_RGB : GL_RGBA;
    unsigned int xformat = format == 1 ? GL_ALPHA : format == 3 ? GL_RGB : GL_RGBA;
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, iformat, w, h, 0, xformat, GL_UNSIGNED_BYTE, NULL);
    if (!check()) {
        fprintf(stderr, "glTexImage2D(): iformat=%d, w=%d, h=%d, xformat=%d\n", iformat, w, h, xformat);
    }
    while (d > 1) {
        d--;
        w >>= 1;
        h >>= 1;
        glTexImage2D(GL_TEXTURE_2D, oTex->miplevels - d, iformat, w, h, 0, xformat, GL_UNSIGNED_BYTE, NULL);
        if (!check()) {
            fprintf(stderr, "glTexImage2D(): mip=%d, iformat=%d, w=%d, h=%d, xformat=%d\n",
                    oTex->miplevels-d, iformat, w, h, xformat);
        }
    }

    /* set filtering parameters */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, oTex->miplevels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    check();

    /* allocate storage */
    size_t needed = oTex->miplevels * sizeof(void *);
    unsigned int wh = oTex->width * oTex->height;
    for (unsigned int m = 0; m != oTex->miplevels; ++m) {
        needed += wh * format;
        wh >>= 2;
    }
    oTex->mipdata = (void **)calloc(needed / 4 + 1, 4);
    needed = oTex->miplevels * sizeof(void *);
    wh = oTex->width * oTex->height;
    void *base = oTex->mipdata + oTex->miplevels;
    for (unsigned int m = 0; m != oTex->miplevels; ++m) {
        oTex->mipdata[m] = base;
        base = (unsigned char *)base + wh * format;
        wh >>= 2;
    }

    /* fill in image, if present */
    if (data) {
        if (data != oTex->mipdata[0]) {
            if (width != oTex->width) {
                for (size_t r = 0; r != oTex->height; ++r) {
                    memcpy((char *)oTex->mipdata[0] + r * oTex->width * format,
                            (char *)data + r * width * format,
                            width * format);
                }
            } else {
                memcpy(oTex->mipdata[0], data, oTex->width * height * format);
            }
        }
        gg_update_texture(oTex, 0, width, 0, height);
    }
}

/* Box filter a given subset of an image to the affected lower-level pixels.
 * Assume linear color space.
 */
static void mip_filter(Texture *tex, unsigned int ml, unsigned int left, unsigned int top, unsigned int width, unsigned int height) {
    assert(ml+1 < tex->miplevels);
    unsigned char *dest = (unsigned char *)tex->mipdata[ml+1];
    unsigned char const *src = (unsigned char *)tex->mipdata[ml];
    unsigned int l = (left >> 1);
    unsigned int t = (top >> 1);
    unsigned int w = ((width + 1) >> 1);
    unsigned int h = ((height + 1) >> 1);
    unsigned int src_rb = (tex->width >> ml) * tex->format;
    unsigned int dst_rb = src_rb >> 1;
    unsigned int ncomp = tex->format;
    dest += l * tex->format;
    dest += t * dst_rb;
    src += l * tex->format * 2;
    src += t * 2 * src_rb;
    for (unsigned int prow = t; prow < t + h; ++prow) {
        unsigned char const *s = src;
        unsigned char *d = dest;
        for (unsigned int pcol = l; pcol < l + w; ++pcol) {
            for (unsigned int comp = 0; comp < ncomp; ++comp) {
                /* todo: should probably sRGB here -- at least assume a 
                 * gamma power 2, which is quick and cheap to calculate.
                 */
                unsigned int v = (s[0] + s[ncomp] + s[src_rb] + s[src_rb + ncomp]);
                v = (v + 2) >> 2;
                d[0] = (unsigned char)v;
                ++s;
                ++d;
            }
        }
        dest += dst_rb;
        src += src_rb * 2;
    }
}


#if ALWAYS_RGBA
static size_t cu_bits_size;
static void *cu_bits;

static unsigned int get_pxsz(GLuint xformat) {
    switch (xformat) {
        case GL_LUMINANCE:
        case GL_ALPHA:
        case 1:
            return 1;
        case GL_RGB:
        case 3:
            return 3;
        case GL_RGBA:
        case 4:
            return 4;
        default:
            assert(!"bad pixel format");
            return 1;
    }
}

static unsigned char get_r(unsigned char const *p, unsigned int pixel_size) {
    return pixel_size == 1 ? 255 : p[0];
}

static unsigned char get_g(unsigned char const *p, unsigned int pixel_size) {
    return pixel_size == 1 ? 255 : p[1];
}

static unsigned char get_b(unsigned char const *p, unsigned int pixel_size) {
    return pixel_size == 1 ? 255 : p[2];
}

static unsigned char get_a(unsigned char const *p, unsigned int pixel_size) {
    return pixel_size == 4 ? p[3] : pixel_size == 1 ? p[0] : 255;
}

static void gg_convert_upload(int miplevel, int left, int top, int width, int height, GLuint xformat, void const *idata) {
    if (xformat != GL_RGBA && xformat != 4) {
        unsigned int pixel_size = get_pxsz(xformat);
        if (cu_bits_size < (size_t)(width * height * 4u)) {
            cu_bits = ::realloc(cu_bits, width * height * 4);
            if (!cu_bits) {
                perror("\nOut of memory in cu_bits realloc()");
                exit(1);
            }
            cu_bits_size = width * height * 4;
        }
        unsigned char *d = (unsigned char *)cu_bits;
        unsigned char const *i = (unsigned char const *)idata;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                d[0] = get_r(i, pixel_size);
                d[1] = get_g(i, pixel_size);
                d[2] = get_b(i, pixel_size);
                d[3] = get_a(i, pixel_size);
                d += 4;
                i += pixel_size;
            }
        }
        idata = cu_bits;
    }
#if ALWAYS_TEXIMAGE
    glTexImage2D(GL_TEXTURE_2D, miplevel, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, idata);
#else
    glTexSubImage2D(GL_TEXTURE_2D, miplevel, left, top, width, height, GL_RGBA, GL_UNSIGNED_BYTE, idata);
#endif
}
#endif

void gg_update_texture(Texture *tex, unsigned int left, unsigned int width, unsigned int top, unsigned int height) {
    assert(tex->mipdata);
    glBindTexture(GL_TEXTURE_2D, tex->texture);
    boundTexture = (unsigned int)-1;
    unsigned int xformat = tex->format == 1 ? GL_ALPHA : tex->format == 3 ? GL_RGB : GL_RGBA;
    left = 0;
    top = 0;
    width = tex->width;
    height = tex->height;
    for (unsigned int i = 0; i < tex->miplevels; ++i) {
#if ALWAYS_RGBA
        gg_convert_upload(i, left, top, width, height, xformat, tex->mipdata[i]);
#else
#if ALWAYS_TEXIMAGE
        glTexImage2D(GL_TEXTURE_2D, i, xformat, width, height, 0, xformat, GL_UNSIGNED_BYTE, tex->mipdata[i]);
#else
        glTexSubImage2D(GL_TEXTURE_2D, i, left, top, width, height, xformat, GL_UNSIGNED_BYTE, tex->mipdata[i]);
#endif
#endif
        check();
        if (i + 1 < tex->miplevels) {
            mip_filter(tex, i, left, top, width, height);
        }
        left >>= 1;
        top >>= 1;
        width = (width + 1) >> 1;
        height = (height + 1) >> 1;
    }
    check();
}

void gg_clear_texture(Texture *tex) {
    free(tex->mipdata);
    glDeleteTextures(1, &tex->texture);
    check();
    boundTexture = (unsigned int)-1;
}


static char const *extn[3] = {
    ".tga",
    ".png",
    ".jpg"
};

Texture const *gg_load_named_texture(char const *name, char *error, size_t esize) {
    *error = 0;
    auto const &ptr(gNamedTextures.find(name));
    if (ptr != gNamedTextures.end()) {
        return &(*ptr).second;
    }
    for (int i = 0; i != 3; ++i) {
        std::string fn("data/");
        fn += name;
        fn += extn[i];
        int x, y, n;
        unsigned char *data = stbi_load(fn.c_str(), &x, &y, &n, 0);
        if (!data) {
            continue;
        }
        Texture *t = &gNamedTextures[name];
        gg_allocate_texture(data, x, y, 0, n, t);
        stbi_image_free(data);
        return t;
    }
    return nullptr;
}

void gg_clear_named_textures() {
    for (auto &p : gNamedTextures) {
        gg_clear_texture(&p.second);
    }
    gNamedTextures.clear();
}


/* Meshes

struct Mesh {
    unsigned int vertexbuf;
    unsigned int vertexsize;
    unsigned int numvertices;
    unsigned int indexbuf;
    unsigned int numindices;
    unsigned char desc_tex;
    unsigned char desc_color;
    unsigned char desc_normal;
    unsigned char flags;
};

struct MeshDrawOp {
    Program const *program;
    Texture const *texture;
    Mesh const *mesh;
    float offset[2];
    float color[4];
};

#define MESH_FLAG_DYNAMIC 0x1
#define MESH_FLAG_COLOR_BYTES 0x2

*/

void gg_allocate_mesh(void const *mdata, unsigned int vertexbytes, unsigned int numvertices, unsigned short const *indices, unsigned int numindices, unsigned int tex_offset, unsigned int color_offset, Mesh *oMesh, unsigned int flags) {
    check();
    memset(oMesh, 0, sizeof(*oMesh));
    oMesh->vertexsize = vertexbytes;
    oMesh->numvertices = 0;
    oMesh->numindices = 0;
    oMesh->desc_tex = tex_offset;
    oMesh->desc_color = color_offset;
    oMesh->flags = flags;
    glGenBuffers(1, &oMesh->vertexbuf);
    glGenBuffers(1, &oMesh->indexbuf);
    check();
    gg_set_mesh(oMesh, mdata, numvertices, indices, numindices);
}

void gg_set_mesh(Mesh *mesh, void const *mdata, unsigned int numvertices, unsigned short const *indices, unsigned int numindices) {
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vertexbuf);
    if (numvertices > mesh->numvertices || !(mesh->flags & MESH_FLAG_DYNAMIC) || !numvertices) {
        mesh->numvertices = numvertices;
        glBufferData(GL_ARRAY_BUFFER, mesh->vertexsize * (GLuint)mesh->numvertices, NULL, (mesh->flags & MESH_FLAG_DYNAMIC) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
        check();
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, mesh->vertexsize * (GLuint)numvertices, mdata);
    check();
    if (numindices) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->indexbuf);
        if (numindices > mesh->numindices || !(mesh->flags & MESH_FLAG_DYNAMIC) || !numindices) {
            mesh->numindices = numindices;
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->numindices * (GLuint)2, NULL, (mesh->flags & MESH_FLAG_DYNAMIC) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
            check();
        }
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, 2 * (GLuint)numindices, indices);
    } else {
        mesh->numindices = 0;
    }
    check();
}

void gg_clear_mesh(Mesh *mesh) {
    glDeleteBuffers(1, &mesh->vertexbuf);
    glDeleteBuffers(1, &mesh->indexbuf);
    check();
    memset(mesh, 0, sizeof(*mesh));
}

template<typename T> void put(T t, unsigned int offset, void *data) {
    memcpy((char *)data + offset, &t, sizeof(t));
}

static bool parse_mesh(std::string const &mdata, Mesh *mesh) {
    std::istringstream ss(mdata);
    unsigned int version = 0;
    ss >> version;
    if (version != 1) {
        fprintf(stderr, "Bad mesh version %u\n", version);
        return false;
    }
    unsigned int flags = 0;
    ss >> flags;
    if (flags >= MESH_FLAG_MAX) {
        fprintf(stderr, "Bad mesh flags %u\n", flags);
        return false;
    }
    unsigned int vsize = 0;
    ss >> vsize;
    if (vsize > 64) {
        fprintf(stderr, "Bad vertex size %u\n", vsize);
        return false;
    }
    unsigned int desc_tex = 0;
    ss >> desc_tex;
    if (desc_tex > vsize-8) {
        fprintf(stderr, "Bad texture offset %u\n", desc_tex);
        return false;
    }
    unsigned int desc_color = 0;
    ss >> desc_color;
    if (desc_color > vsize-4) {
        fprintf(stderr, "Bad color offset %u\n", desc_color);
        return false;
    }
    if (desc_color && !(flags & MESH_FLAG_COLOR_BYTES)) {
        fprintf(stderr, "Colors can only be bytes\n");
        return false;
    }
    unsigned int vcount = 0;
    ss >> vcount;
    if (vcount < 3 || vcount > 65535) {
        fprintf(stderr, "Bad vertex count %u\n", vcount);
        return false;
    }
    unsigned char vdata[64] = { 0 };
    std::vector<char> vertexdata;
    for (unsigned int i = 0; i != vcount; ++i) {
        float x, y;
        ss >> x;
        ss >> y;
        put(x, 0, vdata);
        put(y, 4, vdata);
        if (desc_tex) {
            float u, v;
            ss >> u;
            ss >> v;
            put(u, desc_tex, vdata);
            put(v, desc_tex+4, vdata);
        }
        if (desc_color) {
            uint32_t c;
            ss >> c;
            put(c, desc_color, vdata);
        }
        vertexdata.insert(vertexdata.end(), vdata, &vdata[vsize]);
        if (ss.fail()) {
            fprintf(stderr, "Error reading vertex data\n");
            return false;
        }
    }
    unsigned int icnt = 0;
    ss >> icnt;
    if ((icnt == 0) || (icnt > 1024000) || (icnt % 3)) {
        fprintf(stderr, "Bad index count %u\n", icnt);
        return false;
    }
    std::vector<unsigned short> indexdata;
    for (unsigned int ix = 0; ix != icnt; ++ix) {
        unsigned int i;
        ss >> i;
        if (i >= vcount) {
            fprintf(stderr, "Bad index value %u at index %u\n", i, ix);
            return false;
        }
        if (ss.fail()) {
            fprintf(stderr, "Error reading index data\n");
            return false;
        }
        indexdata.push_back(i);
    }
    gg_allocate_mesh(&vertexdata[0], vsize, vcount, &indexdata[0], icnt, desc_tex, desc_color, mesh, MESH_FLAG_COLOR_BYTES);
    return true;
}

Mesh const *gg_load_named_mesh(char const *name, char *error, size_t esize) {
    std::string path(std::string("data/") + name + ".mesh");
    auto ptr = gNamedMeshes.find(path);
    if (ptr != gNamedMeshes.end()) {
        return &(*ptr).second;
    }
    std::string mdata;
    if (!load_text(path, &mdata)) {
        snprintf(error, esize, "Could not find mesh file: %s", path.c_str());
        return NULL;
    }
    Mesh m;
    if (!parse_mesh(mdata, &m)) {
        snprintf(error, esize, "Could not parse mesh: %s", path.c_str());
        return NULL;
    }
    check();
    gNamedMeshes[path] = m;
    return &gNamedMeshes[path];
}

void gg_clear_named_meshes() {
    for (auto &p : gNamedMeshes) {
        gg_clear_mesh(&p.second);
    }
    gNamedMeshes.clear();
}

static void use(Program const *program) {
    if (program->program != boundProgram) {
        boundProgram = program->program;
        glUseProgram(boundProgram);
        check();
    }
}

static void use(Texture const *texture) {
    if (boundTexture != texture->texture) {
        boundTexture = texture->texture;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture->texture);
        check();
    }
}

static void use(Mesh const *mesh, Program const *program) {
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vertexbuf);
    check();
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, mesh->vertexsize, (GLvoid *)0);
    glEnableVertexAttribArray(0);
    check();
    if (mesh->desc_tex && program->v_tex != (unsigned int)-1) {
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, mesh->vertexsize, (GLvoid *)(ptrdiff_t)mesh->desc_tex);
        check();
    } else {
        glDisableVertexAttribArray(1);
        check();
    }
    if (mesh->desc_color && program->v_color != (unsigned int)-1) {
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, (mesh->flags & MESH_FLAG_COLOR_BYTES) ? GL_UNSIGNED_BYTE : GL_FLOAT,
                (mesh->flags & MESH_FLAG_COLOR_BYTES) ? GL_TRUE : GL_FALSE, mesh->vertexsize, (GLvoid *)(ptrdiff_t)mesh->desc_color);
        check();
    } else {
        glDisableVertexAttribArray(2);
        check();
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->numindices ? mesh->indexbuf : 0);
    check();
}


void gg_set_program_transform(Program const *program, float const *transform) {
    if (program->g_transform != (unsigned int)-1) {
        use(program);
        glUniformMatrix4fv(program->g_transform, 1, GL_FALSE, transform);
        check();
    }
}

void gg_set_named_program_transforms(float const *transform) {
    for (auto const &progs : gNamedPrograms) {
        gg_set_program_transform(&progs.second, transform);
    }
}

void gg_draw_mesh(MeshDrawOp const *draw) {
    START_WATCH("gg_draw_mesh");
    check();
    LAP_WATCH("check");
    use(draw->program);
    LAP_WATCH("use program");
    if (draw->transform) {
        gg_set_program_transform(draw->program, draw->transform);
    }
    LAP_WATCH("gg_set_program_transform");
    if (draw->texture) {
        use(draw->texture);
    }
    LAP_WATCH("use texture");
    use(draw->mesh, draw->program);
    LAP_WATCH("use mesh");
    if (draw->program->g_color != (unsigned int)-1) {
        glUniform4fv(draw->program->g_color, 1, draw->color);
    }
    LAP_WATCH("glUniform color");
    if (draw->mesh->numindices) {
        glDrawElements(draw->primitive == PK_Lines ? GL_LINES : GL_TRIANGLES,
            draw->mesh->numindices, GL_UNSIGNED_SHORT, (GLvoid *)0);
        LAP_WATCH("glDrawElements");
    } else {
        glDrawArrays(draw->primitive == PK_Lines ? GL_LINES : GL_TRIANGLES, 0, draw->mesh->numvertices);
        LAP_WATCH("glDrawArrays");
    }
    check();
    LAP_REPORT();
}

void gg_get_gui_transform(float *oMatrix) {
    memset(oMatrix, 0, 16*sizeof(float));
    oMatrix[0] = 2.0f / gCtx.screen_width;
    oMatrix[5] = 2.0f / gCtx.screen_height;
    oMatrix[12] = -1.0f;
    oMatrix[13] = -1.0f;
    oMatrix[15] = 1.0f;
}

float const *gg_gui_transform() {
    return guiTransform;
}


struct BoxVertex {
    float x;
    float y;
    uint32_t c;
};


static std::vector<TTVertex> textVertices;
static std::vector<uint16_t> textIndices;
static std::vector<BoxVertex> boxVertices;
static std::vector<uint16_t> boxIndices;
static std::vector<BoxVertex> lineVertices;


static void add_text_index(std::vector<TTVertex> const &v, std::vector<uint16_t> const &i, int n) {
    textVertices.insert(textVertices.end(), v.begin(), v.begin() + n*4);
    textIndices.insert(textIndices.end(), i.begin(), i.begin() + n*6);
}

void gg_draw_text(float x, float y, float size, char const *text, uint32_t color) {
    std::vector<TTVertex> vec;
    std::vector<unsigned short> ind;
    size_t len = strlen(text);
    if (!len) {
        return;
    }
    float ox = x;
    float oy = y;
    vec.resize(len * 4);
    ind.resize(len * 6);
    int n = draw_truetype(text, &x, &y, &vec[0], (int)len*4);
    if (!n) {
        return;
    }
    for (int i = 0; i != n; ++i) {
        vec[i].x = ox + (vec[i].x-ox)*size;
        vec[i].y = oy + (vec[i].y-oy)*size;
        vec[i].c = color;
    }
    unsigned short o = 0;
    unsigned int root = textVertices.size();
    for (size_t i = 0; i != len; i++) {
        ind[o++] = i*4 + root;
        ind[o++] = i*4+1 + root;
        ind[o++] = i*4+2 + root;
        ind[o++] = i*4+2 + root;
        ind[o++] = i*4+3 + root;
        ind[o++] = i*4 + root;
    }
    add_text_index(vec, ind, n/4);
}

void gg_draw_box(float left, float bottom, float right, float top, uint32_t color) {
    uint16_t cnt = (uint16_t)boxVertices.size();
    boxVertices.push_back({ left, bottom, color });
    boxVertices.push_back({ right, bottom, color });
    boxVertices.push_back({ right, top, color });
    boxVertices.push_back({ left, top, color });
    boxIndices.push_back(cnt);
    boxIndices.push_back(cnt+1);
    boxIndices.push_back(cnt+2);
    boxIndices.push_back(cnt+2);
    boxIndices.push_back(cnt+3);
    boxIndices.push_back(cnt);
}


void service_mouse(Context *ctx) {
#if READMOUSE
    if (ctx->mousefd <= 0) {
        return;
    }
    ctx->prevx = ctx->mousex;
    ctx->prevy = ctx->mousey;
    ctx->prevbuttons = ctx->mousebuttons;
    unsigned char packet[3];
    while (true) {
        int r = ::read(ctx->mousefd, packet, 3);
        if (r < 3) {
            break;
        }
resync:
        //  decode the PS/2 mouse protocol provided by the "mice" device
        if (!(packet[0] & 8))  {
            packet[0] = packet[1];
            packet[1] = packet[2];
            if (::read(ctx->mousefd, &packet[2], 1) < 1) {
                break;
            }
            goto resync;
        }
        ctx->mousex += packet[1];
        ctx->mousey += packet[2];
        if (packet[0] & (1 << 4)) {
            ctx->mousex -= 256;
        }
        if (packet[0] & (1 << 5)) {
            ctx->mousey -= 256;
        }
        ctx->mousebuttons = packet[0] & 7;
    }
#else
    Window retroot, retwin;
    int rootx = 0, rooty = 0, winx = 0, winy = 0;
    unsigned int buttons = 0;
    if (XQueryPointer(ctx->xdisplay, ctx->rootwindow, &retroot, &retwin,
                &rootx, &rooty, &winx, &winy, &buttons) == True) {
        ctx->mousex = rootx;
        ctx->mousey = ctx->screen_height - rooty - 1;
        assert(Button1Mask == (1 << 8));
        ctx->mousebuttons = (buttons >> 8) & 7;
    }
#endif
    if (ctx->mousex < 0) {
        ctx->mousex = 0;
    }
    if (ctx->mousey < 0) {
        ctx->mousey = 0;
    }
    if (ctx->mousex >= ctx->screen_width) {
        ctx->mousex = ctx->screen_width - 1;
    }
    if (ctx->mousey >= ctx->screen_height) {
        ctx->mousey = ctx->screen_height - 1;
    }
}

static void generate_mouse_events(Context *ctx) {
    if (ctx->mousex != ctx->prevx || ctx->mousey != ctx->prevy) {
        ctx->prevx = ctx->mousex;
        ctx->prevy = ctx->mousey;
        if (cb_mousemove) {
            cb_mousemove(ctx->mousex, ctx->mousey, ctx->prevbuttons);
        }
    }
    if (ctx->mousebuttons != ctx->prevbuttons) {
        if (cb_mousebutton) {
            unsigned int diff = ctx->mousebuttons ^ ctx->prevbuttons;
            if (diff & 1) {
                cb_mousebutton(ctx->mousex, ctx->mousey, 0, (ctx->mousebuttons & 1) ? 1 : 0);
            }
            if (diff & 2) {
                cb_mousebutton(ctx->mousex, ctx->mousey, 1, (ctx->mousebuttons & 1) ? 1 : 0);
            }
            if (diff & 4) {
                cb_mousebutton(ctx->mousex, ctx->mousey, 2, (ctx->mousebuttons & 1) ? 1 : 0);
            }
        }
        ctx->prevbuttons = ctx->mousebuttons;
    }
}

int gg_setup(unsigned int width, unsigned int height, int left, int top, char const *name) {
    if (!init_ogl(&gCtx, width, height)) {
        return -1;
    }
    return 0;
}

void gg_get_viewport(int *owidth, int *oheight) {
    *owidth = gCtx.screen_width;
    *oheight = gCtx.screen_height;
}

void gg_onmousemove(void (*mousemovefn)(int x, int y, unsigned int buttons)) {
    cb_mousemove = mousemovefn;
}

void gg_onmousebutton(void (*mousebuttonfn)(int x, int y, int button, int down)) {
    cb_mousebutton = mousebuttonfn;
}

void gg_ondraw(void (*drawfn)()) {
    cb_draw = drawfn;
}

void gg_set_quit_flag() {
    g_running = false;
}

void gg_run(void (*idlefn)()) {
    g_running = true;
    while (g_running) {
        START_WATCH("gg_run");
        //  lastSwapTime = current_time()
        service_mouse(&gCtx);
        LAP_WATCH("service_mouse");
        generate_mouse_events(&gCtx);
        LAP_WATCH("generate_mouse_events");
        if (idlefn) {
            idlefn();
        }
        LAP_WATCH("idlefn");
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);
        LAP_WATCH("start_draw");
        if (cb_draw) {
            cb_draw();
        }
        LAP_WATCH("cb_draw");
        if (boxVertices.size()) {
            gg_set_mesh(&boxMesh, &boxVertices[0], boxVertices.size(), &boxIndices[0], boxIndices.size());
            boxMesh.numvertices = boxVertices.size();
            boxMesh.numindices = boxIndices.size();
            MeshDrawOp mdo = { boxProgram, NULL, &boxMesh, guiTransform, { 1, 1, 1, 1 } };
            gg_draw_mesh(&mdo);
            boxVertices.clear();
            boxIndices.clear();
        }
        LAP_WATCH("boxVertices");
        if (lineVertices.size()) {
            gg_set_mesh(&lineMesh, &lineVertices[0], lineVertices.size(), NULL, 0);
            lineMesh.numvertices = lineVertices.size();
            lineMesh.numindices = 0;
            MeshDrawOp mdo = { boxProgram, NULL, &lineMesh, guiTransform, { 1, 1, 1, 1 }, PK_Lines };
            gg_draw_mesh(&mdo);
            lineVertices.clear();
        }
        LAP_WATCH("lineVertices");
        if (textVertices.size()) {
            gg_set_mesh(&fontMesh, &textVertices[0], textVertices.size(), &textIndices[0], textIndices.size());
            fontMesh.numvertices = textVertices.size();
            fontMesh.numindices = textIndices.size();
            MeshDrawOp mdo = { fontProgram, &fontTexture, &fontMesh, guiTransform, { 1, 1, 1, 1 } };
            gg_draw_mesh(&mdo);
            textVertices.clear();
            textIndices.clear();
        }
        LAP_WATCH("textVertices");
        check();
        glFlush();
        LAP_WATCH("glFlush()");
        eglSwapBuffers(gCtx.display, gCtx.surface);
        LAP_WATCH("eglSwapBuffers()");
        LAP_REPORT();
    }
}


void gg_init_color(float *d, float r, float g, float b, float a) {
    d[0] = r;
    d[1] = g;
    d[2] = b;
    d[3] = a;
}

namespace color {
    uint32_t const white = 0xffffffff;
    uint32_t const midgray = 0xff808080;
    uint32_t const black = 0xff000000;

    uint32_t const textgray = 0xffc0c0c0;
    uint32_t const textyellow = 0xffa0f0f0;
    uint32_t const textblue = 0xffffa0a0;
    uint32_t const textred = 0xffa0a0ff;
    uint32_t const textgreen = 0xffa0ffa0;

    uint32_t const bggray = 0xff202020;
    uint32_t const bgyellow = 0xff204040;
    uint32_t const bgblue = 0xff402020;
    uint32_t const bgred = 0xff202040;
    uint32_t const bggreen = 0xff204020;
}
#endif  //  __arm__


