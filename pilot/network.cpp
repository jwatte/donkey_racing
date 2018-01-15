#include "network.h"
#include "queue.h"
#include "image.h"  //  for warp size
#include "pipeline.h"
#include "crunk.h"
#include "metrics.h"
#include "settings.h"
#include "serial.h"
#include "../stb/stb_image_write.h"
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include <caffe2/core/db.h>
#include <caffe2/core/operator.h>
#include <caffe2/core/workspace.h>
#include <caffe2/utils/proto_utils.h>
#include <caffe2/proto/caffe2.pb.h>
#include <caffe2/operators/conv_op.h>
#include <caffe2/operators/fully_connected_op.h>
#include <caffe2/operators/pool_op.h>
#include <caffe2/operators/relu_op.h>


#define NETWORK_VARIANT 8

#define PARALLEL_NETWORKS 2

static FrameQueue *networkInput;
static Pipeline *networkPipeline;
bool gNetworkFailed;

using namespace caffe2;

static bool running_status = true;
static Workspace gWorkspace[PARALLEL_NETWORKS];
static Blob *inputs[PARALLEL_NETWORKS];
static Blob *outputs[PARALLEL_NETWORKS];
static std::vector<OperatorBase *> nets[PARALLEL_NETWORKS];
static std::map<std::string, float *> networkBlobs;
static std::map<std::string, std::vector<TIndex>> blobShapes;
static uint64_t clocks[PARALLEL_NETWORKS];
static uint64_t starts[PARALLEL_NETWORKS];
static int counts[PARALLEL_NETWORKS];

float gSteerAdjust = 0.5f;
float gThrottleBase = 2.5f;
float gThrottleAdjust = 0.4f;

static float steer_adjust(float steer) {
    steer = steer * gSteerAdjust;
    return (steer > 1.0f) ? 1.0f : (steer < -1.0f) ? -1.0f : steer;
}

static float throttle_adjust(float throttle) {
    throttle = (throttle - gThrottleBase) * gThrottleAdjust;
    return (throttle > 1.0f) ? 1.0f : (throttle < 0.0f) ? 0.0f : throttle;
}


static void process_network(Pipeline *, Frame *&src, Frame *&dst, void *, int index) {
    assert(index >= 0 && index < PARALLEL_NETWORKS);
    uint64_t start = metric::Collector::clock();
    //  do the thing!
    inputs[index]->GetMutable<Tensor<CPUContext>>()->ShareExternalPointer((float *)src->data_);
    for (auto ptr : nets[index]) {
        ptr->Run();
    }
    inputs[index]->GetMutable<Tensor<CPUContext>>()->FreeMemory();
    float const *output = (float const *)outputs[index]->Get<Tensor<CPUContext>>().data<float>();
    if (running_status && !((counts[index] + index * 10) & 31))
    {
        fprintf(stderr, "\rNet index %1d: steer %8.2f throttle %8.2f  ", index, output[0], output[1]);
    }
    serial_steer(steer_adjust(output[0]), throttle_adjust(output[1]));
    if (dst) {
        //  the other queue will be responsible for this buffer
        dst->link(src);
        src = NULL;
    }
    uint64_t end = metric::Collector::clock();
    clocks[index] += end - start;
    counts[index] += 1;
    if ((counts[index] == 50) || (clocks[index] > 1000000)) {
        double fps = counts[index] / (end - starts[index]);
        double msper = clocks[index] / (1000.0 * counts[index]);
        //fprintf(stderr, "\nindex %d avgtime %.1f ms\n", index, msper);
        Process_NetFps.sample(fps * PARALLEL_NETWORKS);
        Process_NetDuration.sample(msper);
        counts[index] = 0;
        clocks[index] = 0;
        starts[index] = end;
    }
    Process_NetBuffers.increment();
}

bool parse_shape(std::string const &info, std::vector<TIndex> &shape) {
    int s[4] = { 0 };
    int n = sscanf(info.c_str(), "( %d , %d , %d , %d", &s[0], &s[1], &s[2], &s[3]);
    if (n < 1) {
        return false;
    }
    shape.insert(shape.begin(), &s[0], &s[n]);
    return true;
}

bool load_network_db(char const *name) {
    char line[1024];
    try {
        fprintf(stderr, "loading neural network %s\n", name);
        FILE *f = fopen(name, "rb");
        if (!f) {
            fprintf(stderr, "could not open neural network file: %s\n", name);
            return false;
        }
        if (!fgets(line, 1024, f) ||
                strcmp(line, "crunk 1.0\n")) {
            fprintf(stderr, "%s: not a crunk file\n", name);
            fclose(f);
            return false;
        }
        std::string info;
        std::string key;
        std::string colon_net = ":NET";
        float *value;
        while (read_crunk_block(f, key, info, value)) {
            if (key == colon_net) {
                //  do nothing
                delete[] value;
            } else {
                if (!parse_shape(info, blobShapes[key])) {
                    fprintf(stderr, "Shape %s could not be parsed\n", info.c_str());
                    delete[] value;
                    return false;
                } else {
                    networkBlobs[key] = value;
                    fprintf(stderr, "layer %s %s", key.c_str(), info.c_str());
                }
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

bool instantiate_network(Workspace *wks, Blob *&input1, Blob *&output, std::vector<OperatorBase *> &net) {
    for (auto const kv : networkBlobs) {
        fprintf(stderr, "creating blob %s\n", kv.first.c_str());
        Blob *bb = wks->CreateBlob(kv.first);
        Tensor<CPUContext> *t = new Tensor<CPUContext>(blobShapes[kv.first]);
        t->ShareExternalPointer((float *)kv.second);
        bb->Reset(t);
    }

    vector<TIndex> sz;

    size_t w_sz = 0;
    int w_width = 0;
    int w_height = 0;
    int w_planes = 0;
    get_unwarp_info(&w_sz, &w_width, &w_height, &w_planes);

    input1 = wks->CreateBlob("input1");
    sz.push_back(1);
    sz.push_back(w_planes);
    sz.push_back(w_height);
    sz.push_back(w_width);
    Tensor<CPUContext> *it = new Tensor<CPUContext>(sz);
    input1->Reset(it);

    output = wks->CreateBlob("output");
    sz.clear();
    sz.push_back(1);
    sz.push_back(2);
    Tensor<CPUContext> *ot = new Tensor<CPUContext>(sz);
    output->Reset(ot);

#define CONV(n, i, k, s, di, dd) \
    net.push_back(mkconv(wks, n, i, k, s, di, dd))
#define POOL(n, i, k, s, d) \
    net.push_back(mkpool(wks, n, i, k, s, d))
#define RELU(n, i, d) \
    net.push_back(mkrelu(wks, n, i, d))
#define FC(n, i, di, dd) \
    net.push_back(mkfc(wks, n, i, di, dd))

    bool ret = false;

#if NETWORK_VARIANT == 2
    assert(!ret);
    ret = true;
    //  LeNet-like model
    CONV("conv1", "input1", 3, 1, 1, 3);
    POOL("pool1", "conv1", 2, 2, 3);
    CONV("conv2", "pool1", 3, 1, 3, 5);
    POOL("pool2", "conv2", 2, 2, 5);
    CONV("conv3", "pool2", 3, 1, 5, 7);
    POOL("pool3", "conv3", 2, 2, 7);
    RELU("relu4", "pool3", 7);
    FC  ("fc5",   "relu4", 21*7*7, 16);
    RELU("relu5", "fc5",   16);
    FC  ("output","relu5", 16, 2);
#endif
#if NETWORK_VARIANT == 3
    assert(!ret);
    ret = true;
    //  LeNet-like model
    CONV("conv1", "input1", 3, 1, 1, 6);
    POOL("pool1", "conv1", 2, 2, 6);
    CONV("conv2", "pool1", 3, 1, 3, 8);
    POOL("pool2", "conv2", 2, 2, 8);
    CONV("conv3", "pool2", 3, 1, 5, 10);
    POOL("pool3", "conv3", 2, 2, 10);
    RELU("relu4", "pool3", 10);
    FC  ("fc5",   "relu4", 21*7*10, 32);
    RELU("relu5", "fc5",   32);
    FC  ("output","relu5", 32, 2);
#endif
#if NETWORK_VARIANT == 6
    assert(!ret);
    ret = true;
    //  Some LeNet / SqueezeNet / WatteNet hybrid
    CONV("conv2", "input1", 3, 1, 1, 8);
    RELU("relu3", "conv2", 8);
    //  Replace "squeeze" and "max_pool" layers with "relu" and "convolve/squeeze."
    //  The end result is that I have 2x2 pixels in, each with 8 outputs from a 3x3 convolution.
    //  I end up collapsing that down to a 4-output by doing Relu and 2x2x8->4 convolution.
    //  There's a problem here in that images off by one pixel may end up mattering. Maybe a
    //  better way would be to squeeze 1x1x8->4 and then max-pool?
    CONV("conv4", "relu3", 2, 2, 8, 4);
    CONV("conv5", "conv4", 5, 1, 4, 16);
    RELU("relu6", "conv5", 16);
    //  Squeeze 64->8
    CONV("conv7", "relu6", 2, 2, 16, 8);
    CONV("conv8", "conv7", 3, 1, 8, 32);
    RELU("relu9", "conv8", 32);
    //  Squeeze 128->16
    CONV("conv10", "relu9", 2, 2, 32, 16);
    CONV("conv11", "conv10", 3, 1, 16, 64);
    RELU("relu12", "conv11", 64);
    //  Squeeze 256->32
    CONV("conv13", "relu12", 2, 2, 64, 32);
    RELU("relu14", "conv13", 32);
    //  fully connected 7x7x32->192
    FC  ("fc15", "relu14", 1568, 192);
    RELU("relu16", "fc15", 192);
    //  fully connected 192->2
    FC  ("output", "relu16", 192, 2);
    fprintf(stderr, "created model 6\n");
#endif
#if NETWORK_VARIANT == 8
    assert(!ret);
    ret = true;
    //  Updated LeNet / SqueezeNet / MobileNet / WatteNet hybrid
    CONV("conv2", "input1", 3, 1, 3, 8);
    POOL("pool3", "conv2", 2, 2, 8);
    CONV("conv4", "pool3", 3, 1, 8, 16);
    RELU("relu5", "conv4", 16);
    CONV("conv6", "relu5", 1, 1, 16, 8);
    POOL("pool7", "conv6", 2, 2, 8);
    CONV("conv8", "pool7", 3, 1, 8, 32);
    RELU("relu9", "conv8", 32);
    CONV("conv10", "relu9", 1, 1, 32, 8);
    POOL("pool11", "conv10", 2, 2, 8);
    CONV("conv12", "pool11", 3, 1, 8, 64);
    RELU("relu13", "conv12", 64);
    CONV("conv14", "relu13", 1, 1, 64, 32);
    RELU("relu15", "conv14", 32);
    FC  ("fc16", "relu15", 1920, 128);
    RELU("relu17", "fc16", 128);
    FC  ("output", "relu17", 128, 2);
    fprintf(stderr, "created model 8\n");
#endif

    return ret;
}



bool load_network(char const *name, FrameQueue *output) {
    running_status = get_setting_int("running_status", 1);
    gThrottleBase = get_setting_float("net_throttle_base", gThrottleBase);
    gThrottleAdjust = get_setting_float("net_throttle_Adjust", gThrottleAdjust);
    gSteerAdjust = get_setting_float("net_steer_adjust", gSteerAdjust);
    if (!networkInput) {
        size_t size;
        int width, height, planes;
        get_unwarp_info(&size, &width, &height, &planes);
        assert(planes == 1);

        if (!load_network_db(name)) {
            gNetworkFailed = true;
        } else {
            for (int i = 0; i != PARALLEL_NETWORKS; ++i) {
                if (!instantiate_network(&gWorkspace[i], inputs[i], outputs[i], nets[i])) {
                    fprintf(stderr, "Could not create network index %d\n", i);
                    gNetworkFailed = true;
                }
            }
        }

        networkInput = new FrameQueue(5, size, width, height, 4);
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
    networkPipeline->start(NULL, PARALLEL_NETWORKS);
}

void network_stop() {
    if (networkPipeline) {
        networkPipeline->stop();
    }
}

