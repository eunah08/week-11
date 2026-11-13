#ifndef HANDLERS_H
#define HANDLERS_H

#include <pthread.h>

#include "http.h"
#include "store.h"

/*
 * 요청 검증 → store 호출 → JSON 응답. store.h의 공개 함수만 쓴다. (협약 6장)
 * 동시성 : store 호출은 모두 store_mutex 하나 안에서 한다. 잠금 안에서는 store 호출만 하고,
 *          JSON 만들기와 네트워크 쓰기는 잠금 밖에서 한다. (협약 12장)
 * 모든 핸들러는 *out_response를 채운다. body는 호출한 쪽이 http_response_destroy로 해제한다.
 * key는 URL 디코딩을 마친 키다.
 */

typedef struct {
    store_t *store;
    pthread_mutex_t *store_mutex;
} server_context_t;

void handlers_put_key(const server_context_t *context, const char *key,
                      const http_request_t *request, http_response_t *out_response);
void handlers_get_key(const server_context_t *context, const char *key,
                      http_response_t *out_response);
void handlers_delete_key(const server_context_t *context, const char *key,
                         http_response_t *out_response);
void handlers_put_ttl(const server_context_t *context, const char *key,
                      const http_request_t *request, http_response_t *out_response);
void handlers_get_ttl(const server_context_t *context, const char *key,
                      http_response_t *out_response);
void handlers_get_exists(const server_context_t *context, const char *key,
                         http_response_t *out_response);
void handlers_post_invalidate(const server_context_t *context, const http_request_t *request,
                              http_response_t *out_response);
void handlers_get_stats(const server_context_t *context, http_response_t *out_response);
void handlers_get_health(http_response_t *out_response);

#endif
