#include "../pilot/image.h"
#include <opencv/cv.h>
#include <opencv/highgui.h>



void show_image(char const *path) {
}

void apply_option(int &index, int argc, char const *argv[]) {
}

int main(int argc, char const *argv[]) {
    for (int i = 1; argv[i]; ++i) {
        if (argv[i][0] == '-') {
            apply_option(i, argc, argv);
            continue;
        }
        show_image(argv[i]);
    }
}

