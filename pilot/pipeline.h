#if !defined(pipeline_h)
#define pipeline_h

#include <pthread.h>
#include "reactive.h"

class FrameQueue;
class Frame;

class Pipeline : public Reactive {
    public:
        /* note that dstData may be NULL if output queue is full */
        Pipeline(void (*do_the_thing)(Pipeline *you, Frame *&srcData, Frame *&dstData, void *data, int index) = NULL);
        ~Pipeline();
        void connectInput(FrameQueue *input);
        void connectOutput(FrameQueue *output);
        bool start(void *data, int nthread=1);
        void stop();
        bool running();
        /* setDebug() requests that the function be called with the next set of input/output 
         * buffers available. Note that only one such request can be outstanding at a time, 
         * there is not a queue.
         */
        void setDebug(void (*debug)(Pipeline *you, Frame *src, Frame *dst, void *data), void *data);
        /* You can pass multiple thread counts to start() to run more than one instance of 
         * processing at a time. If you pass less than 1, or more than MAX_THREADS, it will 
         * fail and nothing will be started.
         */
        enum { MAX_THREADS=4 };
    protected:
        static void *thread_fn(void *);
        void thread(int index);
        void react() override;
        virtual void process(Frame *&src, Frame *&dst, int index);     //  by default calls processing_ if buffers are available
        void (*processing_)(Pipeline *, Frame *&, Frame *&, void *, int);
        void (*debug_)(Pipeline *, Frame *, Frame *, void *);
        void *debugData_;
        volatile bool running_;
        pthread_t thread_[MAX_THREADS];
        pthread_mutex_t mutex_;
        pthread_cond_t cond_;
        FrameQueue *input_;
        FrameQueue *output_;
        void *data_;
        struct ThreadStart {
            Pipeline *pipeline;
            int index;
        };
        ThreadStart start_[MAX_THREADS];
};


#endif  //  pipeline_h

