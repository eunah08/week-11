#ifndef HTTP_H
#define HTTP_H

#include <stdbool.h>
#include <stddef.h>

/* 요청 읽기, 응답 쓰기, 본문 크기 제한을 맡는다. (협약 11.5) */

#define MAX_BODY_BYTES 2097152   /* 2MiB */
#define MAX_HEADER_BYTES 8192    /* 요청 줄 + 헤더 전체 상한 */

typedef struct {
    char method[16];
    char *path;          /* URL 디코딩 전 경로 (쿼리 포함). 해제는 http_request_destroy */
    char *body;          /* 본문. 없으면 NULL. 끝에 NUL이 붙어 있다 */
    size_t body_len;
} http_request_t;

typedef struct {
    int http_status;
    const char *extra_headers;   /* 예 : "X-Cache: HIT\r\n". 없으면 NULL. 상수 문자열이라 해제하지 않는다 */
    char *body;                  /* JSON 본문. 해제는 http_response_destroy */
} http_response_t;

typedef enum {
    HTTP_READ_OK = 0,
    HTTP_READ_CLOSED,        /* 연결이 끊겼거나 시간 초과. 응답하지 않는다 */
    HTTP_READ_BAD_REQUEST,   /* 요청 줄·헤더 형식 오류, chunked 전송 */
    HTTP_READ_TOO_LARGE,     /* 본문이 MAX_BODY_BYTES 초과 */
    HTTP_READ_NO_MEMORY
} http_read_status_t;

typedef enum {
    HTTP_DECODE_OK = 0,
    HTTP_DECODE_INVALID,     /* 잘못된 퍼센트 인코딩, %00 */
    HTTP_DECODE_NO_MEMORY
} http_decode_status_t;

/*
 * 요청 하나를 읽는다. 본문은 Content-Length만큼 읽는다.
 * Content-Length가 MAX_BODY_BYTES를 넘으면 본문을 읽지 않고 HTTP_READ_TOO_LARGE.
 * 해제 책임 : 성공하면 호출한 쪽이 http_request_destroy로 해제한다. 실패하면 비워 둔다.
 */
http_read_status_t http_read_request(int fd, http_request_t *out_request);

/* 상태 줄, 헤더, 본문을 쓴다. 연결은 한 요청마다 닫는다 (Connection: close). 실패하면 false */
bool http_write_response(int fd, const http_response_t *response);

/* data_len바이트를 모두 쓴다. 끊긴 연결이면 false */
bool http_write_all(int fd, const char *data, size_t data_len);

/* 읽기·쓰기 시간 제한을 건다. 멈춘 상대 때문에 스레드가 영원히 기다리지 않게 한다 */
void http_set_timeout(int fd, int timeout_ms);

/*
 * text_len바이트의 퍼센트 인코딩을 푼다.
 * 해제 책임 : 성공하면 호출한 쪽이 *out_decoded를 free(). 실패하면 *out_decoded는 NULL
 */
http_decode_status_t http_decode_url(const char *text, size_t text_len, char **out_decoded);

/* 영문·숫자·-._~ 밖의 바이트를 %XX로 바꾼다. 해제 책임 : 호출한 쪽이 free(). 메모리 부족이면 NULL */
char *http_encode_url(const char *text);

void http_request_destroy(http_request_t *request);
void http_response_destroy(http_response_t *response);

#endif
