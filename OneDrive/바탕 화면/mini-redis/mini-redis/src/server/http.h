#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

/* 요청 읽기, 응답 쓰기, 본문 크기 제한을 맡는다. (협약 11.5) */

#define MAX_BODY_BYTES 2097152   /* 2MiB */

typedef struct {
    char method[16];
    char *path;          /* URL 디코딩 전 경로. 해제는 http_request_destroy */
    char *body;          /* 본문. 없으면 NULL */
    size_t body_len;
} http_request_t;

/* TODO(이니셜): 요청 읽기·응답 쓰기 함수 시그니처를 정한다 */
void http_request_destroy(http_request_t *request);

#endif
