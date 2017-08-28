#include "glesgui.h"
#include "truetype.h"
#include "../stb/stb_image.h"

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

#include "bcm_host.h"

#include "GLES2/gl2.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"

#define check() assert(glGetError() == 0)


struct Context {
    uint32_t screen_width;
    uint32_t screen_height;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;

    EGL_DISPMANX_WINDOW_T nativewindow;

    int mousefd;
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
static std::map<std::string, Texture> gNamedTextures;



static bool init_ogl(Context *ctx, unsigned int width, unsigned int height) {

    bcm_host_init();

    memset(ctx, 0, sizeof(*ctx));
    ctx->mousefd = -1;
    ctx->screen_width = width;
    ctx->screen_height = height;

    if (init_truetype("/usr/share/fonts/truetype/roboto/Roboto-Regular.ttf") < 0) {
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
    glClearColor(0.0f, 0.25f, 0.50f, 1.0f);
    glClearDepthf(1.0f);
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    check();

    ctx->mousefd = ::open("/dev/input/mice", O_RDONLY | O_NONBLOCK);
    if (ctx->mousefd < 0) {
        perror("/dev/input/mice");
        return false;
    }
    ctx->mousex = width / 2;
    ctx->mousey = height / 2;

    unsigned char const *data;
    unsigned int bmwidth;
    unsigned int bmheight;
    get_truetype_bitmap(&data, &bmwidth, &bmheight);
    gg_allocate_texture(data, bmwidth, bmheight, 0, 1, &fontTexture);

    return true;
}


int gg_compile_program(char const *vshader, char const *fshader, Program *oProg, char *error, size_t esize) {
    
    memset(oProg, 0, sizeof(*oProg));
    *error = 0;

    oProg->vshader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(oProg->vshader, 1, (GLchar const **)&vshader, 0);
    glCompileShader(oProg->vshader);
    if (glGetError() != GL_NO_ERROR) {
        glGetShaderInfoLog(oProg->vshader, sizeof(tmplog), NULL, tmplog);
        snprintf(error, esize, "vertex shader error: %s", tmplog);
        return -1;
    }

    oProg->fshader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(oProg->fshader, 1, (GLchar const **)&fshader, 0);
    glCompileShader(oProg->fshader);
    if (glGetError() != GL_NO_ERROR) {
        glGetShaderInfoLog(oProg->fshader, sizeof(tmplog), NULL, tmplog);
        snprintf(error, esize, "fragment shader error: %s", tmplog);
        return -1;
    }

    oProg->program = glCreateProgram();
    glAttachShader(oProg->program, oProg->vshader);
    glAttachShader(oProg->program, oProg->fshader);
    glLinkProgram(oProg->program);
    GLuint err = glGetError();
    GLint status = 0;
    glGetProgramiv(oProg->program, GL_LINK_STATUS, &status);
    if (err != GL_NO_ERROR || !status) {
        glGetProgramInfoLog(oProg->fshader, sizeof(tmplog), NULL, tmplog);
        snprintf(error, esize, "program error: %s", tmplog);
        return -1;
    }

    oProg->v_pos = glGetAttribLocation(oProg->program, "v_pos");
    oProg->v_tex = glGetAttribLocation(oProg->program, "v_tex");
    oProg->v_color = glGetAttribLocation(oProg->program, "v_color");
    oProg->g_color = glGetAttribLocation(oProg->program, "g_color");
    oProg->g_tex = glGetAttribLocation(oProg->program, "g_tex");

    return oProg->program;
}

void gg_clear_program(Program *iProc) {
    assert(iProc->program);
    glDeleteProgram(iProc->program);
    glDeleteShader(iProc->vshader);
    glDeleteShader(iProc->fshader);
    memset(iProc, 0, sizeof(*iProc));
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
    unsigned int d = 1;
    for (unsigned int ww = w, hh = h; ww > 4 && hh > 4; ww >>= 1, hh >>= 1) {
        ++d;
    }
    if (mipmaps > d || mipmaps == 0) {
        mipmaps = d;
    }
    oTex->width = w;
    oTex->height = h;
    oTex->miplevels = d;
    oTex->format = 4;
    oTex->mipdata = NULL;

    /* allocate VRAM */
    glGenTextures(1, &oTex->texture);
    glBindTexture(GL_TEXTURE_2D, oTex->texture);
    unsigned int iformat = format == 1 ? GL_ALPHA : format == 3 ? GL_RGB : GL_RGBA;
    unsigned int xformat = format == 1 ? GL_ALPHA : format == 3 ? GL_RGB : GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, iformat, w, h, 0, xformat, GL_UNSIGNED_BYTE, NULL);
    assert(!glGetError());
    while (d > 1) {
        d--;
        w >>= 1;
        h >>= 1;
        glTexImage2D(GL_TEXTURE_2D, oTex->miplevels - d, iformat, w, h, 0, xformat, GL_UNSIGNED_BYTE, NULL);
        assert(!glGetError());
    }

    /* set filtering parameters */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, oTex->miplevels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    assert(!glGetError());

    /* allocate storage */
    size_t needed = oTex->miplevels * sizeof(void *);
    unsigned int wh = oTex->width * oTex->height;
    for (unsigned int m = 0; m != oTex->miplevels; ++m) {
        needed += wh * 4;
        wh >>= 2;
    }
    oTex->mipdata = (void **)calloc(needed / 4, 4);
    needed = oTex->miplevels * sizeof(void *);
    wh = oTex->width * oTex->height;
    void *base = oTex->mipdata + oTex->miplevels;
    for (unsigned int m = 0; m != oTex->miplevels; ++m) {
        oTex->mipdata[m] = base;
        base = (unsigned char *)base + wh * 4;
    }

    /* fill in image, if present */
    if (data) {
        gg_update_texture(oTex, width * 4, 0, width, 0, height);
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

void gg_update_texture(Texture *tex, unsigned int rowbytes, unsigned int left, unsigned int width, unsigned int top, unsigned int height) {
    assert(tex->mipdata);
    glBindTexture(GL_TEXTURE_2D, tex->texture);
    unsigned int xformat = tex->format == 1 ? GL_ALPHA : tex->format == 3 ? GL_RGB : GL_RGBA;
    unsigned int w = tex->width;
    unsigned int h = tex->height;
    for (unsigned int i = 0; i < tex->miplevels; ++i) {
        /* GLES 2.0 does not support UNPACK_ROW_LENGTH so upload the whole thing. */
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, xformat, GL_UNSIGNED_BYTE, tex->mipdata[i]);
        if (i + 1 < tex->miplevels) {
            mip_filter(tex, i, left, top, width, height);
        }
        left >>= 1;
        top >>= 1;
        width = (width + 1) >> 1;
        height = (height + 1) >> 1;
    }
    assert(!glGetError());
}

void gg_clear_texture(Texture *tex) {
    free(tex->mipdata);
    glDeleteTextures(1, &tex->texture);
}

Texture const *gg_load_named_texture(char const *name, char *error, size_t esize) {
    *error = 0;
    auto const &ptr(gNamedTextures.find(name));
    if (ptr != gNamedTextures.end()) {
        return &(*ptr).second;
    }
    std::string fn("data/");
    fn += name;
    int x, y, n;
    unsigned char *data = stbi_load(fn.c_str(), &x, &y, &n, 0);
    if (!data) {
        return NULL;
    }
    Texture *t = &gNamedTextures[name];
    gg_allocate_texture(data, x, y, 0, n, t);
    stbi_image_free(data);
    return t;
}

void gg_clear_named_textures() {
    for (auto &p : gNamedTextures) {
        gg_clear_texture(&p.second);
    }
    gNamedTextures.clear();
}


void service_mouse(Context *ctx) {
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
        //  decode the PS/2 mouse protocol provided by the "mice" device
        if (!(packet[0] & 8))  {
            ::read(ctx->mousefd, packet, 1);    //  skip to sync up
            continue;
        }
        ctx->mousex += packet[1];
        ctx->mousey += packet[2];
        if (packet[0] & (1 << 4)) {
            ctx->mousex -= 256;
        }
        if (packet[0] & (1 << 5)) {
            ctx->mousey -= 256;
        }
    }
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
    ctx->mousebuttons = packet[0] & 7;
}

static void generate_mouse_events(Context *ctx) {
    if (ctx->mousex != ctx->prevx || ctx->mousey != ctx->prevy) {
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
    }
}

int gg_setup(unsigned int width, unsigned int height, int left, int top, char const *name) {
    if (!init_ogl(&gCtx, width, height)) {
        return -1;
    }
    return 0;
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

static bool g_running;

void gg_set_quit_flag() {
    g_running = false;
}

void gg_run(void (*idlefn)()) {
    g_running = true;
    while (g_running) {
        //  lastSwapTime = current_time()
        service_mouse(&gCtx);
        generate_mouse_events(&gCtx);
        if (idlefn) {
            idlefn();
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (cb_draw) {
            cb_draw();
        }
        glFlush();
        // diff = current_time() - lastSwapTime
        // if (diff < 0.016) {
        //   usleep((useconds_t)((0.016 - diff) * 1000000));
        // }
        eglSwapBuffers(gCtx.display, gCtx.surface);
    }
}



