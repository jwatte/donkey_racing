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
#include <math.h>

#include <list>
#include <string>
#include <vector>
#include <map>
#include <random>

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
int inputfd = 0;

//int outputwidth = 182, outputheight = 70, outputplanes = 1;
int outputwidth = 146, outputheight = 146, outputplanes = 1;
int outputsize;
unsigned char *outputframe;

static caffe2::TensorProtos protos;
static caffe2::TensorProto *dataProto;
static caffe2::TensorProto *labelProto;
static int databaseKeyIndex;
std::unique_ptr<caffe2::db::DB> test_db;
std::unique_ptr<caffe2::db::Transaction> testTransaction;

static bool exists(char const *path) {
    struct stat st;
    if (!lstat(path, &st)) {
        return true;
    }
    return false;
}

char *wrdata;

void test_write_loop() {
    float label[2];
    char c;
    if (!wrdata) {
        wrdata = (char *)malloc(outputsize);
    }
    int testNumFrames = 0;
    while (true) {
        if (read(inputfd, &c, 1) != 1) {
            abort();
        }
        if (c == 'q') {
            break;
        }
        if (read(inputfd, &databaseKeyIndex, sizeof(databaseKeyIndex))
                != sizeof(databaseKeyIndex)) {
            abort();
        }
        if (read(inputfd, label, 8) != 8) {
            abort();
        }
        if (read(inputfd, wrdata, outputsize) != outputsize) {
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
    close(inputfd);
}

void begin_database_write(char const *output) {

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

    if (exists(output)) {
        fprintf(stderr, "%s: already exists, not overwriting\n", output);
        exit(1);
    }
    test_db = caffe2::db::CreateDB("lmdb", output, caffe2::db::NEW);
    if (!test_db) {
        fprintf(stderr, "create LMDB %s failed\n", output);
        exit(1);
    }
    testTransaction = test_db->NewTransaction();
    test_write_loop();
    exit(0);
}


int main(int argc, char const *argv[]) {
    if (argc != 2 || argv[1][0] == '-') {
        fprintf(stderr, "usage: mkdbfromstream output/path (reads from stdin)\n");
        exit(1);
    }
    begin_database_write(argv[1]);
    return 0;
}

