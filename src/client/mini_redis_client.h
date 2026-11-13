#ifndef MINI_REDIS_CLIENT_H
#define MINI_REDIS_CLIENT_H

#include <stdint.h>

/*
 * demo가 쓰는 미니 레디스 HTTP 호출을 함수로 감싼 얇은 포장. (협약 11.6)
 * 404 KEY_NOT_FOUND는 "없음"으로 돌려주고 에러로 보지 않는다.
 * 연결 실패, 시간 초과, 그 밖의 응답은 에러로 돌려준다.
 */

#define MINI_REDIS_TIMEOUT_MS 1000   /* 미니 레디스가 응답하지 않을 때 기다리는 최대 시간 */

typedef enum {
    MINI_REDIS_CLIENT_OK = 0,
    MINI_REDIS_CLIENT_NOT_FOUND,
    MINI_REDIS_CLIENT_ERROR
} mini_redis_client_status_t;

typedef struct {
    const char *host;   /* IPv4 주소. 예 : "127.0.0.1" */
    int port;
} mini_redis_client_t;

/* GET /keys/{key}. 해제 책임 : 성공하면 호출한 쪽이 *out_value를 free(). 실패하면 NULL */
mini_redis_client_status_t mini_redis_client_get(const mini_redis_client_t *client,
                                                 const char *key, char **out_value);

/* PUT /keys/{key} {"value": value, "ttlMs": ttl_ms} */
mini_redis_client_status_t mini_redis_client_set(const mini_redis_client_t *client,
                                                 const char *key, const char *value,
                                                 int64_t ttl_ms);

/* DELETE /keys/{key} */
mini_redis_client_status_t mini_redis_client_delete(const mini_redis_client_t *client,
                                                    const char *key);

#endif
