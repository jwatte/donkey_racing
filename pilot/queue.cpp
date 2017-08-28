#include "queue.h"
#include "reactive.h"
#include "plock.h"
#include <assert.h>


#define TO_WRITE 0
#define IN_WRITE 1
#define TO_READ 2
#define IN_READ 3


Frame::Frame(size_t size)
    : data_(new unsigned char[size])
    , queue_(NULL)
    , link_(NULL)
    , size_(size)
    , width_(0)
    , height_(0)
    , format_(0)
    , index_(0)
    , state_(0)
{
}

Frame::~Frame() {
    delete[] data_;
}

void Frame::endWrite() {
    assert(state_ == IN_WRITE);
    queue_->endWrite(this);
}

void Frame::endRead() {
    assert(state_ == IN_READ);
    queue_->endRead(this);
}

void Frame::recycle() {
    assert(state_ != TO_WRITE);
    Frame *link = NULL;
    {
        PLock lock(queue_->mutex_);
        link = link_;
        if (link) {
            link_ = NULL;
        }
        state_ = TO_WRITE;
        queue_->toWrite_.push_back(this);
    }
    if (link) {
        link->recycle();
    }
}

void Frame::link(Frame *other) {
    if (link_) {
        link_->link(other);
    } else {
        link_ = other;
    }
}


FrameQueue::FrameQueue(size_t n, size_t size, int width, int height, int format)
    : target_(NULL)
    , mutex_(PTHREAD_MUTEX_INITIALIZER)
    , count_(n)
    , readEmpty_(true)
{
    for (size_t i = 0; i != n; ++i) {
        Frame *f = new Frame(size);
        f->queue_ = this;
        f->link_ = NULL;
        f->width_ = width;
        f->height_ = height;
        f->format_ = format;
        f->index_ = (int)i;
        f->state_ = TO_WRITE;
        toWrite_.push_back(f);
    }
}

FrameQueue::~FrameQueue() {
    for (auto & ptr : toRead_) {
        delete ptr;
    }
    for (auto &ptr : toWrite_) {
        delete ptr;
    }
}

void FrameQueue::setTarget(Reactive *target) {
    target_ = target;
}

Frame *FrameQueue::beginWrite() {
    PLock lock(mutex_);
    if (toWrite_.empty()) {
        return NULL;
    }
    Frame *ret = toWrite_.front();
    toWrite_.pop_front();
    assert(ret->state_ == TO_WRITE);
    ret->state_ = IN_WRITE;
    return ret;
}

void FrameQueue::endWrite(Frame *f) {
    if (f) {
        assert(f->state_ == IN_WRITE);
        Reactive *target = NULL;
        {
            PLock lock(mutex_);
            f->state_ = TO_READ;
            toRead_.push_back(f);
            readEmpty_ = false;
            target = target_;
        }
        if (target) {
            target->react();
        }
    }
}

Frame *FrameQueue::beginRead() {
    PLock lock(mutex_);
    if (toRead_.empty()) {
        return NULL;
    }
    Frame *ret = toRead_.front();
    assert(ret->state_ == TO_READ);
    ret->state_ = IN_READ;
    toRead_.pop_front();
    readEmpty_ = toRead_.empty();
    return ret;
}

void FrameQueue::endRead(Frame *f) {
    if (f) {
        assert(f->state_ == IN_READ);
        f->recycle();
    }
}

bool FrameQueue::readEmpty() {
    return readEmpty_;
}

void FrameQueue::getStats(int &oInSize, int &oOutSize, int &oInFlight) {
    PLock lock(mutex_);
    oInSize = (int)toWrite_.size();
    oOutSize = (int)toRead_.size();
    oInFlight = (int)count_ - oInSize - oOutSize;
}



