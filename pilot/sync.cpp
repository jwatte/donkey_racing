#include "sync.h"
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>


static bool syncRunning = false;
static pthread_mutex_t syncLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t syncCond = PTHREAD_COND_INITIALIZER;
static pthread_t syncThread;

static void *sync_func(void *) {
    fprintf(stderr, "running sync function\n");
    while (syncRunning) {
        sync();
        pthread_mutex_lock(&syncLock);
        if (syncRunning) {
            struct timespec time = { 10, 0 };
            pthread_cond_timedwait(&syncCond, &syncLock, &time);
        }
        pthread_mutex_unlock(&syncLock);
    }
    return NULL;
}

void run_sync_thread() {
    if (!syncRunning) {
        atexit(stop_sync_thread);
        syncRunning = true;
        pthread_create(&syncThread, NULL, &sync_func, NULL);
    }
}

void stop_sync_thread() {
    if (syncRunning) {
        fprintf(stderr, "stopping sync thread\n");
        pthread_mutex_lock(&syncLock);
        syncRunning = false;
        pthread_cond_signal(&syncCond);
        pthread_mutex_unlock(&syncLock);
        void *r;
        pthread_join(syncThread, &r);
        syncThread = 0;
        fprintf(stderr, "sync thread stopped\n");
    }
}
