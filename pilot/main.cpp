#include "glesgui.h"
#include "RaspiCapture.h"
#include "image.h"
#include "queue.h"
#include "network.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>

static char errbuf[256];
static FrameQueue guiFromNetwork(3, 0, 0, 0, 0);
static Texture netTextureY;
static Texture netTextureU;
static int im_width;
static int im_height;
static int im_planes;
static size_t im_size;

void do_click(int mx, int my, int btn, int st) {
}

void do_move(int x, int y, unsigned int btns) {
}

size_t framesDrawn;
uint64_t framesStart;

void do_draw() {
    if (framesDrawn == 64) {
        uint64_t stop = get_microseconds();
        fprintf(stderr, "GUI fps: %.1f\n", framesDrawn * 1000000.0 / (stop - framesStart));
        framesDrawn = 0;
        framesStart = stop;
    }
    ++framesDrawn;
}

static unsigned char byte_from_float(float f) {
    if (f < -1.0f) {
        return 0.0f;
    }
    if (f < 0.0f) {
        return f * 0.1f + 0.1f;
    }
    if (f < 1.0f) {
        return f * 0.8f + 0.1f;
    }
    if (f < 2.0f) {
        return f * 0.1f + 0.8f;
    }
    return 1.0f;
}

void do_idle() {
    Frame *fr = guiFromNetwork.beginRead();
    if (fr) {
        float const *f = (float const *)fr->link_->data_;
        unsigned char *t = (unsigned char *)netTextureY.mipdata[0];
        for (int r = 0; r != im_height; ++r) {
            t = (unsigned char *)netTextureY.mipdata[0] + netTextureY.width * r;
            for (int c = 0; c != im_width; ++c) {
                *t++ = byte_from_float(*f++);
            }
        }
        for (int r = 0; r != im_height; ++r) {
            t = (unsigned char *)netTextureU.mipdata[0] + netTextureU.width * r;
            for (int c = 0; c != im_width; ++c) {
                *t++ = byte_from_float(*f++);
            }
        }
        fr->endRead();
        gg_update_texture(&netTextureY, 0, im_width, 0, im_height);
        gg_update_texture(&netTextureU, 0, im_width, 0, im_height);
    }
}


int nframes = 0;
uint64_t start;
static FrameQueue *networkQueue;
static int numMissed;
static int numProcessed;

void buffer_callback(void *data, size_t size, void *cookie) {
    Frame *f = networkQueue->beginWrite();
    if (f == NULL) {
        ++numMissed;
    } else {
        unwarp_image(data, f->data_);
        f->endWrite();
        ++numProcessed;
    }
    ++nframes;
    if (!(nframes & 0x3f)) {
        uint64_t stop = get_microseconds();
        fprintf(stderr, "%d frames in %.3f seconds; %.1f fps; %.1f%% processing\n",
                nframes, (stop - start) * 1e-6, (0x40)/((stop-start)*1e-6),
                float(numProcessed) * 100.0f / (numProcessed+numMissed));
        start = stop;
        numProcessed = 0;
        numMissed = 0;
    }
}

void setup_run() {

    if (!load_network("network", &guiFromNetwork)) {
        fprintf(stderr, "network could not be loaded\n");
        exit(1);
    }
    networkQueue = network_input_queue();

    get_unwarp_info(&im_size, &im_width, &im_height, &im_planes);
    gg_allocate_texture(NULL, im_width, im_height, 1, 1, &netTextureY);
    gg_allocate_texture(NULL, im_width, im_height, 1, 1, &netTextureU);

    mkdir("/var/tmp/pilot", 0775);

    static char path[156];
    time_t t;
    time(&t);
    sprintf(path, "/var/tmp/pilot/capture-%ld", (long)t);
    CaptureParams cap = {
        640, 480, path, &buffer_callback, NULL
    };
    network_start();
    setup_capture(&cap);

    //set_recording(true);
}

void enum_errors(char const *err, void *p) {
    (*(int *)p) += 1;
    fprintf(stderr, "%s\n", err);
}

int main(int argc, char const *argv[]) {
    if (gg_setup(800, 480, 0, 0, "pilot") < 0) {
        fprintf(stderr, "GLES context setup failed\n");
        return -1;
    }
    int compileErrors = 0;
    bool compiled = false;
    while (argv[1] && argv[2] && !strcmp(argv[1], "--compile")) {
        compiled = true;
        Program const *p = gg_compile_named_program(argv[2], errbuf, sizeof(errbuf));
        if (!p) {
            fprintf(stderr, "Failed to compile %s program:\n%s\n", argv[2], errbuf);
            ++compileErrors;
        }
        argv += 2;
        argc -= 2;
    }
    while (argv[1] && argv[2] && !strcmp(argv[1], "--loadmesh")) {
        compiled = true;
        Mesh const *p = gg_load_named_mesh(argv[2], errbuf, sizeof(errbuf));
        if (!p) {
            fprintf(stderr, "Failed to load %s mesh:\n%s\n", argv[2], errbuf);
            ++compileErrors;
        }
        argv += 2;
        argc -= 2;
    }
    while (argv[1] && argv[2] && !strcmp(argv[1], "--loadtexture")) {
        compiled = true;
        Texture const *p = gg_load_named_texture(argv[2], errbuf, sizeof(errbuf));
        if (!p) {
            fprintf(stderr, "Failed to load %s texture:\n%s\n", argv[2], errbuf);
            ++compileErrors;
        }
        argv += 2;
        argc -= 2;
    }
    if (compiled) {
        if (compileErrors) {
            return -1;
        }
        return 0;
    }
    if (argc != 2 || strcmp(argv[1], "--test")) {
        gg_onmousebutton(do_click);
        gg_onmousemove(do_move);
        gg_ondraw(do_draw);
        setup_run();
        gg_run(do_idle);
    }
    int num = 0;
    gg_get_gl_errors(enum_errors, &num);
    return (num > 0) ? 1 : 0;
}

