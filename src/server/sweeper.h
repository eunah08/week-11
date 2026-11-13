#ifndef SWEEPER_H
#define SWEEPER_H

#include <pthread.h>

#include "store.h"

/* 주기적 만료 정리. (협약 9.2) */

#define SWEEP_INTERVAL_MS 1000
#define SWEEP_BATCH_SIZE 200

/*
 * 정리 스레드를 시작한다. 성공하면 0.
 * SWEEP_INTERVAL_MS마다 store_lock을 잡고 store_remove_expired(SWEEP_BATCH_SIZE)를 부른다.
 * 만료 판정과 삭제가 같은 잠금 안에서 끝나므로, 그 사이 다른 요청이 같은 키를 바꿀 수 없다.
 */
int sweeper_start(store_t *store, pthread_mutex_t *store_lock);

/* 정리 스레드를 멈추고 join한다. 서버 종료 시 store_destroy보다 먼저 반드시 부른다 */
void sweeper_stop(void);

#endif
