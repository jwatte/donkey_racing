#if !defined(pipeline_h)
#define pipeline_h

#include <pthread.h>
#include "reactive.h"

class FrameQueue;
class Frame;

class Pipeline : public Reactive {
    public:
        /* note that dstData may be NULL if output queue is full */
        Pipeline(void (*do_the_thing)(Pipeline *you, Frame *&srcData, Frame *&dstData, void *data) = NULL);
        ~Pipeline();
        void connectInput(FrameQueue *input);
        void connectOutput(FrameQueue *output);
        void start(void *data);
        void stop();
        bool running();
        void setDebug(void (*debug)(Pipeline *you, Frame *src, Frame *dst, void *data), void *data);
    private:
        static void *thread_fn(void *);
        void thread();
        void react() override;
        virtual void process(Frame *&src, Frame *&dst);     //  by default calls processing_ if buffers are available
        void (*processing_)(Pipeline *, Frame *&, Frame *&, void *);
        void (*debug_)(Pipeline *, Frame *, Frame *, void *);
        void *debugData_;
        volatile bool running_;
        pthread_t thread_;
        pthread_mutex_t mutex_;
        pthread_cond_t cond_;
        FrameQueue *input_;
        FrameQueue *output_;
        void *data_;
};


#endif  //  pipeline_h

