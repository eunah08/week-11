#ifndef SWEEPER_H
#define SWEEPER_H

#include "store.h"

/* 주기적 만료 정리. (협약 9.2) */

#define SWEEP_INTERVAL_MS 1000
#define SWEEP_BATCH_SIZE 200

/* 정리 작업을 시작한다. 성공하면 0 */
int sweeper_start(store_t *store);

/* 정리 작업을 멈추고 자원을 남기지 않는다. 서버 종료 시 반드시 부른다 */
void sweeper_stop(void);

#endif
