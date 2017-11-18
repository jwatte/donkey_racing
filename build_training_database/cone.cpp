#include "cone.h"
#include "../stb/stb_image_write.h"
#include <math.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>


struct WeightArea {
    int cx;
    int cy;
    int n;
};

int find_orange_area(unsigned char *rgb, int width, int height, int *cx, int *cy, bool mark) {
    WeightArea weightAreas[100];
    int nweightAreas = 0;
    for (int r = 0; r != height; ++r) {
        unsigned char *b = rgb + width * 3 * r;
        for (int c = 0; c != width; ++c) {
            //  red brightness
            if (b[0] >= 130) {
                //  green ratio
                float gr = (float)b[1] / (float)b[0];
                if (gr >= 0.13f && gr < 0.5f) {
                    //  blue ratio
                    float br = (float)b[2] / (float)b[0];
                    if (br >= 0.05f && br < 0.27f && br < gr) {
                        //  is orange pixel!
                        if (mark) {
                            b[0] = 0;
                            b[1] = 255;
                            b[2] = 0;
                        }
                        for (int j = 0; j != nweightAreas; ++j) {
                            int dr = r - weightAreas[j].cy;
                            int dc = c - weightAreas[j].cx;
                            if (dr*dr + dc*dc < 100) {
                                weightAreas[j].n += 1;
                                goto found;
                            }
                        }
                        if (nweightAreas < 100) {
                            weightAreas[nweightAreas].cx = c;
                            weightAreas[nweightAreas].cy = r;
                            weightAreas[nweightAreas].n = 1;
                            nweightAreas += 1;
                        }
found:
                        ;
                    }
                }
            }
            b += 3;
        }
    }
    if (nweightAreas == 0) {
        return 0;
    }
    WeightArea best = { 0 };
    WeightArea cur = { 0 };
    for (int a = 0; a != nweightAreas; ++a) {
        cur = weightAreas[a];
        int cnt = 1;
        int ccx = cur.cx;
        int ccy = cur.cy;
        for (int b = 0; b != nweightAreas; ++b) {
            if (a != b) {
                WeightArea const &d = weightAreas[b];
                int dr = cur.cy-d.cy;
                int dc = cur.cx-d.cx;
                if (dr*dr+dc*dc <= 256) {
                    cur.n += d.n;
                    ccx += d.cx;
                    ccy += d.cy;
                    ++cnt;
                }
            }
        }
        if (cur.n > best.n) {
            best.cx = ccx / cnt;
            best.cy = ccy / cnt;
            best.n = cur.n;
        }
    }
    *cx = best.cx;
    *cy = best.cy;
    return best.n;
}

int fixup_cone_labels(unsigned char *outputrgb, int width, int height, bool print, int serial, float *label) {
    int cx = 0, cy = 0;
    int npx = find_orange_area(outputrgb, width, height, &cx, &cy, print);
    if (npx > 10*10) {
        float xcone = (float)cx / width - 0.5f;
        if (fabsf(xcone) < 0.25f) { //  in center of image
            float alpha = (float)cy / height;
            float turn = (cx > width / 2) ? -2.0f : 2.0f;  //  veer
            if (label[0] < -0.5f) {
                turn = -2.0f;
            } else if (label[0] > 0.5f) {
                turn = 2.0f;
            }
            label[0] = label[0] * (1.0f - alpha) + turn * alpha;
            if (label[0] < -1.0f) {
                label[0] = -1.0f;
            }
            if (label[0] > 1.0f) {
                label[0] = 1.0f;
            }
        } else if ((xcone > 0) == (label[0] > 0)) { //  steering into cone
            label[0] = -((float)cx / width - 0.5f) / (1.0f + (float)cy / height); // avoid
        }
        if (label[1] > (1.5f - fabsf(label[0])) * 0.66f) {
            label[1] =  (1.5f - fabsf(label[0])) * 0.66f;
        }
        if (print) {
            fprintf(stderr, "\nframe %d: cone at %d,%d: %.2f/%.2f\n", serial, cx, cy, label[0], label[1]);
            char buf[100];
            mkdir("cone-detect", 0777);
            sprintf(buf, "cone-detect/%06d_%.2f_%.2f.png", serial, label[0], label[1]);
            stbi_write_png(buf, width, height, 3, outputrgb, 0);
        }
    }
    return npx;
}
