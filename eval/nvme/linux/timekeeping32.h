#include <time.h>

static inline void getrawmonotonic(struct timespec *ts)
{
    clock_gettime(CLOCK_MONOTONIC_RAW, ts);
}