#include "network.h"
#include "queue.h"
#include "image.h"  //  for warp size
#include "pipeline.h"
#include <assert.h>
#include <unistd.h>

#include <caffe2/core/db.h>
#include <caffe2/core/workspace.h>
#include <caffe2/proto/caffe2.pb.h>

static FrameQueue *networkInput;
static Pipeline *networkPipeline;

static void process_network(Pipeline *, Frame *&src, Frame *&dst, void *, int index) {
    //  do the thing!
    usleep(25000);
    if (dst) {
        //  the other queue will be responsible for this buffer
        dst->link(src);
        src = NULL;
    }
}

bool load_network_db(char const *name, caffe2::Workspace *workspace) {
    try {
        caffe2::db::DBReader dbr("lmdb", name);
        //  This will generate a nasty log message, but Read() doesn't give any indication
        //  of whether it's at the end and wrapping or not
        caffe2::db::Cursor *c = dbr.cursor();
        c->SeekToFirst();
        std::string colon_net(":NET");
        //  look at buildnet.py function save_trained_model() for the format here
        for (; c->Valid(); c->Next()) {
            std::string key(c->key());
            if (key == colon_net) {
                //  do nothing
            } else {
                caffe2::TensorProto tp;
                caffe2::Blob *b = workspace->CreateBlob(key);
                b->Deserialize(c->value());
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

caffe2::Workspace gWorkspace;

bool load_network(char const *name, FrameQueue *output) {
    if (!networkInput) {
        size_t size;
        int width, height, planes;
        get_unwarp_info(&size, &width, &height, &planes);
        assert(planes == 2);
        assert(width == 149);
        assert(height == 59);

        if (!load_network_db(name, &gWorkspace)) {
            return false;
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

