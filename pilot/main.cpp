#include "glesgui.h"

#include <stdio.h>
#include <string.h>


char errbuf[256];

void do_click(int mx, int my, int btn, int st) {
    gg_set_quit_flag();
}

void do_move(int x, int y, unsigned int btns) {
}

Program const *guiProgram;

void do_draw() {
}

void do_idle() {
}

int init() {
    guiProgram = gg_compile_named_program("gui", errbuf, sizeof(errbuf));
    if (!guiProgram) {
        fprintf(stderr, "Failed to compile GUI program:\n%s\n", errbuf);
        return -1;
    }
    return 0;
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
        if (init()) {
            return -1;
        }
        gg_onmousebutton(do_click);
        gg_onmousemove(do_move);
        gg_ondraw(do_draw);
        gg_run(do_idle);
    }
    return 0;
}

