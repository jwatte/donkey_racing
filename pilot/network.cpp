#include "network.h"
#include "queue.h"
#include "image.h"  //  for warp size
#include "pipeline.h"
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

caffe2::Workspace gWorkspace[3];

static void process_network(Pipeline *, Frame *&src, Frame *&dst, void *, int index) {
    //  do the thing!
    usleep(25000);
    if (dst) {
        //  the other queue will be responsible for this buffer
        dst->link(src);
        src = NULL;
    }
}

#if 0
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
        if (!strcmp(line, "crunk 1.0\n")) {
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
    return true;
}

bool instantiate_network(Workspace *wks, Blob *&input, Blob *&output) {
    // conv1_w: (8, 2, 4, 4)
    // conv1_b: (8,)
    // conv2_w: (16, 8, 4, 4)
    // conv2_b: (16,)
    // conv3_w: (32, 16, 4, 4)
    // conv3_b: (32,)
    // fc4_w: (128, 320)
    // fc4_b: (128,)
    // output_w: (2, 128)
    // output_b: (2,)
/*
# 149x59 -> 146x56                                                                                                  
conv1 = brew.conv(model, data, 'conv1', dim_in=2, dim_out=8, kernel=4)                                              
# 146x56 -> 73x28                                                                                                   
pool1 = brew.max_pool(model, conv1, 'pool1', kernel=2, stride=2)                                                    
relu1 = brew.relu(model, pool1, 'relu1')                                                                            
# 73x28 -> 70x25                                                                                                    
conv2 = brew.conv(model, relu1, 'conv2', dim_in=8, dim_out=16, kernel=4)                                            
# 70x25 -> 14x5                                                                                                     
pool2 = brew.max_pool(model, conv2, 'pool2', kernel=5, stride=5)                                                    
relu2 = brew.relu(model, pool2, 'relu2')                                                                            
# 14x5 -> 11x2                                                                                                      
conv3 = brew.conv(model, relu2, 'conv3', dim_in=16, dim_out=32, kernel=4)                                           
# 11x2 -> 10x1                                                                                                      
pool3 = brew.max_pool(model, conv3, 'pool3', kernel=2)                                                              
relu3 = brew.relu(model, pool3, 'relu3')                                                                            
# 10x1x32 -> 128                                                                                                    
fc4 = brew.fc(model, relu3, 'fc4', dim_in=10*1*32, dim_out=128)                                                     
relu4 = brew.relu(model, fc4, 'relu4')                                                                              
# 128 -> 2                                                                                                          
output = brew.fc(model, relu4, 'output', dim_in=128, dim_out=2)                                                     
*/
    input = wks->CreateBlob("input");
    input->Reset((float *)new float[2*59*249]);
    output = wks->CreateBlob("output");
    putput->Reset((float *)new float[2]);
    for (auto const kv : networkBlobs) {
        Blob *bb = wks->CreateBlob(kv.first);
        bb->ShareExternal(kv.second);
    }

    CONV("conv1", "Conv",    "input", "conv1", 4, 1, 2, 8);
    POOL("pool1", "MaxPool", "conv1", "pool1", 2, 2, 8);
    RELU("relu1", "Relu",    "pool1", "relu1", 8);
    CONV("conv2", "Conv",    "pool1", "conv2", 4, 1, 8, 16);
    POOL("pool2", "MaxPool", "conv2", "pool2", 5, 5, 16);
    RELU("relu2", "Relu",    "pool2", "relu2", 16);
    CONF("conv3", "Conv",    "pool2", "conv3", 4, 1, 16, 32);
    POOL("pool3", "MaxPool", "conv3", "pool3", 2, 2, 32);
    RELU("relu3", "Relu",    "pool3", "relu3", 32);
    FC  ("fc4",   "FC",      "relu3", "fc4",   320, 128);
    RELU("relu4", "Relu",    "fc4",   "relu4", 128);
    FC  ("fc5",   "FC",      "relu4", "output",128, 2);
}

#endif


bool load_network(char const *name, FrameQueue *output) {
    if (!networkInput) {
        size_t size;
        int width, height, planes;
        get_unwarp_info(&size, &width, &height, &planes);
        assert(planes == 2);
        assert(width == 149);
        assert(height == 59);

#if 0
        if (!load_network_db(name)) {
            gNetworkFailed = true;
        } else {
            for (int i = 0; i != 3; ++i) {
                if (!instantiate_network(&gWorkspace[i])) {
                    fprintf(stderr, "Could not create network index %d\n", i);
                    gNetworkFailed = true;
                }
            }
        }
#endif

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

