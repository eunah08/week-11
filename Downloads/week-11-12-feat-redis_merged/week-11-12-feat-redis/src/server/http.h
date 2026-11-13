#ifndef HTTP_H
#define HTTP_H

#include <stdbool.h>
#include <stddef.h>

/*
 * 요청 읽기, 응답 쓰기, 본문 크기 제한을 맡는다. (협약 11.5)
 * store를 모른다. 샘플 API(demo)도 같은 모듈로 요청을 읽고 응답을 쓴다.
 * 연결 하나에 요청 하나만 처리하고 닫는다 (keep-alive 없음).
 */

#define MAX_BODY_BYTES 2097152     /* 2MiB */
#define MAX_HEADER_BYTES 16384     /* 요청 줄 + 헤더 전체 */
#define HTTP_METHOD_BYTES 16
#define HTTP_REQUEST_TIMEOUT_MS 10000    /* 요청 하나(헤더 + 본문)를 다 받는 전체 마감 */

typedef struct {
    char method[HTTP_METHOD_BYTES];
    char *path;          /* URL 디코딩 전 경로 (쿼리 포함). 해제는 http_request_destroy */
    char *body;          /* 본문. 없으면 NULL. 끝에 '\0'을 붙여 둔다 */
    size_t body_len;
} http_request_t;

typedef enum {
    HTTP_READ_OK = 0,
    HTTP_READ_CLOSED,          /* 요청을 다 받기 전에 연결이 끊기거나 시간 초과. 응답하지 않는다 */
    HTTP_READ_BAD_REQUEST,     /* 요청 줄·헤더 형식 오류, 헤더가 MAX_HEADER_BYTES 초과 */
    HTTP_READ_UNSUPPORTED_BODY,/* chunked 전송 (지원하지 않음 → 400 INVALID_BODY) */
    HTTP_READ_TOO_LARGE,       /* Content-Length가 MAX_BODY_BYTES 초과 → 413 */
    HTTP_READ_NO_MEMORY
} http_read_status_t;

typedef struct {
    int status_code;
    const char *extra_headers;  /* "X-Cache: HIT\r\n"처럼 줄바꿈까지 포함. 없으면 NULL */
    char *body;                 /* JSON 본문. 해제는 http_response_destroy */
    size_t body_len;
} http_response_t;

typedef enum {
    HTTP_DECODE_OK = 0,
    HTTP_DECODE_INVALID,      /* "%" 뒤가 16진수 두 자리가 아니다 → 400 */
    HTTP_DECODE_NO_MEMORY     /* → 500 */
} http_decode_status_t;

/*
 * fd에서 요청 하나를 읽는다. Content-Length만큼 본문을 읽고, 넘치면 읽기를 멈춘다.
 * HTTP_REQUEST_TIMEOUT_MS 안에 다 받지 못하면 HTTP_READ_CLOSED다.
 * 반환 : HTTP_READ_OK면 *out_request를 채운다. 그 밖에는 *out_request는 빈 상태
 * 메모리 : 호출한 쪽이 http_request_destroy로 해제한다
 */
http_read_status_t http_read_request(int fd, http_request_t *out_request);

void http_request_destroy(http_request_t *request);

/*
 * 응답을 끝까지 쓴다. 끊긴 연결에 써도 SIGPIPE로 죽지 않게 MSG_NOSIGNAL을 쓴다.
 * 반환 : 다 썼으면 참
 */
bool http_write_response(int fd, const http_response_t *response);

void http_response_destroy(http_response_t *response);

/*
 * 쓰기를 닫고, 상대가 아직 보내는 본문을 일정량 읽어 버린 뒤 연결을 닫는다.
 * 413처럼 본문을 다 읽지 않고 응답했을 때 상대가 응답을 받기 전에 연결이 리셋되는 것을 막는다.
 */
void http_close_connection(int fd);

/*
 * URL 퍼센트 인코딩을 푼다. 형식 오류와 메모리 부족을 구분해 돌려준다.
 * 결과에 '\0'(%00)이 섞일 수 있으므로 길이를 *out_len으로 돌려준다.
 * 메모리 : HTTP_DECODE_OK면 *out_text는 호출한 쪽이 free()
 */
http_decode_status_t http_decode_percent(const char *text, size_t text_len, char **out_text,
                                         size_t *out_len);

/* 상태 코드의 사유 문구. 모르는 코드면 "Unknown" */
const char *http_find_status_text(int status_code);

#endif
