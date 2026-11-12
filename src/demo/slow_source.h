#ifndef SLOW_SOURCE_H
#define SLOW_SOURCE_H

#include <stdint.h>

/* 느린 원본(DB나 외부 API)을 흉내낸다. (협약 13.1) */

#define SLOW_SOURCE_DELAY_MS 300

/* TODO(이니셜): get_product(id)의 시그니처와 메모리 해제 책임을 정한다 */

/* 원본을 부른 횟수 */
uint64_t slow_source_get_origin_call_count(void);

#endif
