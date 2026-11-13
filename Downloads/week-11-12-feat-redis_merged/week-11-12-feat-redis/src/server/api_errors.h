#ifndef API_ERRORS_H
#define API_ERRORS_H

#include "http.h"
#include "store.h"

/* store_status_t와 에러 코드·상태 코드의 대응을 한 곳에 모은다. (협약 11.3) */

typedef enum {
    API_ERROR_INVALID_JSON,
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
    API_ERROR_INTERNAL
} api_error_id_t;

typedef struct {
    int http_status;
    const char *code;           /* 예 : "INVALID_TTL" */
    const char *description;    /* message에서 "[함수이름] " 뒤에 붙는 한글 설명 */
} api_error_t;

/* 에러 하나의 상태 코드, code, 설명. 표는 api_errors.c 한 곳에 있다 */
api_error_t api_errors_get(api_error_id_t error_id);

/* store 결과를 에러로 바꾼다. STORE_OK를 넘기면 부르는 쪽 실수이므로 API_ERROR_INTERNAL */
api_error_id_t api_errors_from_store_status(store_status_t status);

/*
 * {"error": {"code": "...", "message": "[function_name] 설명"}} 응답을 *out_response에 만든다.
 * 메모리 : out_response->body는 호출한 쪽이 http_response_destroy로 해제한다.
 *          본문을 만들 메모리도 없으면 본문 없는 500 응답이 된다
 */
void api_errors_write(api_error_id_t error_id, const char *function_name,
                      http_response_t *out_response);

#endif
