#ifndef SWEEPER_H
#define SWEEPER_H

#include <pthread.h>

#include "store.h"

/*
 * 주기적 만료 정리. (협약 9.2)
 * 스레드 하나가 SWEEP_INTERVAL_MS마다 store_mutex를 잡고 store_remove_expired(SWEEP_BATCH_SIZE)를
 * 부른다. 요청 처리와 같은 잠금을 쓰므로, 만료 판정과 삭제 사이에 다른 요청이 끼어들 수 없다.
 * 서버에 하나만 둔다 (상태를 sweeper.c 안에 둔다).
 */

#define SWEEP_INTERVAL_MS 1000
#define SWEEP_BATCH_SIZE 200

/* 정리 스레드를 시작한다. 성공하면 0, 스레드를 만들지 못했거나 이미 돌고 있으면 -1 */
int sweeper_start(store_t *store, pthread_mutex_t *store_mutex);

/* 정리 스레드를 깨워 멈추고 join한다. 시작하지 않았으면 아무것도 안 한다. 서버 종료 시 반드시 부른다 */
void sweeper_stop(void);

#endif
