#include "clock.h"

#include <time.h>

#define MS_PER_SECOND 1000
#define NS_PER_MS 1000000

uint64_t clock_now_ms(void) {
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * MS_PER_SECOND + (uint64_t)now.tv_nsec / NS_PER_MS;
}
