#include "slow_source.h"

static uint64_t origin_call_count = 0;

uint64_t slow_source_get_origin_call_count(void) {
    return origin_call_count;
}
