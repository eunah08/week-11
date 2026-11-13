#ifndef ROUTER_H
#define ROUTER_H

#include "handlers.h"
#include "http.h"

/* 경로 → 핸들러 연결을 맡는다. 엔드포인트 목록은 협약 11.1 */

/*
 * 요청의 메서드와 경로로 핸들러를 골라 out_response를 채운다.
 * 없는 경로는 404 ROUTE_NOT_FOUND, 허용하지 않는 메서드는 405 METHOD_NOT_ALLOWED.
 * 해제 책임 : out_response->body는 호출한 쪽이 http_response_destroy로 해제한다.
 */
void router_route(const handlers_context_t *context, const http_request_t *request,
                  http_response_t *out_response);

#endif
