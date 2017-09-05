#if !defined(metric_h)
#define metric_h

#include <stdint.h>

/* The counters / samples / flags are created once and never go away. 
 * There is an assumption that this happens during program init, and 
 * no new metrics are created at runtime.
 * However, the values within them are volatile, and the collect() 
 * operation of the Collector will swap them out for new, zeroed-out 
 * values by doing pointer swapping. This means that iterating through 
 * all metrics is safe, even from multiple threads, but the implementation 
 * of incrementing, reading, and swapping values needs to be careful. In 
 * practice, by using double memory, as long as collect() isn't run more 
 * than once in parallel, and threads get scheduled and finish within the 
 * interval between collect() calls, no inconsistent data will be 
 * observed. There is a small risk of live-lock if some thread is doing
 * aggressive mutation of a particular value in a tight loop. Don't do that.
 */
namespace metric {

    struct MetricBase;

    class Counter {
        public:
            Counter(char const *name);
            void increment(int64_t amount = 1);
            void get(int64_t &oCount, double &oAverage, uint64_t &oTime) const;
            char const *name() const;
        private:
            friend class Collector;
            Counter(Counter const &) = delete;
            Counter &operator=(Counter const &) = delete;
            Counter *next_;
            char const *name_;
            MetricBase *metric_;
    };

    class Sampler {
        public:
            Sampler(char const *name);
            void sample(double amount);
            void get(double &oValue, double &oAverage, uint64_t &oTime) const;
            char const *name() const;
        private:
            friend class Collector;
            Sampler(Sampler const &) = delete;
            Sampler &operator=(Sampler const &) = delete;
            Sampler *next_;
            char const *name_;
            MetricBase *metric_;
    };

    class Flag {
        public:
            Flag(char const *name);
            void set(bool on=true);
            inline void clear() { set(false); }
            void get(bool &oFlag, double &oAverage, uint64_t &oTime) const;
            char const *name() const;
        private:
            friend class Collector;
            Flag(Flag const &) = delete;
            Flag &operator=(Flag const &) = delete;
            Flag *next_;
            char const *name_;
            MetricBase *metric_;
    };

    class Collector {
        public:
            Collector();
            static uint64_t clock();
            static void collect(void (*data_fn)(int type, char const *name, MetricBase const *mb, void *cookie), void *cookie);

            static Counter const *getCounter(char const *name);
            static Counter const *iterCounters();
            static Counter const *nextCounter(Counter const *counter);

            static Sampler const *getSampler(char const *name);
            static Sampler const *iterSamplers();
            static Sampler const *nextSampler(Sampler const *sampler);

            static Flag const *getFlag(char const *name);
            static Flag const *iterFlags();
            static Flag const *nextFlag(Flag const *flag);

        private:
            friend class Counter;
            friend class Sampler;
            friend class Flag;
            static MetricBase *newMetric();
            static Counter *counters_;
            static Sampler *samplers_;
            static Flag *flags_;
            static MetricBase *freeList_;
    };

    /* By adding an atomic busy counter to this structure, it
     * is possible to ensure that modifications always happen 
     * to a consistent MetricBase, even when racing with collect().
     * Currently, I don't worry about that.
     */
    struct MetricBase {
        MetricBase *nextBase_;
        uint64_t time_;
        uint64_t count_;
        union {
            int64_t i;
            double d;
            bool b;
        } value_;
        double sum_;    //  for sampler
    };
};

#endif  //  metric_h

