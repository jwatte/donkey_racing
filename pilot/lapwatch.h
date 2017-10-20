#if !defined(lapwatch_h)
#define lapwatch_h

#include "metric.h"

#if 1
#define START_WATCH(x) (void)0
#define LAP_WATCH(x) (void)0
#define LAP_REPORT() (void)0

#else

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

#endif

#endif  //  lapwatch_h
