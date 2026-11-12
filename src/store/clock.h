#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

/*
 * 단조 시계 기준 현재 시각을 밀리초로 돌려준다. (협약 4.7)
 * 시스템 시각을 바꿔도 거꾸로 가지 않는다.
 */
uint64_t clock_now_ms(void);

#endif
