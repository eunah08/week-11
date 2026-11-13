#include "api_errors.h"

#include <stdio.h>
#include <string.h>

#include "json.h"

/* 순서는 api_error_id_t와 같다 (협약 11.3의 표) */
static const api_error_t API_ERROR_TABLE[] = {
    [API_ERROR_INVALID_JSON] = {400, "INVALID_JSON", "본문이 올바른 JSON이 아닙니다"},
    [API_ERROR_INVALID_BODY] = {400, "INVALID_BODY",
                                "본문 형식이 잘못되었습니다 (객체, 필드 누락·중복·정의되지 않은 필드)"},
    [API_ERROR_INVALID_KEY] = {400, "INVALID_KEY",
                               "키는 1~256바이트의 UTF-8 문자열이어야 하고 NUL을 포함할 수 없습니다"},
    [API_ERROR_INVALID_VALUE] = {400, "INVALID_VALUE",
                                 "value는 NUL이 없는 문자열이어야 합니다"},
    [API_ERROR_INVALID_TTL] = {400, "INVALID_TTL",
                               "ttlMs는 1~2147483647 사이의 정수여야 합니다"},
    [API_ERROR_INVALID_PREFIX] = {400, "INVALID_PREFIX",
                                  "prefix는 비어 있지 않은 문자열이어야 합니다"},
    [API_ERROR_KEY_NOT_FOUND] = {404, "KEY_NOT_FOUND", "키가 없거나 만료되었습니다"},
    [API_ERROR_ROUTE_NOT_FOUND] = {404, "ROUTE_NOT_FOUND", "정의되지 않은 경로입니다"},
    [API_ERROR_METHOD_NOT_ALLOWED] = {405, "METHOD_NOT_ALLOWED",
                                      "이 경로에서 허용하지 않는 메서드입니다"},
    [API_ERROR_VALUE_TOO_LARGE] = {413, "VALUE_TOO_LARGE", "값이 1MiB를 넘습니다"},
    [API_ERROR_PAYLOAD_TOO_LARGE] = {413, "PAYLOAD_TOO_LARGE", "본문이 2MiB를 넘습니다"},
    [API_ERROR_INTERNAL] = {500, "INTERNAL_ERROR", "서버 내부 오류입니다"},
};

api_error_t api_errors_get(api_error_id_t error_id) {
    size_t table_len = sizeof(API_ERROR_TABLE) / sizeof(API_ERROR_TABLE[0]);

    if ((size_t)error_id >= table_len) {
        return API_ERROR_TABLE[API_ERROR_INTERNAL];
    }
    return API_ERROR_TABLE[error_id];
}

api_error_id_t api_errors_from_store_status(store_status_t status) {
    switch (status) {
    case STORE_NOT_FOUND: return API_ERROR_KEY_NOT_FOUND;
    case STORE_ERR_INVALID_KEY: return API_ERROR_INVALID_KEY;
    case STORE_ERR_INVALID_VALUE: return API_ERROR_INVALID_VALUE;
    case STORE_ERR_VALUE_TOO_LARGE: return API_ERROR_VALUE_TOO_LARGE;
    case STORE_ERR_INVALID_TTL: return API_ERROR_INVALID_TTL;
    case STORE_ERR_INVALID_PREFIX: return API_ERROR_INVALID_PREFIX;
    case STORE_OK:
    case STORE_ERR_NO_MEMORY:
        break;
    }
    return API_ERROR_INTERNAL;
}

void api_errors_write(api_error_id_t error_id, const char *function_name,
                      http_response_t *out_response) {
    api_error_t error = api_errors_get(error_id);
    json_buffer_t buffer = {0};

    json_buffer_append_text(&buffer, "{\"error\":{\"code\":");
    json_buffer_append_string(&buffer, error.code, strlen(error.code));
    json_buffer_append_text(&buffer, ",\"message\":\"[");
    json_buffer_append_text(&buffer, function_name);
    json_buffer_append_text(&buffer, "] ");
    json_buffer_append_text(&buffer, error.description);
    json_buffer_append_text(&buffer, "\"}}");
    out_response->status_code = error.http_status;
    if (buffer.has_error) {
        fprintf(stderr, "[api_errors_write] 에러 응답을 만들 메모리가 부족합니다\n");
        json_buffer_destroy(&buffer);
        out_response->status_code = API_ERROR_TABLE[API_ERROR_INTERNAL].http_status;
    }
    out_response->body = buffer.data;
    out_response->body_len = buffer.len;
}
