#include <opencv2/core/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <iostream>
#include <string>

#include "table.h"

using namespace cv;
using namespace std;

TableInputCoord tc[RECTIFIED_HEIGHT][RECTIFIED_WIDTH];

int main(int argc, char const *argv[]) {
    if (argc == 1) {
        cerr << "usage: flatshow image ..." << endl;
        exit(1);
    }
    for (int r = 0; r != RECTIFIED_HEIGHT; ++r) {
        for (int c = 0; c != RECTIFIED_WIDTH; ++c) {
            tc[r][c].sx = sTableInputCoords[r][c].sy;
            tc[r][c].sy = sTableInputCoords[r][c].sx;
        }
    }
    Mat mapxy(RECTIFIED_WIDTH, RECTIFIED_HEIGHT, CV_32FC2, (float *)tc);
    Mat proc(RECTIFIED_WIDTH, RECTIFIED_HEIGHT, CV_32FC3, Scalar::all(0.0f));
    for (int i = 1; i < argc; ++i) {
        Mat image;
        image = imread(argv[i], IMREAD_COLOR);
        if (image.empty()) {
            cerr << "could not open: " << argv[1] << endl;
        } else {
            remap(image, proc, mapxy, Mat(), INTER_LINEAR);
            Mat *f = new Mat(proc.clone());
            namedWindow(argv[i], WINDOW_AUTOSIZE);
            imshow(argv[1], *f);
        }
    }
    int k = waitKey(0);
    if (k == 's') {
        imwrite("saved.png", proc);
        fprintf(stderr, "saved saved.png\n");
    }
    return 0;
}

