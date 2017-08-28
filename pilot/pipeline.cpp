#include "pipeline.h"
#include "queue.h"
#include <stdio.h>
#include "plock.h"


Pipeline::Pipeline(void (*do_the_thing)(Pipeline *you, Frame *&srcData, Frame *&dstData, void *data))
    : processing_(do_the_thing)
    , running_(false)
    , thread_(0)
    , mutex_(PTHREAD_MUTEX_INITIALIZER)
    , cond_(PTHREAD_COND_INITIALIZER)
    , input_(NULL)
    , output_(NULL)
{
}

Pipeline::~Pipeline() {
    stop();
    connectInput(NULL);
}

void Pipeline::connectInput(FrameQueue *input) {
    if (input_) {
        input_->setTarget(NULL);
    }
    input_ = input;
    if (input_) {
        input_->setTarget(this);
        pthread_cond_signal(&cond_);
    }
}

void Pipeline::connectOutput(FrameQueue *output) {
    output_ = output;
}

void Pipeline::start(void *data) {
    PLock lock(mutex_);
    if (!thread_) {
        data_ = data;
        running_ = true;
        if (pthread_create(&thread_, NULL, thread_fn, this)) {
            fprintf(stderr, "Pipeline: pthread_create() failed\n");
            thread_ = 0;
            running_ = false;
            return;
        }
    }
}

void Pipeline::stop() {
    if (thread_) {
        {
            PLock lock(mutex_);
            running_ = false;
            pthread_cond_signal(&cond_);
        }
        void *x = NULL;
        pthread_join(thread_, &x);
        data_ = NULL;
        thread_ = 0;
    }
}

bool Pipeline::running() {
    return running_;
}

void Pipeline::setDebug(void (*func)(Pipeline *, Frame *, Frame *, void *), void *data) {
    PLock lock(mutex_);
    debug_ = func;
    debugData_ = data;
}


void *Pipeline::thread_fn(void *that) {
    reinterpret_cast<Pipeline *>(that)->thread();
    return NULL;
}

void Pipeline::thread() {
    fprintf(stderr, "starting Pipeline\n");
    while (running_) {
        Frame *srcData = NULL;
        Frame *dstData = NULL;
        {
            PLock lock(mutex_);
            if (!input_ || input_->readEmpty()) {
                pthread_cond_wait(&cond_, &mutex_);
            }
            if (!running_) {
                break;
            }
            if (input_) {
                srcData = input_->beginRead();
            }
            if (srcData && output_) {
                dstData = output_->beginWrite();
            }
        }
        if (srcData) {
            Frame *origSrc = srcData;
            Frame *origDst = dstData;
            process(srcData, dstData);
            if (debug_) {
                void (*dbfn)(Pipeline *, Frame *, Frame *, void *) = NULL;
                void *dd = NULL;
                {
                    PLock lock(mutex_);
                    dbfn = debug_;
                    dd = debugData_;
                    debug_ = NULL;
                    debugData_ = NULL;
                }
                if (dbfn) {
                    dbfn(this, origSrc, origDst, dd);
                }
            }
            if (srcData) {
                srcData->endRead();
            }
            if (dstData) {
                dstData->endWrite();
            }
        } else {
            fprintf(stderr, "Pipeline: wakeup without data\n");
        }
    }
    fprintf(stderr, "ending Pipeline\n");
}

void Pipeline::react() {
    pthread_cond_signal(&cond_);
}

void Pipeline::process(Frame *&srcData, Frame *&dstData) {
    if (processing_ != NULL) {
        processing_(this, srcData, dstData, data_);
    }
}


