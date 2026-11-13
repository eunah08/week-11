#ifndef HANDLERS_H
#define HANDLERS_H

#include <pthread.h>

#include "http.h"
#include "store.h"

/*
 * 요청 검증 → store 호출 → JSON 응답. store.h의 공개 함수만 쓴다. (협약 6장)
 * 동시성 : store 전체에 잠금 하나를 두고, store 함수를 부르는 동안만 잡는다. (협약 12장)
 * 잠금을 쥔 채로 네트워크 읽기·쓰기를 하지 않는다.
 * 해제 책임 : 채운 out_response->body는 호출한 쪽이 http_response_destroy로 해제한다.
 */

typedef struct {
    store_t *store;
    pthread_mutex_t *store_lock;
} handlers_context_t;

/* PUT /keys/{key} : {"value": "...", "ttlMs": 5000} → {"created": true|false} */
void handlers_put_key(const handlers_context_t *context, const char *key,
                      const http_request_t *request, http_response_t *out_response);

/* GET /keys/{key} → {"value": "..."} */
void handlers_get_key(const handlers_context_t *context, const char *key,
                      http_response_t *out_response);

/* DELETE /keys/{key} → {"deleted": true|false} */
void handlers_delete_key(const handlers_context_t *context, const char *key,
                         http_response_t *out_response);

/* PUT /keys/{key}/ttl : {"ttlMs": 5000} → {"applied": true|false} */
void handlers_put_ttl(const handlers_context_t *context, const char *key,
                      const http_request_t *request, http_response_t *out_response);

/* GET /keys/{key}/ttl → {"ttlMs": 4980} (만료 없으면 -1) */
void handlers_get_ttl(const handlers_context_t *context, const char *key,
                      http_response_t *out_response);

/* POST /invalidate : {"prefix": "product:"} → {"deletedCount": 3} */
void handlers_post_invalidate(const handlers_context_t *context, const http_request_t *request,
                              http_response_t *out_response);

/* GET /stats → {"size", "capacity", "hitCount", "missCount", "expiredCount"} */
void handlers_get_stats(const handlers_context_t *context, http_response_t *out_response);

#endif
