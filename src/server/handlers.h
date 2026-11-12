#ifndef HANDLERS_H
#define HANDLERS_H

#include <pthread.h>
#include "http.h"
#include "store.h"

typedef struct {
    store_t *store;
    pthread_mutex_t mutex;
} server_context_t;
typedef enum { ROUTE_VALUE, ROUTE_TTL, ROUTE_INVALIDATE, ROUTE_STATS } route_kind_t;
typedef struct { route_kind_t kind; const char *key; } route_t;

/* 검증 완료 후 공개 store 호출 전체만 잠근다. 응답 body는 http 계층이 free. */
void handlers_dispatch(server_context_t *server, const route_t *route,
                        const http_request_t *request, http_response_t *out_response);

#endif
