#ifndef ROUTER_H
#define ROUTER_H
#include "http.h"
/* 소유권은 HTTP 요청/응답 규칙과 동일. context는 server_context_t. */
void router_dispatch(const http_request_t *request, http_response_t *out_response, void *context);
#endif
