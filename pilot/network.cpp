#include "network.h"
#include "queue.h"
#include "image.h"  //  for warp size
#include "pipeline.h"
#include "crunk.h"
#include <assert.h>
#include <unistd.h>

#include <caffe2/core/db.h>
#include <caffe2/core/operator.h>
#include <caffe2/core/workspace.h>
#include <caffe2/utils/proto_utils.h>
#include <caffe2/proto/caffe2.pb.h>
#include <caffe2/operators/conv_op.h>
#include <caffe2/operators/fully_connected_op.h>
#include <caffe2/operators/pool_op.h>
#include <caffe2/operators/relu_op.h>


static FrameQueue *networkInput;
static Pipeline *networkPipeline;
bool gNetworkFailed;

using namespace caffe2;

Workspace gWorkspace[3];

static void process_network(Pipeline *, Frame *&src, Frame *&dst, void *, int index) {
    //  do the thing!
    usleep(25000);
    if (dst) {
        //  the other queue will be responsible for this buffer
        dst->link(src);
        src = NULL;
    }
}

std::map<std::string, float *> networkBlobs;

bool load_network_db(char const *name) {
    char line[1024];
    try {
        fprintf(stderr, "loading neural network %s\n", name);
        FILE *f = fopen(name, "rb");
        if (!f) {
            fprintf(stderr, "could not open neural network file: %s\n", name);
            return false;
        }
        fgets(line, 1024, f);
        if (strcmp(line, "crunk 1.0\n")) {
            fprintf(stderr, "%s: not a crunk file\n", name);
            fclose(f);
            return false;
        }
        std::string info;
        std::string key;
        std::string colon_net = ":NET";
        float *value;
        while (read_crunk_block(f, key, info, value)) {
            fprintf(stderr, "got block: %s\n", key.c_str());
            if (key == colon_net) {
                //  do nothing
                delete[] value;
            } else {
                networkBlobs[key] = value;
            }
        }
    } catch (std::exception const &x) {
        fprintf(stderr, "Could not load network %s: %s\n", name, x.what());
        return false;
    } catch (...) {
        fprintf(stderr, "Could not load network %s (unknown error)\n", name);
        return false;
    }
    fprintf(stderr, "loaded %d blocks; done loading %s\n", (int)networkBlobs.size(), name);
    return true;
}


OperatorBase *mkconv(Workspace *wks, char const *name, char const *input,
        int kernel, int stride, int dims_in, int dims_out) {
    OperatorDef def;
    def.add_input(input);
    def.add_input(std::string(name) + "_w");
    def.add_input(std::string(name) + "_b");
    def.add_output(name);
    def.set_name(name);
    def.set_type("Conv");
    AddArgument<int>("kernel", kernel, &def);
    AddArgument<int>("stride", stride, &def);
    AddArgument<int>("dim_in", dims_in, &def);
    AddArgument<int>("dim_out", dims_out, &def);
    return CreateOperator(def, wks).release();
}

OperatorBase *mkpool(Workspace *wks, char const *name, char const *input,
        int kernel, int stride, int dims) {
    OperatorDef def;
    def.add_input(input);
    def.add_output(name);
    def.set_name(name);
    def.set_type("MaxPool");
    AddArgument<int>("kernel", kernel, &def);
    AddArgument<int>("stride", stride, &def);
    AddArgument<int>("dims", dims, &def);
    return CreateOperator(def, wks).release();
}

OperatorBase *mkrelu(Workspace *wks, char const *name, char const *input,
        int dims) {
    OperatorDef def;
    def.add_input(input);
    def.add_output(name);
    def.set_name(name);
    def.set_type("Relu");
    AddArgument<int>("dims", dims, &def);
    return CreateOperator(def, wks).release();
}

OperatorBase *mkfc(Workspace *wks, char const *name, char const *input,
        int dims_in, int dims_out) {
    OperatorDef def;
    def.add_input(input);
    def.add_input(std::string(name) + "_w");
    def.add_input(std::string(name) + "_b");
    def.add_output(name);
    def.set_name(name);
    def.set_type("FC");
    return CreateOperator(def, wks).release();
}

bool instantiate_network(Workspace *wks, Blob *&input, Blob *&output) {
    for (auto const kv : networkBlobs) {
        Blob *bb = wks->CreateBlob(kv.first);
        bb->ShareExternal<float>((float *)kv.second);
    }
    input = wks->CreateBlob("input");
    input->Reset((float *)new float[2*59*249]);
    output = wks->CreateBlob("output");
    output->Reset((float *)new float[2]);

#define CONV(n, i, k, s, di, dd) \
    mkconv(wks, n, i, k, s, di, dd)
#define POOL(n, i, k, s, d) \
    mkpool(wks, n, i, k, s, d)
#define RELU(n, i, d) \
    mkrelu(wks, n, i, d)
#define FC(n, i, di, dd) \
    mkfc(wks, n, i, di, dd)

#if 0

    /*
    // This is a LeNet-like model
    //
    def AddNetModel_2(model, data):
        # 182x70x1 -> 90x34x3
        conv1 = brew.conv(model, data, 'conv1', dim_in=1, dim_out=3, kernel=3)
        pool1 = brew.max_pool(model, conv1, 'pool1', kernel=2, stride=2)
        # 90x34x3 -> 44x16x5
        conv2 = brew.conv(model, pool1, 'conv2', dim_in=3, dim_out=5, kernel=3)
        pool2 = brew.max_pool(model, conv2, 'pool2', kernel=2, stride=2)
        # 44x16x5 -> 21x7x7
        conv3 = brew.conv(model, pool2, 'conv3', dim_in=5, dim_out=7, kernel=3)
        pool3 = brew.max_pool(model, conv3, 'pool3', kernel=2, stride=2)
        relu4 = brew.relu(model, pool3, 'relu4')
        # 21x7x7 -> 2
        fc5 = brew.fc(model, relu4, 'fc5', dim_in=21*7*7, dim_out=16)
        relu5 = brew.relu(model, fc5, 'relu5')
        output = brew.fc(model, relu5, 'output', dim_in=16, dim_out=2)
        return output
    */
#endif

    CONV("conv1", "input", 3, 1, 1, 3);
    POOL("pool1", "conv1", 2, 2, 3);
    CONV("conv2", "pool1", 3, 1, 3, 5);
    POOL("pool2", "conv2", 2, 2, 5);
    CONV("conv3", "pool2", 3, 1, 5, 7);
    POOL("pool3", "conv3", 2, 2, 7);
    RELU("relu4", "pool3", 7);
    FC  ("fc5",   "relu4", 21*7*7, 16);
    RELU("relu5", "fc5",   16);
    FC  ("output","relu5", 16, 2);

    return true;
}



bool load_network(char const *name, FrameQueue *output) {
    if (!networkInput) {
        size_t size;
        int width, height, planes;
        get_unwarp_info(&size, &width, &height, &planes);
        assert(planes == 1);
        assert(width == 182);
        assert(height == 70);

        if (!load_network_db(name)) {
            gNetworkFailed = true;
        } else {
            for (int i = 0; i != 3; ++i) {
                Blob *input = nullptr, *output = nullptr;
                if (!instantiate_network(&gWorkspace[i], input, output)) {
                    fprintf(stderr, "Could not create network index %d\n", i);
                    gNetworkFailed = true;
                }
            }
        }

        networkInput = new FrameQueue(5, size, width, height, 8);
        networkPipeline = new Pipeline(process_network);
        networkPipeline->connectInput(networkInput);
        networkPipeline->connectOutput(output);
    }
    return true;
}

FrameQueue *network_input_queue() {
    assert(networkInput != NULL);
    return networkInput;
}

void network_start() {
    networkPipeline->start(NULL, 3);
}

void network_stop() {
    if (networkPipeline) {
        networkPipeline->stop();
    }
}

