#include <opencv2/core/core.hpp>
#if 0
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#else
#include <opencv2/imgproc/imgproc.hpp>
#endif
#include <opencv2/highgui/highgui.hpp>

#include <iostream>
#include <string>

#include "flat_table.h"

using namespace cv;
using namespace std;

TableInputCoord tc[FLAT_RECTIFIED_HEIGHT][FLAT_RECTIFIED_WIDTH];

int main(int argc, char const *argv[]) {
    if (argc == 1) {
        cerr << "usage: flatshow image ..." << endl;
        exit(1);
    }
    for (int r = 0; r != FLAT_RECTIFIED_HEIGHT; ++r) {
        for (int c = 0; c != FLAT_RECTIFIED_WIDTH; ++c) {
            tc[r][c].sx = sFlatTableInputCoords[r][c].sy;
            tc[r][c].sy = sFlatTableInputCoords[r][c].sx;
        }
    }
    Mat mapxy(FLAT_RECTIFIED_HEIGHT, FLAT_RECTIFIED_WIDTH, CV_32FC2, (float *)tc);
    Mat proc(FLAT_RECTIFIED_HEIGHT, FLAT_RECTIFIED_WIDTH, CV_32FC3, Scalar::all(0.0f));
    for (int i = 1; i < argc; ++i) {
        Mat image;
        image = imread(argv[i], IMREAD_COLOR);
        if (image.empty()) {
            cerr << "could not open: " << argv[1] << endl;
        } else {
            image.at<uint32_t>(0, 0, 0) = 0;
            remap(image, proc, mapxy, Mat(), INTER_LINEAR);
            Mat *f = new Mat(proc.clone());
            namedWindow("image", WINDOW_AUTOSIZE);
            imshow("image", *f);
        }
        int k = waitKey(0);
        if (k == 's') {
            imwrite("saved.png", proc);
            fprintf(stderr, "saved saved.png\n");
        } else if (k == 27) {
            break;
        }
    }
    return 0;
}

