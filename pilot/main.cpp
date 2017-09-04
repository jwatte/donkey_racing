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

char errbuf[256];

void do_click(int mx, int my, int btn, int st) {
}

void do_move(int x, int y, unsigned int btns) {
}

void do_draw() {
}

void do_idle() {
}


int im_width;
int im_height;
int im_planes;
size_t im_size;
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
    if (!load_network("network")) {
        fprintf(stderr, "network could not be loaded\n");
        exit(1);
    }
    networkQueue = network_input_queue();

    mkdir("/var/tmp/pilot", 0775);

    static char path[156];
    time_t t;
    time(&t);
    sprintf(path, "/var/tmp/pilot/capture-%ld", (long)t);
    get_unwarp_info(&im_size, &im_width, &im_height, &im_planes);
    CaptureParams cap = {
        640, 480, path, &buffer_callback, NULL
    };
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

