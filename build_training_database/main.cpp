#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/fcntl.h>
#include <math.h>

#include <list>
#include <string>
#include <vector>
#include <map>
#include <random>

extern "C" {

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>
#include <libavutil/buffer.h>
#include <libavutil/log.h>

}

#include "math2.h"
#include "../pilot/image.h"

#define CAFFE2_LOG_THRESHOLD 1
#include <caffe2/core/logging_is_not_google_glog.h>
#include <caffe2/core/db.h>
#include <caffe2/core/common.h>
#include <caffe2/core/init.h>
#include <caffe2/proto/caffe2.pb.h>
#include <caffe2/core/logging.h>

#include "../stb/stb_image_write.h"


bool verbose = false;
bool progress = isatty(2);
bool devmode = false;
bool dumppng = true;
bool fakedata = false;
bool output_as_streams = false;
bool computelabels = false;
float scaleSteer = 1.0f;
float scaleThrottle = 1.0f;
long skip = 0;

float minSteer = 0;
float maxSteer = 0;
float minThrottle = 0;
float maxThrottle = 0;
int numSkipped = 0;
int numWritten = 0;

int numfakeframes = 10000;
uint64_t maxconcatoffset;


struct file_header {
    char type[4];
    uint32_t zero;
    char subtype[4];
};

struct chunk_header {
    char type[4];
    uint32_t size;
};

struct stream_chunk {
    FILE *file;
    uint64_t concatoffset;
    uint32_t fileoffset;
    uint32_t streamoffset;
    uint32_t size;
};

std::list<stream_chunk> h264Chunks;
std::list<stream_chunk> timeChunks;
std::list<stream_chunk> pdtsChunks;
std::map<FILE *, std::string> fileNames;

struct steer_packet {
    uint16_t code;
    int16_t steer;
    int16_t throttle;
};
struct throttle_steer {
    float throttle;
    float steer;
};
struct pts_dts {
    uint64_t pts;
    uint64_t dts;
};

struct dump_chunk {
    char type[4];
    std::string filename;
};

std::list<dump_chunk> dumpChunks;
std::list<std::string> filenameArgs;
std::string datasetName;

static bool is_dir(char const *path) {
    struct stat st;
    if (stat(path, &st)) {
        return false;
    }
    if (!S_ISDIR(st.st_mode)) {
        return false;
    }
    return true;
}

static bool exists(char const *path) {
    struct stat st;
    if (!lstat(path, &st)) {
        return true;
    }
    return false;
}

int fc_w = 0;
int fc_h = 0;
unsigned char *fc_ptr = nullptr;

void mkfalse(unsigned char *d, unsigned char ix) {
    if (ix == 0) {
        d[0] = d[1] = d[2] = 0;
        return;
    }
    if (ix == 255) {
        d[0] = d[1] = d[2] = 255;
        return;
    }
    if (ix == 254) {
        d[0] = 255; d[1] = 0; d[2] = 255;
        return;
    }
    if (ix == 253) {
        d[0] = 255; d[1] = 255; d[2] = 0;
        return;
    }
    float s1 = sinf(ix);
    float s2 = sinf(ix + 2.1f);
    float s3 = sinf(ix - 2.1f);
    float k = 0.75f - ix / 255.0;
    float r = k + s1;
    float g = k + s2;
    float b = k + s3;
    d[0] = (r > 1.0f) ? 255 : (r < 0.0f) ? 0 : (unsigned char)r;
    d[1] = (g > 1.0f) ? 255 : (g < 0.0f) ? 0 : (unsigned char)g;
    d[2] = (b > 1.0f) ? 255 : (b < 0.0f) ? 0 : (unsigned char)b;
}

void write_false_color(char const *name, int width, int height, unsigned char const *frame) {
    if (fc_w != width || fc_h != height || !fc_ptr) {
        if (fc_ptr) {
            free(fc_ptr);
        }
        fc_ptr = (unsigned char *)malloc(width * height * 3);
        fc_w = width;
        fc_h = height;
    }
    unsigned char *d = fc_ptr;
    for (int cnt = 0, n = width * height; cnt != n; ++cnt) {
        mkfalse(d, *frame);
        ++frame;
        d += 3;
    }
    stbi_write_png(name, width, height, 3, fc_ptr, 0);
}


void check_header(FILE *f, char const *name) {
    file_header fh;
    if ((12 != fread(&fh, 1, 12, f)) || strncmp(fh.type, "RIFF", 4) || strncmp(fh.subtype, "h264", 4)) {
        fprintf(stderr, "%s: not a h264 RIFF file\n", name);
        exit(1);
    }
}

bool generate_requested_file(char const *type, char const *output) {
    abort();
    return false;
}

char const *get_filename(FILE *f) {
    static char ret[32];
    auto ptr = fileNames.find(f);
    if (ptr == fileNames.end()) {
        sprintf(ret, "%30s", "???");
    } else {
        char const *p = (*ptr).second.c_str();
        if (strlen(p) > 30) {
            p += strlen(p)-30;
        }
        sprintf(ret, "%30s", p);
    }
    return ret;
}


int stretchData = 7;
float stretchRotate = 0.04f;
float stretchOffset = 5.0f;
float testFraction = 0.1f;
unsigned char *outputframe;
int outputwidth;
int outputheight;
int outputplanes;
size_t outputsize;
static std::default_random_engine generator;
static std::uniform_real_distribution<float> distribution;
static auto stretch_random = std::bind(distribution, generator);
static caffe2::TensorProtos protos;
static caffe2::TensorProto *dataProto;
static caffe2::TensorProto *labelProto;
static int databaseKeyIndex;
std::unique_ptr<caffe2::db::DB> train_db;
std::unique_ptr<caffe2::db::DB> test_db;
std::unique_ptr<caffe2::db::Transaction> trainTransaction;
std::unique_ptr<caffe2::db::Transaction> testTransaction;

int testfds[2] = { -1, -1 };
int trainfds[2] = { -1, -1 };
pid_t testpid = -1;
pid_t trainpid = -1;

char *wrdata;

void test_write_loop() {
    float label[2];
    char c;
    if (!wrdata) {
        wrdata = (char *)malloc(outputsize);
    }
    int testNumFrames = 0;
    while (true) {
        if (read(testfds[0], &c, 1) != 1) {
            abort();
        }
        if (c == 'q') {
            break;
        }
        if (read(testfds[0], &databaseKeyIndex, sizeof(databaseKeyIndex))
                != sizeof(databaseKeyIndex)) {
            abort();
        }
        if (read(testfds[0], label, 8) != 8) {
            abort();
        }
        if (read(testfds[0], wrdata, outputsize) != outputsize) {
            abort();
        }
        dataProto->set_byte_data(wrdata, outputsize);
        labelProto->set_float_data(0, label[0]);
        labelProto->set_float_data(1, label[1]);

        char dki[12];
        sprintf(dki, "%08d", databaseKeyIndex);
        std::string keyname(dki);

        std::string value;
        protos.SerializeToString(&value);

        testTransaction->Put(keyname, value);
        ++testNumFrames;
        if (!(testNumFrames & 127)) {
            if (verbose) {
                fprintf(stderr, "partial commit of test database at %d items\n", testNumFrames);
            }
            testTransaction->Commit();
            testTransaction.reset();
            testTransaction = test_db->NewTransaction();
        }
    }
    close(testfds[0]);
}

void train_write_loop() {
    float label[2];
    char c;
    if (!wrdata) {
        wrdata = (char *)malloc(outputsize);
    }
    int trainNumFrames = 0;
    while (true) {
        if (read(trainfds[0], &c, 1) != 1) {
            abort();
        }
        if (c == 'q') {
            break;
        }
        if (read(trainfds[0], &databaseKeyIndex, sizeof(databaseKeyIndex))
                != sizeof(databaseKeyIndex)) {
            abort();
        }
        if (read(trainfds[0], label, 8) != 8) {
            abort();
        }
        if (read(trainfds[0], wrdata, outputsize) != outputsize) {
            abort();
        }
        dataProto->set_byte_data(wrdata, outputsize);
        labelProto->set_float_data(0, label[0]);
        labelProto->set_float_data(1, label[1]);

        char dki[12];
        sprintf(dki, "%08d", databaseKeyIndex);
        std::string keyname(dki);

        std::string value;
        protos.SerializeToString(&value);

        trainTransaction->Put(keyname, value);
        if (!(trainNumFrames & 127)) {
            if (verbose) {
                fprintf(stderr, "partial commit of train database at %d items\n", trainNumFrames);
            }
            trainTransaction->Commit();
            trainTransaction.reset();
            trainTransaction = train_db->NewTransaction();
        }
    }
    close(testfds[0]);
}

void begin_database_write(char const *output) {

    get_unwarp_info(&outputsize, &outputwidth, &outputheight, &outputplanes);
    outputsize = outputwidth * outputheight * outputplanes;
    outputframe = (unsigned char *)malloc(outputsize);
    memset(outputframe, 0xff, outputsize);

    protos = caffe2::TensorProtos();
    dataProto = protos.add_protos();
    dataProto->set_data_type(caffe2::TensorProto::BYTE);
    dataProto->add_dims(outputplanes);
    dataProto->add_dims(outputheight);
    dataProto->add_dims(outputwidth);
    labelProto = protos.add_protos();
    labelProto->set_data_type(caffe2::TensorProto::FLOAT);
    labelProto->add_dims(2);
    labelProto->add_float_data(0);
    labelProto->add_float_data(0);

    mkdir(output, 0777);
    if (!is_dir(output)) {
        fprintf(stderr, "%s: could not create directory\n", output);
        exit(1);
    }
    if (output_as_streams) {
        std::string path(output);
        path += "/test.stream";
        testfds[1] = open(path.c_str(), O_RDWR|O_CREAT|O_EXCL, 0666);
        if (testfds[1] < 0) {
            perror(path.c_str());
            exit(1);
        }
        path = output;
        path += "/train.stream";
        trainfds[1] = open(path.c_str(), O_RDWR|O_CREAT|O_EXCL, 0666);
        if (trainfds[1] < 0) {
            perror(path.c_str());
            exit(1);
        }
        return;
    }
    std::string path(output);
    path += "/train";
    if (exists(path.c_str())) {
        fprintf(stderr, "%s: already exists, not overwriting\n", path.c_str());
        exit(1);
    }
    std::string path2 = output;
    path2 += "/test";
    if (exists(path2.c_str())) {
        fprintf(stderr, "%s: already exists, not overwriting\n", path2.c_str());
        exit(1);
    }
    if ((pipe(testfds) < 0) || (pipe(trainfds) < 0)) {
        fprintf(stderr, "pipe() failed; cannot write training database\n");
        exit(1);
    }
    if (verbose) {
        fprintf(stderr, "Creating training database as '%s'\n", path.c_str());
    }
    trainpid = fork();
    if (trainpid == -1) {
        fprintf(stderr, "fork() failed; cannot write training database\n");
        exit(1);
    }
    if (!trainpid) {
        train_db = caffe2::db::CreateDB("lmdb", path, caffe2::db::NEW);
        if (!train_db) {
            fprintf(stderr, "create LMDB %s failed\n", path.c_str());
            exit(1);
        }
        trainTransaction = train_db->NewTransaction();
        train_write_loop();
        exit(0);
    }

    if (verbose) {
        fprintf(stderr, "Creating test database as '%s'\n", path2.c_str());
    }
    testpid = fork();
    if (testpid == -1) {
        fprintf(stderr, "fork() failed; cannot write testing database\n");
        exit(1);
    }
    if (!testpid) {
        test_db = caffe2::db::CreateDB("lmdb", path2, caffe2::db::NEW);
        if (!test_db) {
            fprintf(stderr, "create LMDB %s failed\n", path2.c_str());
            exit(1);
        }
        testTransaction = test_db->NewTransaction();
        test_write_loop();
        exit(0);
    }
}

int q;

void end_database_write() {
    if (verbose) {
        fprintf(stderr, "closing databases\n");
    }
    q += write(testfds[1], "q", 1);
    q += close(testfds[1]);
    q += write(trainfds[1], "q", 1);
    q += close(trainfds[1]);
    int st;
    waitpid(testpid, &st, 0);
    waitpid(trainpid, &st, 0);
}

void put_to_database2(unsigned char const *frame, float const *label) {
    ++databaseKeyIndex;
    if (stretch_random() < testFraction) {
        q += write(testfds[1], "w", 1);
        q += write(testfds[1], &databaseKeyIndex, sizeof(databaseKeyIndex));
        q += write(testfds[1], label, 8);
        q += write(testfds[1], frame, outputsize);
    } else {
        q += write(trainfds[1], "w", 1);
        q += write(trainfds[1], &databaseKeyIndex, sizeof(databaseKeyIndex));
        q += write(trainfds[1], label, 8);
        q += write(trainfds[1], frame, outputsize);
    }
}

unsigned char *flipbuf = 0;

void put_to_database(unsigned char const *frame, float *label) {
    put_to_database2(frame, label);
    /* stretch data some more by flipping horizontally */
    if (!flipbuf) {
        flipbuf = (unsigned char *)malloc(outputsize);
    }
    float label2[2];
    label2[0] = -label[0];
    label2[1] = label[1];
    int n = outputwidth;
    for (int i = 0; i != outputheight; ++i) {
        unsigned char const *src = frame + n * i;
        unsigned char *dst = flipbuf + n * (i+1);
        for (int j = 0; j != n; ++j) {
            *--dst = *src++;
        }
    }
    put_to_database2(flipbuf, label2);
}


#define FIX_ROWS 20
#define FIX_COLUMNS 40

void magically_fix_labeling(int serial, void const *yy, float *st) {
    unsigned char const *y = (unsigned char const *)yy + outputwidth*outputheight;
    int nleft = 0;
    int nright = 0;
    for (int r = 0; r < FIX_ROWS; ++r) {
        for (int c = 0; c < FIX_COLUMNS; ++c) {
            if (y[r*outputwidth+c] > 60) {
                nleft++;
            }
            if (y[(r+1)*outputwidth-c-1] > 60) {
                nright++;
            }
        }
    }
    float steer = float(nright-nleft);
    if (steer > -100 && steer < 100) {
        steer = 0;
    } else if (steer < 0) {
        steer = (steer + 100) / 200.0f;
    } else {
        steer = (steer - 100) / 200.0f;
    }
    if (steer < -4) {
        steer = -4;
    }
    if (steer > 4) {
        steer = 4;
    }
    float throttle = 400.0f / (nright+nleft+100);
    st[0] = steer;
    st[1] = throttle;
}

void histogram(char const *name, int width, int height, unsigned char const *data) {
    int count[256] = { 0 };
    for (int i = 0, cnt = width * height; i != cnt; ++i) {
        count[*data]++;
        ++data;
    }
    fprintf(stderr, "%s: ", name);
    for (int i = 0; i < 256; ++i) {
        if (count[i]) {
            fprintf(stderr, "%d:%d ", i, count[i]);
        }
    }
    fprintf(stderr, "\n");
}

void render_vote(unsigned char *fb, int width, int height,
        double cx, double cy, double dir, double power, unsigned char color)
{
    cx += width/2;
    cy += height/2;
    float s = sin(dir);
    float c = cos(dir);
    int n = power * 20;
    if (n < 5) {
        n = 5;
    } else if (n > 40) {
        n = 40;
    }
    while (n-- > 0) {
        int x = (int)floor(cx);
        int y = (int)floor(cy);
        cx = cx + s;
        cy = cy - c;
        if (x < 0 || x >= width || y < 0 || y >= height) {
            continue;
        }
        fb[x + y * width] = color;
    }
}

unsigned char *cl_temp;

void compute_labels_cv(
        int serial,
        void const *y,
        unsigned char const *frame,
        float *label)
{
    int const opw = outputwidth;
    int const oph = outputheight;
    //  calculate statistics on the input image
    double avg = 0;
    double avg2 = 0;
    int count = opw * oph;
    assert(opw * oph == outputsize);
    for (unsigned char const *ptr = frame, *end = frame + count;
            ptr != end; ++ptr) {
        avg += *ptr;
        avg2 += *ptr * *ptr;
    }
    double var = (avg2 - avg * avg / count) / count;
    double sdev = sqrt(var);
    avg = avg / count;

    //  threshold at 1.8 sdev above average (todo: is that about right?)
    unsigned char thresh = (unsigned char)(avg + 1.8 * sdev);
    if ((thresh < avg) || (thresh > (255 + avg) / 2)) {
        //  if sdev ends up being wildly mis-behaved, revert to a 
        //  rule of thumb
        if (verbose) {
            fprintf(stderr, "avg %.1f sdev %.1f thresh %d; adjusting to %.1f\n",
                    avg, sdev, thresh, (255 + avg) / 2);
        }
        thresh = (255 + avg) / 2;
    }
    if (cl_temp == nullptr) {
        cl_temp = (unsigned char *)malloc(count);
    }
    unsigned char *dst = cl_temp;
    int nfound = 0;
    for (unsigned char const *ptr = frame, *end = frame + count; 
            ptr != end; ++ptr) {
        unsigned char c = *ptr;
        if (c <= thresh) {
            *dst = 0;
        } else  {
            *dst = 255;
            ++nfound;
        }
        ++dst;
    }
    if (verbose) {
        fprintf(stderr, "nfound=%d\n", nfound);
        stbi_write_png("thresh.png", opw, oph, 1, cl_temp, 0);
    }

    //  cluster the identified samples
    unsigned char next = 0;
    for (int r = 0; r != oph; ++r) {
        unsigned char *s = cl_temp + r * opw;
        unsigned char left = 0;
        unsigned char top = 0;
        for (int c = 0; c != opw; ++c) {
            if (*s) {
                if (r > 0) {
                    top = s[-opw];
                } else {
                    top = 0;
                }
                if (top) {
                    *s = top;
                    if (left && (left != top)) {
                        //  merge! this is a very slow implementation
                        //  memory of who-merges-with-who in a 256 element table
                        //  and a single pass at the end would be better
                        if (verbose) {
                            fprintf(stderr, "merging %ld pix from %d to %d\n",
                                    (long)(s - cl_temp), left, top);
                        }
                        for (unsigned char *q = cl_temp; q != s; ++q) {
                            if (*q == left) {
                                *q = top;
                            }
                        }
                    }
                } else if (left) {
                    *s = left;
                } else {
                    if (next == 254) {
                        fprintf(stderr, "OOPS! Too many clusters\n");
                    }
                    if (next < 255) {
                        ++next;
                    }
                    *s = next;
                }
                left = *s;
            } else {
                left = 0;
            }
            ++s;
        }
    }
    if (verbose) {
        histogram("clusters", opw, oph, cl_temp);
        write_false_color("fakecolor.png", opw, oph, cl_temp);
    }

    //  calculate statistics per cluster
    int counts[256] = { 0 };
    double xs[256] = { 0.0 };
    double ys[256] = { 0.0 };
    double xs2[256] = { 0.0 };
    double xys[256] = { 0.0 };
    double xmin[256] = { 0.0 };
    double xmax[256] = { 0.0 };
    double ymin[256] = { 0.0 };
    double ymax[256] = { 0.0 };
    double xsize[256] = { 0.0 };
    double ysize[256] = { 0.0 };
    for (int r = 0; r != oph; ++r) {
        unsigned char *p = cl_temp + r * opw;
        for (int c = 0; c != opw; ++c) {
            unsigned char ix = *p;
            if (ix) {
                double x = (c - opw/2);
                double y = (r - oph/2);
                if (counts[ix] == 0) {
                    xmin[ix] = xmax[ix] = x;
                    ymin[ix] = ymax[ix] = y;
                } else {
                    xmin[ix] = std::min(xmin[ix], x);
                    xmax[ix] = std::max(xmax[ix], x);
                    ymin[ix] = std::min(ymin[ix], y);
                    ymax[ix] = std::max(ymax[ix], y);
                }
                xs[ix] += x;
                ys[ix] += y;
                xs2[ix] += x * x;
                xys[ix] += x * y;
                counts[ix] += 1;
            }
            ++p;
        }
    }

    //  what constitutes a "big enough" cluster?
    int const cnt_thresh = 12;
    //  vote for whether to turn left/right
    double xavg[256] = { 0.0 };
    double yavg[256] = { 0.0 };
    //double lr_a[256] = { 0.0 };
    double lr_b[256] = { 0.0 };
    double votes = 0.0;
    double votecount = 0.0;
    for (int i = 0; i != 256; ++i) {
        xavg[i] = (counts[i] >= cnt_thresh) ? xs[i] / counts[i] : 0.0;
        yavg[i] = (counts[i] >= cnt_thresh) ? ys[i] / counts[i] : 0.0;
        xsize[i] = xmax[i] - xmin[i] + 1;
        ysize[i] = ymax[i] - ymin[i] + 1;
        counts[i] = (counts[i] >= cnt_thresh) ? counts[i] : 0;
        if (counts[i]) {
            // lr_a[i] = (ys[i] * xs2[i] - xs[i] * xys[i]) /
            //     (counts[i] * xs2[i] - xs[i] * xs[i]);
            double denominator = (counts[i] * xs2[i] - xs[i] * xs[i]);
            if (fabs(denominator) < 1e-5) {
                lr_b[i] = 1e10;
            } else {
                lr_b[i] = (counts[i] * xys[i] - xs[i] * ys[i]) / denominator;
            }
            double direction = lr_b[i];
            if (std::isnan(direction)) {
                fprintf(stderr, "direction NaN: %.1f i=%d, counts[i]=%d\n",
                        direction, i, counts[i]);
            }
            //  TODO: magic constant
            double power = counts[i] / (yavg[i] + oph/2 + 50.0);
            double vote = 0.0;
            double straight = atan2(-xavg[i], yavg[i]+65);  //  where is the horizon?
            render_vote(cl_temp, outputwidth, outputheight, 
                    xavg[i], yavg[i], straight, 2, 254);
            if (fabs(direction) < 1e-2) {
                //  horizontal line? Turn right.
                votes += 0.25 * power;
                votecount += power;
                vote = M_PI / 2;
            } else {
                if (fabs(direction) > 1e2) {
                    //  Too vertical to be numerically reliable, but we 
                    //  know that vertical on the left means turn left, 
                    //  and vice versa.
                    votes += xavg[i] / (opw/2) * power;
                    votecount += power;
                    vote = 0;
                } else {
                    //  horizontal, square-ish? Reduce power
                    //  TODO: magic constant
                    if (fabsf(direction) < 0.5 && fabsf(ysize[i]/xsize[i]-1.0) < 0.2) {
                        power *= 0.1;
                    }
                    direction = -atan(1.0 / direction);
                    render_vote(cl_temp, outputwidth, outputheight, 
                            xavg[i], yavg[i], direction, power, 253);
                    votes += (direction - straight) * power;
                    votecount += power;
                    vote = direction - straight;
                }
            }
            if (std::isnan(votes) || std::isnan(votecount)) {
                fprintf(stderr,
                        "NaN in votes %.1f / votecount %.1f at iteration %d in serial %d\n",
                        votes, votecount, i, serial);
            } else {
                render_vote(cl_temp, outputwidth, outputheight, 
                        xavg[i], yavg[i], vote, power, 255);
            }
        }
    }

    if (std::isnan(votes) || std::isnan(votecount)) {
        fprintf(stderr, "Serial %d, NaN voting!\n", serial);
        goto dump_stuff;
    }
    if (votecount <= 0.01) {
        fprintf(stderr, "Serial %d: no clusters detected?\n", serial);
dump_stuff:
        char name[200];
        sprintf(name, "png_test/no_cluster_%d_output.png", serial);
        write_false_color(name, opw, oph, cl_temp);
        sprintf(name, "png_test/no_cluster_%d_input.png", serial);
        stbi_write_png(name, opw, oph, 1, frame, 0);
    }
    label[0] = (votecount > 0.01) ? (votes / votecount) : 0.25;
    label[1] = 0.5 / (fabs(label[0]) + 0.5);
    if (std::isnan(label[0]) || std::isnan(label[1])) {
        fprintf(stderr, "NaN in compute_labels_cv()!\n");
        abort();
    }
}

void database_frame(
        int serial,
        int width, int height,
        void const *y, int ylen,
        void const *u, int ulen,
        void const *v, int vlen,
        float steer, float throttle)
{
    if (ylen != 640 || ulen != 320 || vlen != 320) {
        fprintf(stderr, "sorry, database only works with 640x480 I420 format input frames\n");
        exit(1);
    }
    if (serial < skip) {
    }
    float mat[6] = { 1, 0, 0, 0, 1, 0 };
    unwarp_transformed_bytes(y, u, v, mat, outputframe);
    float label[2] = { steer, throttle };
    if (computelabels) {
        compute_labels_cv(serial, y, outputframe, label);
    }
    if (devmode) {
        magically_fix_labeling(serial, outputframe, label);
    }
    bool record_this = true;
    if (label[0] < -1.0f || label[0] > 1.0f || label[1] < 0.01f) {
        record_this = false;
    }
    if (label[0] < minSteer) {
        minSteer = label[0];
    }
    if (label[0] > maxSteer) {
        maxSteer = label[0];
    }
    if (label[0] < minThrottle) {
        minThrottle = label[0];
    }
    if (label[0] > maxThrottle) {
        maxThrottle = label[0];
    }
    label[0] *= scaleSteer;
    label[1] *= scaleThrottle;
    if (fabsf(label[0]) < 0.01) {
        label[0] = 0;
    }
    if (fabsf(label[0]) > 10) {
        label[0] = (label[0] > 0) ? 10 : -10;
    }
    if (label[1] < 0.1) {
        label[1] = 0.1;
    }
    if (record_this) {
        put_to_database(outputframe, label);
        ++numWritten;
        for (int i = 0; i != stretchData; ++i) {
            m2_rotation(stretch_random() * 2 * stretchRotate - stretchRotate, mat);
            mat[2] = stretch_random() * 2 * stretchOffset - stretchOffset;
            mat[5] = stretch_random() * 2 * stretchOffset - stretchOffset;
            unwarp_transformed_bytes(y, u, v, mat, outputframe);
            put_to_database(outputframe, label);
            ++numWritten;
            if (devmode || dumppng) {
                if (!(serial & 127)) {
                    mkdir ("png_test", 0777);
                    char name[100];
                    sprintf(name, "png_test/%06d_%d_%.2f_%.2f.png", serial, i, label[0], label[1]);
                    stbi_write_png(name, outputwidth, outputheight, 1,
                            (unsigned char const *)outputframe, 0);
                    if (computelabels && i == 0) {
                        sprintf(name, "png_test/%06d_%d_%.2f_%.2f_cluster.png", serial, i, label[0], label[1]);
                        write_false_color(name, outputwidth, outputheight, (unsigned char const *)cl_temp);
                    }
                }
            }
        }
    } else {
        numSkipped += 1;
    }
}

unsigned char *fakeframe;


void line(unsigned char *frame, int width, int height, float x1, float y1, float x2, float y2, unsigned char val) {
    float d = sqrtf((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
    float n = ceilf(d * 100);
    for (float i = 0; i != n; i += 1) {
        int x = floor((x1 + (x2-x1)*i/n) * outputwidth * 0.5f + outputwidth * 0.5f);
        int y = floor((y1 + (y2-y1)*i/n) * outputheight);
        if (x >= 0 && x < outputwidth && y >= 0 && y < outputheight) {
            frame[x + (outputheight-y-1)*outputwidth] = val;
        }
    }
}

void plot_road(unsigned char *frame, float x, float y, float dir, int i) {
    float s = sinf(dir);
    float c = cosf(dir);
    if (i & 8) {
        line(frame+outputwidth*outputheight, outputwidth, outputheight,
                x-0.05f*c, y+0.05f*s, x+0.05f*c, y-0.05f*s, 200);
    }
    line(frame+outputwidth*outputheight, outputwidth, outputheight, 
            x+0.5f*c, y-0.45f*s, x+0.45f*c, y-0.5f*s, 100);
    line(frame+outputwidth*outputheight, outputwidth, outputheight, 
            x-0.45f*c, y+0.5f*s, x-0.5f*c, y+0.45f*s, 100);
    if (i & 8) {
        line(frame, outputwidth, outputheight,
                x-0.05f*c, y+0.05f*s, x+0.05f*c, y-0.05f*s, 150);
    }
    line(frame, outputwidth, outputheight, 
            x+0.5f*c, y-0.45f*s, x+0.45f*c, y-0.5f*s, 200);
    line(frame, outputwidth, outputheight, 
            x-0.45f*c, y+0.5f*s, x-0.5f*c, y+0.45f*s, 200);
}

void fake_database_frame(int frameno) {
    if (frameno < skip) {
        return;
    }
    float x = stretch_random() - 0.5f;
    float steer = stretch_random() * 2 - 1;
    float throttle = fabsf(steer) > 0.5f ? 1.2f - fabsf(steer) : 1.0f;
    //steer = 0;
    //throttle = 1;
    if (!fakeframe) {
        fakeframe = (unsigned char *)malloc(outputwidth * outputheight * outputplanes);
    }
    memset(fakeframe, 0, outputwidth * outputheight * outputplanes);
    float dir = 0.0f;
    float y = 0.0f;
    for (int i = 0; i < 100; ++i) {
        plot_road(fakeframe, x, y, dir, i+frameno);
        dir = dir + steer * 0.02f;
        float s = sinf(dir);
        float c = cosf(dir);
        x = x + s * 0.017f;
        y = y + c * 0.017f;
    }
    float label[2] = { steer, throttle };
    put_to_database(fakeframe, label);
    if (!(frameno & 127)) {
        mkdir("png_test", 0777);
        char name[100];
        sprintf(name, "png_test/%06d_%.3f_%.3f.png", frameno, steer, throttle);
        stbi_write_png(name, outputwidth, outputheight, 1, fakeframe, 0);
    }
}

bool generate_fake_dataset(char const *output) {
    begin_database_write(output);
    for (int i = 0; i != numfakeframes; ++i) {
        fake_database_frame(i);
    }
    end_database_write();
    return true;
}

bool generate_dataset(char const *output) {
    if (verbose) {
        av_log_set_level(99);
    }
    avcodec_register_all();
    AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "avcodec_find_decoder(): h264 not found\n");
        return false;
    }
    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        fprintf(stderr, "avcodec_alloc_context3(): failed to allocate\n");
        return false;
    }
    ctx->flags2 |= AV_CODEC_FLAG2_CHUNKS;
    if (avcodec_open2(ctx, codec, NULL) < 0) {
        fprintf(stderr, "avcodec_open2(): failed to open\n");
        return false;
    }
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "av_frame_alloc(): alloc failed\n");
        return false;
    }
    AVCodecParserContext *parser = av_parser_init(AV_CODEC_ID_H264);
    if (!parser) {
        fprintf(stderr, "av_parser_init(): h264 failed\n");
        return false;
    }
    AVPacket avp = { 0 };
    av_init_packet(&avp);

    begin_database_write(output);

    //  loop
    auto curpdts = pdtsChunks.begin(), endpdts = pdtsChunks.end();
    auto curtime = timeChunks.begin(), endtime = timeChunks.end();
    auto curh264 = h264Chunks.begin(), endh264 = h264Chunks.end();
    throttle_steer ts = { 0 };
    pts_dts pd = { 0 };
    int frameno = 0;
    uint64_t ptsbase = 0;
    uint64_t dtsbase = 0;
    std::vector<unsigned char> readBuf;
    std::vector<unsigned char> tsbuf;
    uint64_t timeStart = 0;
    int nprogress = 1;
    readBuf.resize(100);
    while (curpdts != endpdts && curtime != endtime && curh264 != endh264) {
        while (curtime != endtime &&
                curtime->concatoffset < curpdts->concatoffset) {
            if (curtime->size >= 6) {
                fseek(curtime->file, curtime->fileoffset, 0);
                tsbuf.resize(curtime->size);
                q += fread(&tsbuf[0], 1, curtime->size, curtime->file);
                steer_packet sp = { 0 };
                uint64_t timeNow;
                memcpy(&timeNow, &tsbuf[0], 8);
                if (!timeStart) {
                    timeStart = timeNow;
                }
                for (size_t offset = 8; offset <= curtime->size-6;) {
                    memcpy(&sp, &tsbuf[offset], 6);
                    if (sp.code == 'S') {
                        //  steercontrol
                        ts.throttle = sp.throttle / 16383.0f;
                        ts.steer = sp.steer / 16383.0f;
                        if (verbose) {
                            fprintf(stderr, "now=%.3f throttle=%.2f steer=%.2f\n",
                                    (timeNow-timeStart) * 1e-6, ts.throttle, ts.steer);
                        }
                        offset += 6;
                    } else if (sp.code == 'T') {
                        //  triminfo
                        offset += 10;
                    } else if (sp.code == 'i') {
                        //  ibus
                        offset += 22;
                    } else {
                        if (verbose) {
                            fprintf(stderr, "unknown metadata type 0x%04x\n", sp.code);
                        }
                        break;
                    }
                }
            }
            ++curtime;
        }
        while (curpdts != endpdts &&
                curpdts->concatoffset < curh264->concatoffset) {
            fseek(curpdts->file, curpdts->fileoffset, 0);
            q += fread(&pd, 1, 16, curpdts->file);
            ++curpdts;
            if (frameno == 0) {
                ptsbase = pd.pts;
                dtsbase = pd.dts;
            }
        }
        while (curh264 != endh264 &&
                curh264->concatoffset < curtime->concatoffset &&
                curh264->concatoffset < curpdts->concatoffset) {
            fseek(curh264->file, curh264->fileoffset, 0);
            size_t readBufOffset = readBuf.size()-100;
            size_t actualSize = readBufOffset + curh264->size;
            readBuf.resize(actualSize + 100);
            q += fread(&readBuf[readBufOffset], 1, curh264->size, curh264->file);
            avp.pos = curh264->streamoffset;
parse_more:
            avp.data = NULL;
            avp.size = 0;
            avp.pts = pd.pts - ptsbase;
            avp.dts = pd.dts ? pd.dts - dtsbase : pd.pts - ptsbase;
            int lenParsed = av_parser_parse2(parser, ctx, &avp.data, &avp.size,
                    &readBuf[0], actualSize, avp.pts, avp.dts, avp.pos);
            if (verbose) {
                fprintf(stderr, "av_parser_parse2(): lenParsed %d size %d pointer %p readbuf 0x%p\n",
                        lenParsed, avp.size, avp.data, &readBuf[0]);
            }
            if (avp.size) {
                int lenSent = avcodec_send_packet(ctx, &avp);
                if (lenSent < 0) {
                    if (verbose) {
                        fprintf(stderr, "avcodec_send_packet(): error %d at concatoffset %ld\n",
                                lenSent, avp.pos);
                    }
                }
                readBuf.erase(readBuf.begin(), readBuf.begin()+lenParsed);
                int err = avcodec_receive_frame(ctx, frame);
                if (err == 0) {
                    //  got a frame!
                    ++frameno;
                    if (verbose) {
                        fprintf(stderr, "frame %d concatoffset %ld pts %ld steer %.2f drive %.2f format %d size %dx%d ptrs %p@%d %p@%d %p@%d\n",
                                frameno, curh264->concatoffset, pd.pts, ts.steer, ts.throttle,
                                frame->format, frame->width, frame->height,
                                frame->data[0], frame->linesize[0], frame->data[1], frame->linesize[1],
                                frame->data[2], frame->linesize[2]);
                    }
                    database_frame(
                            frameno,
                            frame->width, frame->height,
                            frame->data[0], frame->linesize[0],
                            frame->data[1], frame->linesize[1], 
                            frame->data[2], frame->linesize[2],
                            ts.steer, ts.throttle
                            );
                    if (ctx->refcounted_frames) {
                        av_frame_unref(frame);
                    }
                } else if (err == AVERROR(EAGAIN)) {
                    //  nothing for now
                } else if (err == AVERROR_EOF) {
                    //  nothing for now
                } else {
                    //  not a header
                    if (curh264->size > 128) {
                        if (verbose) {
                            fprintf(stderr, "avcodec_receive_frame() error %d concatoffset %ld\n",
                                    err, curh264->concatoffset);
                        }
                        // return false;
                    }
                }
                avp.pos += avp.size;
                goto parse_more;
            }
            readBuf.erase(readBuf.begin(), readBuf.begin()+lenParsed);
            ++curh264;
        }

        if (progress) {
            if (!--nprogress) {
                double fraction = double(curh264->concatoffset) / maxconcatoffset;
                fprintf(stderr, "%.30s: [%s%s] %6.2f%%\r",
                        get_filename(curh264->file),
                        "==================================>" + int((1-fraction)*35),
                        "                                   " + int(fraction*35),
                        fraction*100);
                nprogress = 20;
            }
        }
    }

    if (progress) {
        fprintf(stderr, "\n");
    }

    end_database_write();

    //  todo cleanup
    return true;
}

void parse_arguments(int argc, char const *argv[]) {

    if (argc == 1 || !strcmp(argv[1], "--help")) {
usage:
        fprintf(stderr, "usage: mktrain [options] file1.riff file2.riff ...\n");
        fprintf(stderr, "--dataset name.lmdb\n");
        fprintf(stderr, "--dump type:filename\n");
        fprintf(stderr, "--scale-steer 1.0\n");
        fprintf(stderr, "--scale-throttle 1.0\n");
        fprintf(stderr, "--skip-frames 0\n");
        fprintf(stderr, "--devmode\n");
        fprintf(stderr, "--dumppng\n");
        fprintf(stderr, "--fakedata\n");
        fprintf(stderr, "--computelabels\n");
        fprintf(stderr, "--quiet\n");
        fprintf(stderr, "--verbose\n");
        fprintf(stderr, "--output-as-streams\n");
        exit(1);
    }

    for (int i = 1; i != argc; ++i) {

        if (argv[i][0] == '-') {

            if (!strcmp(argv[i], "--dataset")) {
                ++i;
                if (!argv[i]) {
                    fprintf(stderr, "--dataset requires filename.lmdb\n");
                    exit(1);
                }
                char const *dot = strrchr(argv[i], '.');
                if (dot && !strcmp(dot, ".riff")) {
                    //  Is this an existing .riff file? Likely a typo, forgetting
                    //  to name the output dataset.
                    struct stat stbuf;
                    if (!stat(argv[i], &stbuf)) {
                        fprintf(stderr, "refusing to use an existing .riff file (%s) as output dataset\n",
                                argv[i]);
                        exit(1);
                    }
                }
                datasetName = argv[i];
                continue;
            }

            if (!strcmp(argv[i], "--dump")) {
                ++i;
                if (!argv[i]) {
                    fprintf(stderr, "--dump requires type:filename\n");
                    exit(1);
                }
                if (strlen(argv[i]) < 6 || argv[i][4] != ':') {
                    fprintf(stderr, "--dump bad argument: %s\n", argv[i]);
                    exit(1);
                }
                dump_chunk dc;
                memcpy(dc.type, argv[i], 4);
                dc.filename = &argv[i][5];
                dumpChunks.push_back(dc);
                continue;
            }

            if (!strcmp(argv[i], "--scale-steer")) {
                ++i;
                if (!argv[i]) {
                    fprintf(stderr, "--scale-steer requires scaling factor\n");
                    exit(1);
                }
                char *s;
                double sval = strtod(argv[i], &s);
                if (!s || sval < 1e-6 || sval > 1e6 || std::isnan(sval)) {
                    fprintf(stderr, "Invalid steer scale value: %f\n", sval);
                    exit(1);
                }
                scaleSteer = sval;
                continue;
            }

            if (!strcmp(argv[i], "--scale-throttle")) {
                ++i;
                if (!argv[i]) {
                    fprintf(stderr, "--scale-throttle requires scaling factor\n");
                    exit(1);
                }
                char *s;
                double sval = strtod(argv[i], &s);
                if (!s || sval < 1e-6 || sval > 1e6 || std::isnan(sval)) {
                    fprintf(stderr, "Invalid throttle scale value: %f\n", sval);
                    exit(1);
                }
                scaleThrottle = sval;
                continue;
            }

            if (!strcmp(argv[i], "--skip-frames")) {
                ++i;
                if (!argv[i]) {
                    fprintf(stderr, "--skip-frames requires scaling factor\n");
                    exit(1);
                }
                char *s;
                long sval = strtol(argv[i], &s, 10);
                if (sval < 0 || sval > 10000) {
                    fprintf(stderr, "Invalid skip frames count: %ld\n", sval);
                    exit(1);
                }
                skip = sval;
                continue;
            }

            if (!strcmp(argv[i], "--verbose")) {
                verbose = true;
                progress = false;
                continue;
            }

            if (!strcmp(argv[i], "--quiet")) {
                verbose = false;
                progress = false;
                continue;
            }

            if (!strcmp(argv[i], "--devmode")) {
                devmode = true;
                continue;
            }

            if (!strcmp(argv[i], "--dumppng")) {
                dumppng = true;
                continue;
            }

            if (!strcmp(argv[i], "--fakedata")) {
                fakedata = true;
                continue;
            }

            if (!strcmp(argv[i], "--computelabels")) {
                computelabels = true;
                mkdir("png_test", 0777);
                continue;
            }

            if (!strcmp(argv[i], "--output-as-streams")) {
                output_as_streams = true;
                continue;
            }

            fprintf(stderr, "unknown argument: '%s'\n", argv[i]);
            goto usage;
        } else {
            filenameArgs.push_back(argv[i]);
        }
    }

    if (devmode && computelabels) {
        fprintf(stderr, "--devmode and --computelabels are mutually exclusive\n");
        goto usage;
    }
    if (!filenameArgs.size()) {
        if (!fakedata) {
            fprintf(stderr, "no input files specified -- do you need --fakedata?\n");
            goto usage;
        }
    } else if (fakedata) {
        fprintf(stderr, "can't use both input file names and --fakedata at the same time\n");
        goto usage;
    }
}

FILE *open_check(char const *cpath, long *len) {

    FILE *f = fopen(cpath, "rb");
    if (!f) {
        perror(cpath);
        exit(1);
    }

    check_header(f, cpath);

    fseek(f, 0, 2);
    *len = ftell(f);
    fseek(f, 12, 0);

    if (verbose) {
        fprintf(stdout, "%s: %ld bytes\n", cpath, *len);
    }
    fileNames[f] = cpath;

    return f;
}


void slurp_files() {

    uint64_t h264Offset = 0;
    uint64_t pdtsOffset = 0;
    uint64_t timeOffset = 0;
    uint64_t concatoffset = 0;

    for (auto const &path : filenameArgs) {

        char const *cpath = path.c_str();
        long len = 0;

        if (progress) {
            char pathstr[50];
            char const *q = cpath;
            if (strlen(q) > 49) {
                q += strlen(q)-49;
            }
            sprintf(pathstr, "%.49s", q);
            fprintf(stderr, "reading: %49s\r", pathstr);
        }

        FILE *f = open_check(cpath, &len);

        while (!feof(f) && !ferror(f)) {

            long pos = ftell(f);

            chunk_header ch;
            long rd = fread(&ch, 1, 8, f);
            if (8 != rd) {
                if (rd != 0) {
                    perror("short read");
                }
                break;
            }

            stream_chunk sc;
            sc.file = f;
            sc.concatoffset = pos + concatoffset;
            sc.fileoffset = pos + 8;
            sc.streamoffset = 0;
            sc.size = ch.size;

            if (!strncmp(ch.type, "h264", 4)) {
                sc.streamoffset = h264Offset;
                h264Offset += sc.size;
                h264Chunks.push_back(sc);
            }
            else if (!strncmp(ch.type, "pdts", 4)) {
                sc.streamoffset = pdtsOffset;
                pdtsOffset += sc.size;
                pdtsChunks.push_back(sc);
            }
            else if (!strncmp(ch.type, "time", 4)) {
                sc.streamoffset = timeOffset;
                timeOffset += sc.size;
                timeChunks.push_back(sc);
            }

            fseek(f, (ch.size + 3) & ~3, 1);
        }

        concatoffset += ftell(f);
    }

    if (progress) {
        fprintf(stderr, "\n");
    }

    if (verbose) {
        fprintf(stdout, "%ld h264 chunks size %ld\n%ld pdts chunks size %ld\n%ld time chunks size %ld\n",
                h264Chunks.size(), h264Offset, pdtsChunks.size(), pdtsOffset, timeChunks.size(), timeOffset);
        fprintf(stdout, "total size %ld bytes over %ld files\n", concatoffset, filenameArgs.size());
    }

    maxconcatoffset = concatoffset;
}


namespace caffe2 {
    namespace db {
        class LMDBCursor : public Cursor {
            public:
                bool SupportsSeek();
        };
    }
}

int main(int argc, char const *argv[]) {

    struct timeval starttime = { 0 };
    gettimeofday(&starttime, 0);

    parse_arguments(argc, argv);

    slurp_files();

    int errors = 0;
    for (auto const &dc : dumpChunks) {
        if (!generate_requested_file(dc.type, dc.filename.c_str())) {
            fprintf(stderr, "Could not generate '%s' of type '%.4s'\n", dc.filename.c_str(), dc.type);
            ++errors;
        }
    }
    if (datasetName.size()) {
        if (fakedata) {
            if (!generate_fake_dataset(datasetName.c_str())) {
                fprintf(stderr, "Error generating fake dataset '%s'\n", datasetName.c_str());
                ++errors;
            }
        } else {
            if (!generate_dataset(datasetName.c_str())) {
                fprintf(stderr, "Error generating dataset '%s'\n", datasetName.c_str());
                ++errors;
            }
            if (progress || verbose) {
                fprintf(stderr, "steer input range: %.2f - %.2f\n", minSteer, maxSteer);
                fprintf(stderr, "throttle input range: %.2f - %.2f\n", minThrottle, maxThrottle);
                fprintf(stderr, "skipped %d frames for bad labeling; recorded %d total frames\n",
                        numSkipped, numWritten);
            }
        }
    }

    struct timeval endtime = { 0 };
    gettimeofday(&endtime, 0);

    double seconds = (endtime.tv_sec - starttime.tv_sec) +
        (endtime.tv_usec - starttime.tv_usec) * 1e-6;
    if (progress || verbose) {
        fprintf(stderr, "%s in %dh %dm %.1fs\n",
                errors ? "errorored" : "completed",
                int(seconds)/3600, int(seconds)/60%60, fmod(seconds, 60));
    }

    return errors;/* || !&caffe2::db::LMDBCursor::SupportsSeek;*/
}
