#ifndef API_ERRORS_H
#define API_ERRORS_H

#include "http.h"
#include "store.h"

/* store_status_t와 에러 코드·상태 코드의 대응을 한 곳에 모은다. (협약 11.3) */

typedef enum {
    API_ERROR_INVALID_JSON = 0,
    API_ERROR_INVALID_BODY,
    API_ERROR_INVALID_KEY,
    API_ERROR_INVALID_VALUE,
    API_ERROR_INVALID_TTL,
    API_ERROR_INVALID_PREFIX,
    API_ERROR_KEY_NOT_FOUND,
    API_ERROR_ROUTE_NOT_FOUND,
    API_ERROR_METHOD_NOT_ALLOWED,
    API_ERROR_VALUE_TOO_LARGE,
    API_ERROR_PAYLOAD_TOO_LARGE,
    API_ERROR_INTERNAL_ERROR,
    API_ERROR_KIND_COUNT
} api_error_kind_t;

typedef struct {
    int http_status;
    const char *code;       /* 예 : "INVALID_TTL" */
    const char *message;    /* 한글 설명. 앞에 [함수이름]이 붙는다 */
} api_error_t;

/* store 실패 결과를 에러 종류로 바꾼다. STORE_OK를 넘기지 않는다 */
api_error_kind_t api_errors_from_store_status(store_status_t status);

/*
 * {"error": {"code": ..., "message": "[function_name] ..."}} 응답을 채운다.
 * 해제 책임 : out_response->body는 http_response_destroy가 해제한다.
 * 본문을 만들 메모리가 없으면 본문 없이 500을 채운다.
 */
void api_errors_set_response(http_response_t *out_response, api_error_kind_t kind,
                             const char *function_name);

#endif
