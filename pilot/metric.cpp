#include "metric.h"
#include <sched.h>
#include <assert.h>
#include <string.h>


static int64_t base_time =  -1;

#if defined(__arm__)    //  raspberry pi ghetto detection

#include "interface/vcos/vcos.h"

uint64_t get_microseconds_base() {
    return vcos_getmicrosecs64();
}

uint64_t get_microseconds() {
    if (base_time == -1) {
        base_time = vcos_getmicrosecs64();
    }
    return vcos_getmicrosecs64() - base_time;
}
#else

#error "not on raspberry pi"

#include <time.h>

uint64_t get_microseconds_base() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

uint64_t get_microseconds() {
    if (base_time == -1) {
        base_time = get_microseconds_base();
    }
    return get_microseconds_base() - base_time;
}
#endif

namespace metric {

    Counter *Collector::counters_;
    Sampler *Collector::samplers_;
    Flag *Collector::flags_;
MetricBase *Collector::freeList_;


Collector::Collector() {
    //  make sure clock is initialized
    (void)Collector::clock();
}

uint64_t Collector::clock() {
    return get_microseconds();
}

void Collector::collect(void (*data_fn)(int type, char const *name, MetricBase const *mb, void *cookie), void *cookie) {
    MetricBase *mb = freeList_;
    freeList_ = NULL;
    MetricBase *newFreeList = NULL;

    for (Counter *c = counters_; c != NULL; c = c->next_) {
        assert(mb != NULL);
        MetricBase *nu = mb;
        mb = mb->nextBase_;
        memset(nu, 0, sizeof(*nu));
        MetricBase *old = c->metric_;
        c->metric_ = nu;
        if (data_fn) {
            data_fn(0, c->name_, old, cookie);
        }
        old->nextBase_ = newFreeList;
        newFreeList = old;
    }

    for (Sampler *s = samplers_; s != NULL; s = s->next_) {
        assert(mb != NULL);
        MetricBase *nu = mb;
        mb = mb->nextBase_;
        memset(nu, 0, sizeof(*nu));
        MetricBase *old = s->metric_;
        s->metric_ = nu;
        data_fn(1, s->name_, old, cookie);
        old->nextBase_ = newFreeList;
        newFreeList = old;
    }

    for (Flag *f = flags_; f != NULL; f = f->next_) {
        assert(mb != NULL);
        MetricBase *nu = mb;
        mb = mb->nextBase_;
        memset(nu, 0, sizeof(*nu));
        MetricBase *old = f->metric_;
        //  flags are special -- their value carries forward
        nu->value_.b = old->value_.b;
        nu->time_ = old->time_;
        f->metric_ = nu;
        data_fn(2, f->name_, old, cookie);
        old->nextBase_ = newFreeList;
        newFreeList = old;
    }

    assert(mb == NULL); /* exact count */
    freeList_ = newFreeList;
}

Counter const *Collector::getCounter(char const *name) {
    for (Counter *c = counters_; c != NULL; c = c->next_) {
        if (!strcmp(name, c->name_)) {
            return c;
        }
    }
    return NULL;
}

Counter const *Collector::iterCounters() {
    return counters_;
}

Counter const *Collector::nextCounter(Counter const *counter) {
    return counter ? counter->next_ : NULL;
}


Sampler const *Collector::getSampler(char const *name) {
    for (Sampler *s = samplers_; s != NULL; s = s->next_) {
        if (!strcmp(name, s->name_)) {
            return s;
        }
    }
    return NULL;
}

Sampler const *Collector::iterSamplers() {
    return samplers_;
}

Sampler const *Collector::nextSampler(Sampler const *sampler) {
    return sampler ? sampler->next_ : NULL;
}


Flag const *Collector::getFlag(char const *name) {
    for (Flag *f = flags_; f != NULL; f = f->next_) {
        if (!strcmp(name, f->name_)) {
            return f;
        }
    }
    return NULL;
}

Flag const *Collector::iterFlags() {
    return flags_;
}

Flag const *Collector::nextFlag(Flag const *flag) {
    return flag ? flag->next_ : NULL;
}

MetricBase *Collector::newMetric() {
    /* All allocation happens during start-up, when 
     * metrics are created. Allocate the additional 
     * metric record needed for collect() at the same 
     * time.
     */
    MetricBase *mb = new MetricBase();
    memset(mb, 0, sizeof(*mb));
    *(MetricBase **)mb = freeList_;
    freeList_ = mb;
    mb = new MetricBase();
    memset(mb, 0, sizeof(*mb));
    return mb;
}

Counter::Counter(char const *name) {
    next_ = Collector::counters_;
    Collector::counters_ = this;
    name_ = name;
    metric_ = Collector::newMetric();
}

void Counter::increment(int64_t amount) {
    uint64_t time = Collector::clock();
    MetricBase *met = metric_;
    //  For now, don't worry about collect() races.
    //  I will need to add an atomic modify counter to 
    //  it to fix that.
    met->time_ = time;
    met->count_ += 1;
    met->value_.i = amount;
    met->sum_ += amount;
}

void Counter::get(int64_t &oCount, double &oAverage, uint64_t &oTime) const {
    MetricBase *met = metric_;
    oCount = met->count_;
    oAverage = met->count_ ? met->sum_ / met->count_ : 0;
    oTime = met->time_;
}

char const *Counter::name() const {
    return name_;
}

Sampler::Sampler(char const *name) {
    next_ = Collector::samplers_;
    Collector::samplers_ = this;
    name_ = name;
    metric_ = Collector::newMetric();
}

void Sampler::sample(double amount) {
    uint64_t time = Collector::clock();
    MetricBase *met = metric_;
    met->time_ = time;
    met->sum_ += amount;
    met->count_ += 1;
    met->value_.d = amount;
}

void Sampler::get(double &oValue, double &oAverage, uint64_t &oTime) const {
    MetricBase *met = metric_;
    oValue = met->value_.d;
    oAverage = met->count_ ? met->sum_ / met->count_ : 0;
    oTime = met->time_;
}

char const *Sampler::name() const {
    return name_;
}

Flag::Flag(char const *name) {
    next_ = Collector::flags_;
    Collector::flags_ = this;
    name_ = name;
    metric_ = Collector::newMetric();
}

void Flag::set(bool on) {
    uint64_t time = Collector::clock();
    MetricBase *met = metric_;
    met->time_ = time;
    met->count_ += 1;
    met->value_.b = on;
    met->sum_ += on ? 1 : 0;
}

void Flag::get(bool &oFlag, double &oAverage, uint64_t &oTime) const {
    MetricBase *met = metric_;
    oFlag = met->value_.b;
    oAverage = met->count_ ? met->sum_ / met->count_ : met->value_.b ? 1.0 : 0.0;
    oTime = met->time_;
}

char const *Flag::name() const {
    return name_;
}

}

