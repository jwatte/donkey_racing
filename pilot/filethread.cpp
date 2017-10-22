#include "filethread.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <string>
#include <list>
#include <map>
#include <vector>


static FileResult results[256];
static size_t frAlloc;
static size_t frHead;
static size_t frTail;
static pthread_t fileThread;
static pthread_mutex_t fileMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t fileCond = PTHREAD_COND_INITIALIZER;
static bool fileRunning = false;
static std::map<int, FILE *> files;

class Thing {
    public:
        Thing(int type, int name)
            : type_(type)
            , name_(name)
        {
        }
        int type_;
        int name_;
        virtual int perform() = 0;
        virtual ~Thing() {}
};

class FileOpenThing : public Thing {
    public:
        FileOpenThing(int name, char const *path)
            : Thing(FILE_OPEN, name)
            , path_(path)
        {
        }
        int perform() override {
            if (files.find(name_) != files.end()) {
                //fprintf(stderr, "attempt to re-open file %d\n", name_);
                return -1;
            }
            FILE *f = fopen(path_.c_str(), "wb");
            if (!f) {
                //fprintf(stderr, "unable to open file %d (%s)\n", name_, path_.c_str());
                return -2;
            }
            files[name_] = f;
            //fprintf(stderr, "open file %s as %d\n", path_.c_str(), name_);
            return 0;
        }
        std::string path_;
};

class FileWriteThing : public Thing {
    public:
        FileWriteThing(int name, void const *data, size_t size)
            : Thing(FILE_WRITE, name)
        {
            buf_.resize(size);
            memcpy(&buf_[0], data, size);
        }
        std::vector<char> buf_;
        int perform() override {
            auto const &ptr(files.find(name_));
            if (ptr == files.end()) {
                //fprintf(stderr, "attempt to write to unknown file %d\n", name_);
                return -1;
            }
            size_t sz = buf_.size();
            if (sz) {
                size_t n = fwrite(&buf_[0], 1, sz, (*ptr).second);
                if (n != sz) {
                    //fprintf(stderr, "short write to file %d\n", name_);
                    return -2;
                }
            }
            return 0;
        }
};

class FileFlushThing : public Thing {
    public:
        FileFlushThing(int name)
            : Thing(FILE_CLOSE, name)
        {
        }

        int perform() override {
            auto const &ptr(files.find(name_));
            if (ptr == files.end()) {
                //fprintf(stderr, "attempt to flush unknown file %d\n", name_);
                return -1;
            }
            fflush((*ptr).second);
            return 0;
        }
};

class FileCloseThing : public Thing {
    public:
        FileCloseThing(int name)
            : Thing(FILE_CLOSE, name)
        {
        }

        int perform() override {
            auto const &ptr(files.find(name_));
            if (ptr == files.end()) {
                //fprintf(stderr, "attempt to close unknown file %d\n", name_);
                return -1;
            }
            fclose((*ptr).second);
            sync();
            files.erase(ptr);
            return 0;
        }
};

static std::list<Thing *> things;


static void *file_thread_fn(void *) {
    pthread_mutex_lock(&fileMutex);
    while (fileRunning) {
        if (things.empty()) {
            pthread_cond_wait(&fileCond, &fileMutex);
            if (!fileRunning) {
                break;
            }
        }
        assert(!things.empty());
        Thing *t = things.front();
        things.pop_front();
        pthread_mutex_unlock(&fileMutex);
        int r = t->perform();
        delete t;
        assert(frAlloc - frHead > 0);
        FileResult &fr(results[frHead & (sizeof(results)/sizeof(results[0])-1)]);
        fr.name = t->name_;
        fr.type = t->type_;
        fr.result = r;
        ++frHead;
        assert(frAlloc - frHead <= sizeof(results)/sizeof(results[0]));
        pthread_mutex_lock(&fileMutex);
    }
    pthread_mutex_unlock(&fileMutex);
    return NULL;
}

bool start_filethread() {
    if (fileThread) {
        return false;
    }
    fileRunning = true;
    if (pthread_create(&fileThread, NULL, file_thread_fn, NULL)) {
        fileRunning = false;
        return false;
    }
    return true;
}

bool stop_filethread() {
    if (fileThread) {
        pthread_mutex_lock(&fileMutex);
        fileRunning = false;
        pthread_cond_signal(&fileCond);
        pthread_mutex_unlock(&fileMutex);
        void *r = NULL;
        pthread_join(fileThread, &r);
        for (auto const &p : things) {
            delete p;
        }
        things.clear();
        for (auto const &f : files) {
            fclose(f.second);
        }
        files.clear();
        frHead = frAlloc = frTail = 0;
        fileThread = 0;
        return true;
    }
    return false;
}

bool new_file(int name, char const *path) {
    pthread_mutex_lock(&fileMutex);
    if (frAlloc - frTail >= sizeof(results)/sizeof(results[0])) {
        pthread_mutex_unlock(&fileMutex);
        fprintf(stderr,
            "out of result space %ld %ld; get results before queuing more work!\n",
            (long)frAlloc, (long)frTail);
        return false;
    }
    frAlloc++;
    things.push_back(new FileOpenThing(name, path));
    pthread_cond_signal(&fileCond);
    pthread_mutex_unlock(&fileMutex);
    return true;
}

bool write_file(int name, void const *data, size_t size) {
    pthread_mutex_lock(&fileMutex);
    if (frAlloc - frTail >= sizeof(results)/sizeof(results[0])) {
        pthread_mutex_unlock(&fileMutex);
        fprintf(stderr, "out of result space; get results before queuing more work!\n");
        return false;
    }
    frAlloc++;
    things.push_back(new FileWriteThing(name, data, size));
    pthread_cond_signal(&fileCond);
    pthread_mutex_unlock(&fileMutex);
    return true;
}

bool write_file_vec(int name, std::vector<char> &f) {
    pthread_mutex_lock(&fileMutex);
    if (frAlloc - frTail >= sizeof(results)/sizeof(results[0])) {
        pthread_mutex_unlock(&fileMutex);
        fprintf(stderr, "out of result space; get results before queuing more work!\n");
        return false;
    }
    frAlloc++;
    FileWriteThing *fwt = new FileWriteThing(name, NULL, 0);
    fwt->buf_.swap(f);
    things.push_back(fwt);
    pthread_cond_signal(&fileCond);
    pthread_mutex_unlock(&fileMutex);
    return true;
}

bool flush_file(int name) {
    pthread_mutex_lock(&fileMutex);
    if (frAlloc - frTail >= sizeof(results)/sizeof(results[0])) {
        pthread_mutex_unlock(&fileMutex);
        fprintf(stderr, "out of result space; get results before queuing more work!\n");
        return false;
    }
    frAlloc++;
    things.push_back(new FileFlushThing(name));
    pthread_cond_signal(&fileCond);
    pthread_mutex_unlock(&fileMutex);
    return true;
}

bool close_file(int name) {
    pthread_mutex_lock(&fileMutex);
    if (frAlloc - frTail >= sizeof(results)/sizeof(results[0])) {
        pthread_mutex_unlock(&fileMutex);
        fprintf(stderr, "out of result space; get results before queuing more work!\n");
        return false;
    }
    frAlloc++;
    things.push_back(new FileCloseThing(name));
    pthread_cond_signal(&fileCond);
    pthread_mutex_unlock(&fileMutex);
    return true;
}

size_t get_results(FileResult *fr, size_t maxNum) {
    size_t nThere = frHead - frTail;
    if (nThere > maxNum) {
        nThere = maxNum;
    }
    if (nThere > 0) {
        for (size_t i = 0; i != nThere; ++i) {
            memcpy(fr, &results[frTail & (sizeof(results)/sizeof(results[0])-1)], sizeof(FileResult));
            ++fr;
            ++frTail;
        }
    }
    return nThere;
}


