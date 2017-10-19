#if !defined(lapwatch_h)
#define lapwatch_h

#include "metric.h"

#define START_WATCH(t) \
    uint64_t _t, _mx = 0, _lw = metric::Collector::clock(); char const *_lt = t; char const *_tx = ""

#define LAP_WATCH(n) \
    do { \
        _t = metric::Collector::clock(); \
        if (_t - _lw > _mx) { \
            _mx = _t - _lw; \
            _tx = n; \
        } \
        _lw = _t; \
    } while (false)

#define LAP_REPORT() \
    fprintf(stderr, "\n%s: %s: %ld", _lt, _tx, (long)_mx)

#endif  //  lapwatch_h
