#include "glesgui.h"

#include <stdio.h>
#include <string.h>


void do_click(int mx, int my, int btn, int st) {
    gg_set_quit_flag();
}

int main(int argc, char const *argv[]) {
    if (gg_setup(800, 480, 0, 0, "pilot") < 0) {
        fprintf(stderr, "GLES context setup failed\n");
        return -1;
    }
    if (argc != 2 || strcmp(argv[1], "--test")) {
        gg_onmousebutton(do_click);
        gg_run(NULL);
    }
    return 0;
}

