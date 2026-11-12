#ifndef CACHE_CLIENT_H
#define CACHE_CLIENT_H
#include <stdint.h>
typedef struct { int port; int64_t ttl_ms; } cache_client_t;
/* HTTP만 사용. 조회 실패/부재 NULL, 성공 복사본은 호출자가 free. */
char *cache_client_get(const cache_client_t *client, const char *id);
/* 성공 0, 실패 -1. 문자열은 빌린 입력이며 상태는 원격 서버에만 존재한다. */
int cache_client_set(const cache_client_t *client, const char *id, const char *value);
int cache_client_delete(const cache_client_t *client, const char *id);
#endif
