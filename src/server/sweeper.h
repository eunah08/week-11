#ifndef SWEEPER_H
#define SWEEPER_H
#include "handlers.h"
#define SWEEP_INTERVAL_MS 1000
#define SWEEP_BATCH_SIZE 200
/* 서버 문맥을 빌려 정리 시작. 성공 0, 실패 -1. stop 전 문맥 해제 금지. */
int sweeper_start(server_context_t *server);
/* 종료 플래그 후 join. 저장소 해제 전에 호출하며 여러 번 호출해도 된다. */
void sweeper_stop(void);
#endif
