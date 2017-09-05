#if !defined(plock_h)
#define plock_h

#include <pthread.h>

class PLock {
    public:
        PLock(pthread_mutex_t &mtx) : mtx_(mtx) {
            pthread_mutex_lock(&mtx_);
        }
        ~PLock() {
            pthread_mutex_unlock(&mtx_);
        }
        pthread_mutex_t &mtx_;
    private:
        PLock(PLock const &) = delete;
        PLock &operator=(PLock const &) = delete;
};

#endif  //  plock_h

