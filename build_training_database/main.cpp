#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

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
float stretchRotate = 0.03f;
float stretchOffset = 3.0f;
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
static int testNumFrames;
static int trainNumFrames;
std::unique_ptr<caffe2::db::DB> train_db;
std::unique_ptr<caffe2::db::DB> test_db;
std::unique_ptr<caffe2::db::Transaction> trainTransaction;
std::unique_ptr<caffe2::db::Transaction> testTransaction;

void begin_database_write(char const *output) {

    get_unwarp_info(&outputsize, &outputwidth, &outputheight, &outputplanes);
    outputsize = outputwidth * outputheight * outputplanes;
    outputframe = (unsigned char *)malloc(outputsize);

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
    std::string path(output);
    path += "/train";
    if (exists(path.c_str())) {
        fprintf(stderr, "%s: already exists, not overwriting\n", path.c_str());
        exit(1);
    }
    if (verbose) {
        fprintf(stderr, "Creating training database as '%s'\n", path.c_str());
    }
    train_db = caffe2::db::CreateDB("lmdb", path, caffe2::db::NEW);
    trainTransaction = train_db->NewTransaction();

    path = output;
    path += "/test";
    if (exists(path.c_str())) {
        fprintf(stderr, "%s: already exists, not overwriting\n", path.c_str());
        exit(1);
    }
    if (verbose) {
        fprintf(stderr, "Creating test database as '%s'\n", path.c_str());
    }
    test_db = caffe2::db::CreateDB("lmdb", path, caffe2::db::NEW);
    testTransaction = test_db->NewTransaction();
}

void end_database_write() {
    if (verbose) {
        fprintf(stderr, "closing databases\n");
    }
    trainTransaction->Commit();
    trainTransaction.reset();
    testTransaction->Commit();
    testTransaction.reset();
    train_db.reset();
    test_db.reset();
}

void put_to_database(unsigned char const *frame, float const *label) {
    dataProto->set_byte_data(frame, outputsize);
    labelProto->set_float_data(0, label[0]);
    labelProto->set_float_data(1, label[1]);

    char dki[12];
    sprintf(dki, "%08d", databaseKeyIndex);
    ++databaseKeyIndex;
    std::string keyname(dki);

    std::string value;
    protos.SerializeToString(&value);

    if (stretch_random() < testFraction) {
        testTransaction->Put(keyname, value);
        ++testNumFrames;
        if (!(testNumFrames & 511)) {
            if (verbose) {
                fprintf(stderr, "partial commit of test database at %d items\n", testNumFrames);
            }
            testTransaction->Commit();
        }
    } else {
        trainTransaction->Put(keyname, value);
        ++trainNumFrames;
        if (!(trainNumFrames & 511)) {
            if (verbose) {
                fprintf(stderr, "partial commit of training database at %d items\n", trainNumFrames);
            }
            trainTransaction->Commit();
        }
    }
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
    if (label[1] > 10) {
        label[1] = 10;
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
                    mkdir ("test_png", 0777);
                    char name[100];
                    sprintf(name, "test_png/%06d_%d_%.2f_%.2f.png", serial, i, label[0], label[1]);
                    stbi_write_png(name, outputwidth, outputheight, 1,
                            (unsigned char const *)outputframe
                            + outputwidth * outputheight, 0);
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
        mkdir("test_png", 0777);
        char name[100];
        sprintf(name, "test_png/%06d_%.3f_%.3f.png", frameno, steer, throttle);
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
    while (curpdts != endpdts && curtime != endtime && curh264 != endh264) {
        while (curtime != endtime &&
                curtime->concatoffset < curpdts->concatoffset) {
            if (curtime->size >= 6) {
                fseek(curtime->file, curtime->fileoffset, 0);
                tsbuf.resize(curtime->size);
                fread(&tsbuf[0], 1, curtime->size, curtime->file);
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
            fread(&pd, 1, 16, curpdts->file);
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
            size_t readBufOffset = readBuf.size();
            readBuf.resize(readBufOffset + curh264->size);
            fread(&readBuf[readBufOffset], 1, curh264->size, curh264->file);
            avp.pos = curh264->streamoffset;
parse_more:
            avp.data = NULL;
            avp.size = 0;
            avp.pts = pd.pts - ptsbase;
            avp.dts = pd.dts ? pd.dts - dtsbase : pd.pts - ptsbase;
            int lenParsed = av_parser_parse2(parser, ctx, &avp.data, &avp.size,
                    &readBuf[0], readBuf.size(), avp.pts, avp.dts, avp.pos);
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
        fprintf(stderr, "--quiet\n");
        fprintf(stderr, "--verbose\n");
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

            fprintf(stderr, "unknown argument: '%s'\n", argv[i]);
            goto usage;
        } else {
            filenameArgs.push_back(argv[i]);
        }
    }

    if (!filenameArgs.size()) {
        if (!fakedata) {
            fprintf(stderr, "no input files specified -- did you need --fakedata?\n");
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

    return errors;
}
