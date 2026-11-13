#ifndef ROUTER_H
#define ROUTER_H

#include "handlers.h"
#include "http.h"

/*
 * 경로 → 핸들러 연결을 맡는다. 엔드포인트 목록은 협약 11.1, 11.2
 * 경로의 키는 퍼센트 인코딩을 풀어서 핸들러에 넘긴다. 잘못된 인코딩, 빈 키, NUL은 400 INVALID_KEY.
 * *out_response를 채운다. body는 호출한 쪽이 http_response_destroy로 해제한다.
 */
void router_handle_request(const server_context_t *context, const http_request_t *request,
                           http_response_t *out_response);

#endif
