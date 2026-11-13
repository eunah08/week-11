#ifndef MINI_REDIS_CLIENT_H
#define MINI_REDIS_CLIENT_H

#include <stdint.h>

/*
 * 미니 레디스 HTTP API를 부르는 얇은 포장. (협약 11.6, 선택)
 * demo가 미니 레디스를 외부 사용자와 같은 입장(HTTP)에서만 쓰게 한다. store를 모른다.
 * 요청마다 연결을 새로 열고 닫는다. 서버가 꺼져 있거나 timeout_ms 안에 답이 없으면 ERROR.
 */

typedef enum {
    MINI_REDIS_CLIENT_OK = 0,
    MINI_REDIS_CLIENT_NOT_FOUND,    /* 404 KEY_NOT_FOUND. 에러가 아니다 */
    MINI_REDIS_CLIENT_ERROR         /* 연결 실패, 시간 초과, 예상하지 못한 응답, 메모리 부족 */
} mini_redis_client_status_t;

typedef struct {
    const char *host;    /* IPv4 주소 문자열. 예 : "127.0.0.1" */
    int port;
    int timeout_ms;
} mini_redis_client_t;

/* GET /keys/{key}. 성공하면 *out_value는 호출한 쪽이 free(). 실패하면 *out_value는 NULL */
mini_redis_client_status_t mini_redis_client_get(const mini_redis_client_t *client,
                                                 const char *key, char **out_value);

/* PUT /keys/{key} {"value", "ttlMs"}. ttl_ms가 0이면 ttlMs를 보내지 않는다 */
mini_redis_client_status_t mini_redis_client_set(const mini_redis_client_t *client,
                                                 const char *key, const char *value,
                                                 int64_t ttl_ms);

/* DELETE /keys/{key}. 키가 없어도 OK */
mini_redis_client_status_t mini_redis_client_delete(const mini_redis_client_t *client,
                                                    const char *key);

#endif
