#include "pipeline.h"
#include "queue.h"
#include <stdio.h>
#include <string.h>
#include "plock.h"


Pipeline::Pipeline(void (*do_the_thing)(Pipeline *you, Frame *&srcData, Frame *&dstData, void *data, int index))
    : processing_(do_the_thing)
    , debug_(NULL)
    , debugData_(NULL)
    , running_(false)
    , mutex_(PTHREAD_MUTEX_INITIALIZER)
    , cond_(PTHREAD_COND_INITIALIZER)
    , input_(NULL)
    , output_(NULL)
{
    memset(thread_, 0, sizeof(thread_));
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

bool Pipeline::start(void *data, int nthread) {
    if (nthread < 0 || nthread > MAX_THREADS) {
        return false;
    }
    PLock lock(mutex_);
    if (!thread_[0]) {
        data_ = data;
        running_ = true;
        for (int i = 0; i != nthread; ++i) {
            start_[i].pipeline = this;
            start_[i].index = i;
            if (pthread_create(&thread_[i], NULL, thread_fn, &start_[i])) {
                fprintf(stderr, "Pipeline: pthread_create() failed\n");
                running_ = false;
                pthread_cond_broadcast(&cond_);
                memset(thread_, 0, sizeof(thread_));
                return false;
            }
        }
        return true;
    }
    return false;
}

void Pipeline::stop() {
    if (thread_[0]) {
        {
            PLock lock(mutex_);
            running_ = false;
            pthread_cond_broadcast(&cond_);
        }
        for (int i = 0; i != MAX_THREADS; ++i) {
            void *x = NULL;
            if (thread_[i]) {
                pthread_join(thread_[i], &x);
            }
        }
        data_ = NULL;
        memset(thread_, 0, sizeof(thread_));
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
    ThreadStart *ts = reinterpret_cast<ThreadStart *>(that);
    ts->pipeline->thread(ts->index);
    return NULL;
}

void Pipeline::thread(int index) {
    fprintf(stderr, "starting Pipeline\n");
    while (running_) {
        Frame *srcData = NULL;
        Frame *dstData = NULL;
        {
            PLock lock(mutex_);
            if (!running_) {
                break;
            }
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
            process(srcData, dstData, index);
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

void Pipeline::process(Frame *&srcData, Frame *&dstData, int index) {
    if (processing_ != NULL) {
        processing_(this, srcData, dstData, data_, index);
    }
}


