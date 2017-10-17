#if !defined(lapwatch_h)
#define lapwatch_h

#include "metric.h"

#define START_WATCH() \
    uint64_t _t, _lw = metric::Collector::clock()

#define LAP_WATCH(n) \
    do { \
        _t = metric::Collector::clock(); \
        if (_t - _lw > 30000) { \
            fprintf(stderr, "LAP_WATCH: %s: %ld\n", n, (long)(_t - _lw)); \
        } \
        _lw = _t; \
    } while (false)

#endif  //  lapwatch_h
