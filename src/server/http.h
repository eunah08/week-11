#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

#define MAX_BODY_BYTES 2097152
#define HTTP_TIMEOUT_MS 2000
#define HTTP_MAX_HEADERS 8192

typedef struct {
    char method[16];
    char *path;
    char *body;
    size_t body_len;
} http_request_t;
typedef struct {
    int status;
    char *body;
    const char *extra_headers;
} http_response_t;
typedef void (*http_handler_fn)(const http_request_t *, http_response_t *, void *);

/* 요청 문자열은 read가 소유권을 넘기며 destroy로 해제. 실패 HTTP 상태 반환. */
int http_read(int fd, http_request_t *out_request);
void http_request_destroy(http_request_t *request);
/* 응답 body는 호출자가 소유. 네트워크 실패 시 -1, O(body 길이). */
int http_write(int fd, const http_response_t *response);
/* body 복사본 할당. 실패 시 body=NULL이며 write가 500으로 변환한다. */
void http_response_json(http_response_t *response, int status, const char *body);
void http_response_error(http_response_t *response, int status, const char *code,
                          const char *function);
/* 포트 서버를 실행. 신호 종료 후 모든 요청 스레드 종료를 기다린다. 실패 -1. */
int http_serve(int port, http_handler_fn handler, void *context);
typedef struct {
    int port;
    const char *method;
    const char *path;
    const char *body;
} http_client_request_t;

/* loopback HTTP 클라이언트. 성공 0, 실패 -1. 응답 body는 호출자가 free. */
int http_request(const http_client_request_t *request, http_response_t *out_response);
/* 퍼센트 디코딩. 실패 NULL(errno=EINVAL/ENOMEM), 결과 호출자 free. '+' 보존. */
char *http_decode_key(const char *input, size_t len);

#endif
