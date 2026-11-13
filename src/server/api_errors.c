#include "api_errors.h"

#include <stdio.h>
#include <string.h>

#define ERROR_BODY_BYTES 512
#define HTTP_STATUS_INTERNAL_ERROR 500

static const api_error_t API_ERRORS[API_ERROR_KIND_COUNT] = {
    [API_ERROR_INVALID_JSON] = {400, "INVALID_JSON", "본문이 올바른 JSON이 아닙니다"},
    [API_ERROR_INVALID_BODY] = {400, "INVALID_BODY",
                                "본문은 정의된 필드만 한 번씩 담은 JSON 객체여야 합니다"},
    [API_ERROR_INVALID_KEY] = {400, "INVALID_KEY", "키는 1~256바이트이고 NUL이 없어야 합니다"},
    [API_ERROR_INVALID_VALUE] = {400, "INVALID_VALUE", "value는 NUL이 없는 문자열이어야 합니다"},
    [API_ERROR_INVALID_TTL] = {400, "INVALID_TTL", "ttlMs는 1~2147483647 사이의 정수여야 합니다"},
    [API_ERROR_INVALID_PREFIX] = {400, "INVALID_PREFIX",
                                  "prefix는 비어 있지 않은 문자열이어야 합니다"},
    [API_ERROR_KEY_NOT_FOUND] = {404, "KEY_NOT_FOUND", "키가 없거나 만료되었습니다"},
    [API_ERROR_ROUTE_NOT_FOUND] = {404, "ROUTE_NOT_FOUND", "정의되지 않은 경로입니다"},
    [API_ERROR_METHOD_NOT_ALLOWED] = {405, "METHOD_NOT_ALLOWED", "허용하지 않는 메서드입니다"},
    [API_ERROR_VALUE_TOO_LARGE] = {413, "VALUE_TOO_LARGE", "value는 1MiB를 넘을 수 없습니다"},
    [API_ERROR_PAYLOAD_TOO_LARGE] = {413, "PAYLOAD_TOO_LARGE", "본문은 2MiB를 넘을 수 없습니다"},
    [API_ERROR_INTERNAL_ERROR] = {500, "INTERNAL_ERROR", "서버 내부 오류입니다"},
};

api_error_kind_t api_errors_from_store_status(store_status_t status) {
    switch (status) {
    case STORE_NOT_FOUND:
        return API_ERROR_KEY_NOT_FOUND;
    case STORE_ERR_INVALID_KEY:
        return API_ERROR_INVALID_KEY;
    case STORE_ERR_INVALID_VALUE:
        return API_ERROR_INVALID_VALUE;
    case STORE_ERR_VALUE_TOO_LARGE:
        return API_ERROR_VALUE_TOO_LARGE;
    case STORE_ERR_INVALID_TTL:
        return API_ERROR_INVALID_TTL;
    case STORE_ERR_INVALID_PREFIX:
        return API_ERROR_INVALID_PREFIX;
    case STORE_OK:
    case STORE_ERR_NO_MEMORY:
        break;
    }
    return API_ERROR_INTERNAL_ERROR;
}

void api_errors_set_response(http_response_t *out_response, api_error_kind_t kind,
                             const char *function_name) {
    const api_error_t *error = &API_ERRORS[kind];
    char body[ERROR_BODY_BYTES];

    if (kind == API_ERROR_INTERNAL_ERROR) {
        fprintf(stderr, "[%s] 내부 오류 (메모리 부족 등)\n", function_name);
    }
    snprintf(body, sizeof(body), "{\"error\": {\"code\": \"%s\", \"message\": \"[%s] %s\"}}",
             error->code, function_name, error->message);

    out_response->http_status = error->http_status;
    out_response->body = strdup(body);
    if (out_response->body == NULL) {
        fprintf(stderr, "[api_errors_set_response] 에러 응답을 만들 메모리가 부족합니다\n");
        out_response->http_status = HTTP_STATUS_INTERNAL_ERROR;
    }
}
