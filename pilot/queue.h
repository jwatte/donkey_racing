#if !defined(queue_h)
#define queue_h

#include <atomic>
#include <stddef.h>
#include <pthread.h>
#include <list>

class Reactive;
class FrameQueue;

struct Frame;

class FrameQueue {
    public:
        FrameQueue(size_t n, size_t size, int width, int height, int format);
        ~FrameQueue();

        void setTarget(Reactive *target);

        Frame *beginWrite();
        void endWrite(Frame *);

        Frame *beginRead();
        void endRead(Frame *);
        bool readEmpty();
    
        void getStats(int &oInSize, int &oOutSize, int &oInFlight);

    private:
        friend class Frame;
        Reactive *target_;
        std::list<Frame *> toRead_;
        std::list<Frame *> toWrite_;
        pthread_mutex_t mutex_;
        size_t count_;
        bool readEmpty_;
};

//  format:
//  1 == gray
//  2 == YUV420
//  3 == RGB
//  4 == RGBA
//  8 == 2*float
struct Frame {
    public:
        Frame(size_t size);
        ~Frame();
        unsigned char *data_;
        FrameQueue *queue_;
        Frame *link_;
        size_t size_;
        int width_;
        int height_;
        int format_;
        int index_;
        int state_;
        void endWrite();
        void endRead();
        void recycle();
        void link(Frame *other);
    private:
        Frame(Frame const &) = delete;
        Frame &operator=(Frame const &) = delete;
};

#endif  //  queue_h

