#include "handlers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api_errors.h"
#include "json.h"
#include "store.h"

/* 입력 검증은 상태를 바꾸기 전에 모두 끝낸다. (협약 11.4) */

#define FIELD_COUNT(fields) (sizeof(fields) / sizeof((fields)[0]))

static const char *const PUT_KEY_FIELDS[] = {"value", "ttlMs"};
static const char *const PUT_TTL_FIELDS[] = {"ttlMs"};
static const char *const INVALIDATE_FIELDS[] = {"prefix"};

static void lock_store(const server_context_t *context) {
    pthread_mutex_lock(context->store_mutex);
}

static void unlock_store(const server_context_t *context) {
    pthread_mutex_unlock(context->store_mutex);
}

static api_error_id_t to_body_error(json_status_t status) {
    switch (status) {
    case JSON_ERR_SYNTAX: return API_ERROR_INVALID_JSON;
    case JSON_ERR_NO_MEMORY: return API_ERROR_INTERNAL;
    default: return API_ERROR_INVALID_BODY;    /* 객체가 아님, 중복 필드 */
    }
}

static bool is_allowed_name(const json_field_t *field, const char *const *allowed_names,
                            size_t allowed_count) {
    for (size_t i = 0; i < allowed_count; i++) {
        if (field->name_len == strlen(allowed_names[i]) &&
            memcmp(field->name, allowed_names[i], field->name_len) == 0) {
            return true;
        }
    }
    return false;
}

/* 본문을 객체로 읽고, 정의되지 않은 필드가 있으면 거부한다. 성공하면 *out_object를 해제해야 한다 */
static bool parse_body(const http_request_t *request, const char *const *allowed_names,
                       size_t allowed_count, json_object_t *out_object,
                       api_error_id_t *out_error) {
    const char *body = request->body != NULL ? request->body : "";
    json_status_t status = json_parse_object(body, request->body_len, out_object);

    if (status != JSON_OK) {
        *out_error = to_body_error(status);
        return false;
    }
    for (size_t i = 0; i < out_object->field_count; i++) {
        if (!is_allowed_name(&out_object->fields[i], allowed_names, allowed_count)) {
            json_object_destroy(out_object);
            *out_error = API_ERROR_INVALID_BODY;
            return false;
        }
    }
    return true;
}

/* 문자열이고 중간에 '\0'(\u0000)이 없어야 C 문자열로 store에 넘길 수 있다 */
static bool is_plain_string(const json_field_t *field) {
    return field->type == JSON_TYPE_STRING && memchr(field->text, '\0', field->text_len) == NULL;
}

/* ttlMs는 정수 값을 가진 숫자이고 1~MAX_TTL_MS 이어야 한다. 1.0은 허용 (협약 11.4) */
static bool read_ttl(const json_field_t *field, int64_t *out_ttl_ms) {
    int64_t ttl_ms = 0;

    if (!json_field_to_int64(field, &ttl_ms) || ttl_ms < 1 || ttl_ms > MAX_TTL_MS) {
        return false;
    }
    *out_ttl_ms = ttl_ms;
    return true;
}

/* 버퍼를 200 응답 본문으로 넘긴다. 버퍼를 만들다 메모리가 모자랐으면 500 */
static void write_ok(json_buffer_t *buffer, const char *function_name,
                     http_response_t *out_response) {
    if (buffer->has_error) {
        json_buffer_destroy(buffer);
        fprintf(stderr, "[%s] 응답을 만들 메모리가 부족합니다\n", function_name);
        api_errors_write(API_ERROR_INTERNAL, function_name, out_response);
        return;
    }
    out_response->status_code = 200;
    out_response->body = buffer->data;
    out_response->body_len = buffer->len;
}

/* {"field_name": true} 모양의 응답 */
static void write_bool(const char *field_name, bool value, const char *function_name,
                       http_response_t *out_response) {
    json_buffer_t buffer = {0};

    json_buffer_append_text(&buffer, "{");
    json_buffer_append_string(&buffer, field_name, strlen(field_name));
    json_buffer_append_text(&buffer, value ? ":true}" : ":false}");
    write_ok(&buffer, function_name, out_response);
}

static void write_store_error(store_status_t status, const char *function_name,
                              http_response_t *out_response) {
    if (status == STORE_ERR_NO_MEMORY) {
        fprintf(stderr, "[%s] 메모리가 부족합니다\n", function_name);
    }
    api_errors_write(api_errors_from_store_status(status), function_name, out_response);
}

/* PUT /keys/{key} 본문 검증. 성공하면 *out_value는 body 안의 문자열을 가리킨다 */
static bool validate_put_key_body(const json_object_t *body, const char **out_value,
                                  int64_t *out_ttl_ms, api_error_id_t *out_error) {
    const json_field_t *value_field = json_object_find(body, "value");
    const json_field_t *ttl_field = json_object_find(body, "ttlMs");

    if (value_field == NULL) {
        *out_error = API_ERROR_INVALID_BODY;
        return false;
    }
    if (!is_plain_string(value_field)) {
        *out_error = API_ERROR_INVALID_VALUE;
        return false;
    }
    *out_ttl_ms = STORE_NO_TTL;
    if (ttl_field != NULL && !read_ttl(ttl_field, out_ttl_ms)) {
        *out_error = API_ERROR_INVALID_TTL;
        return false;
    }
    *out_value = value_field->text;
    return true;
}

void handlers_put_key(const server_context_t *context, const char *key,
                      const http_request_t *request, http_response_t *out_response) {
    json_object_t body = {0};
    api_error_id_t error = API_ERROR_INTERNAL;
    const char *value = NULL;
    int64_t ttl_ms = STORE_NO_TTL;
    bool is_created = false;
    store_status_t status = STORE_OK;

    if (!parse_body(request, PUT_KEY_FIELDS, FIELD_COUNT(PUT_KEY_FIELDS), &body, &error) ||
        !validate_put_key_body(&body, &value, &ttl_ms, &error)) {
        json_object_destroy(&body);
        api_errors_write(error, "store_set", out_response);
        return;
    }
    lock_store(context);
    status = store_set(context->store, key, value, ttl_ms, &is_created);
    unlock_store(context);
    json_object_destroy(&body);
    if (status != STORE_OK) {
        write_store_error(status, "store_set", out_response);
        return;
    }
    write_bool("created", is_created, "store_set", out_response);
}

void handlers_get_key(const server_context_t *context, const char *key,
                      http_response_t *out_response) {
    char *value = NULL;
    store_status_t status = STORE_OK;
    json_buffer_t buffer = {0};

    lock_store(context);
    status = store_get(context->store, key, &value);
    unlock_store(context);
    if (status != STORE_OK) {
        write_store_error(status, "store_get", out_response);
        return;
    }
    json_buffer_append_text(&buffer, "{\"value\":");
    json_buffer_append_string(&buffer, value, strlen(value));
    json_buffer_append_text(&buffer, "}");
    free(value);
    write_ok(&buffer, "store_get", out_response);
}

void handlers_delete_key(const server_context_t *context, const char *key,
                         http_response_t *out_response) {
    bool is_deleted = false;
    store_status_t status = STORE_OK;

    lock_store(context);
    status = store_delete(context->store, key, &is_deleted);
    unlock_store(context);
    if (status != STORE_OK) {
        write_store_error(status, "store_delete", out_response);
        return;
    }
    write_bool("deleted", is_deleted, "store_delete", out_response);
}

void handlers_put_ttl(const server_context_t *context, const char *key,
                      const http_request_t *request, http_response_t *out_response) {
    json_object_t body = {0};
    api_error_id_t error = API_ERROR_INTERNAL;
    const json_field_t *ttl_field = NULL;
    int64_t ttl_ms = 0;
    bool is_applied = false;
    store_status_t status = STORE_OK;

    if (!parse_body(request, PUT_TTL_FIELDS, FIELD_COUNT(PUT_TTL_FIELDS), &body, &error)) {
        api_errors_write(error, "store_expire", out_response);
        return;
    }
    ttl_field = json_object_find(&body, "ttlMs");
    error = (ttl_field == NULL) ? API_ERROR_INVALID_BODY : API_ERROR_INVALID_TTL;
    if (ttl_field == NULL || !read_ttl(ttl_field, &ttl_ms)) {
        json_object_destroy(&body);
        api_errors_write(error, "store_expire", out_response);
        return;
    }
    json_object_destroy(&body);
    lock_store(context);
    status = store_expire(context->store, key, ttl_ms, &is_applied);
    unlock_store(context);
    if (status != STORE_OK) {
        write_store_error(status, "store_expire", out_response);
        return;
    }
    write_bool("applied", is_applied, "store_expire", out_response);
}

void handlers_get_ttl(const server_context_t *context, const char *key,
                      http_response_t *out_response) {
    int64_t ttl_ms = 0;
    store_status_t status = STORE_OK;
    json_buffer_t buffer = {0};

    lock_store(context);
    status = store_ttl(context->store, key, &ttl_ms);
    unlock_store(context);
    if (status != STORE_OK) {
        write_store_error(status, "store_ttl", out_response);
        return;
    }
    json_buffer_append_text(&buffer, "{\"ttlMs\":");
    json_buffer_append_int(&buffer, ttl_ms);
    json_buffer_append_text(&buffer, "}");
    write_ok(&buffer, "store_ttl", out_response);
}

void handlers_get_exists(const server_context_t *context, const char *key,
                         http_response_t *out_response) {
    bool exists = false;
    store_status_t status = STORE_OK;

    lock_store(context);
    status = store_exists(context->store, key, &exists);
    unlock_store(context);
    if (status != STORE_OK) {
        write_store_error(status, "store_exists", out_response);
        return;
    }
    write_bool("exists", exists, "store_exists", out_response);
}

/* POST /invalidate 본문 검증. 성공하면 *out_prefix는 body 안의 문자열을 가리킨다 */
static bool validate_invalidate_body(const json_object_t *body, const char **out_prefix,
                                     api_error_id_t *out_error) {
    const json_field_t *prefix_field = json_object_find(body, "prefix");

    if (prefix_field == NULL) {
        *out_error = API_ERROR_INVALID_BODY;
        return false;
    }
    if (!is_plain_string(prefix_field)) {
        *out_error = API_ERROR_INVALID_PREFIX;
        return false;
    }
    *out_prefix = prefix_field->text;
    return true;
}

void handlers_post_invalidate(const server_context_t *context, const http_request_t *request,
                              http_response_t *out_response) {
    json_object_t body = {0};
    api_error_id_t error = API_ERROR_INTERNAL;
    const char *prefix = NULL;
    size_t deleted_count = 0;
    store_status_t status = STORE_OK;
    json_buffer_t buffer = {0};

    if (!parse_body(request, INVALIDATE_FIELDS, FIELD_COUNT(INVALIDATE_FIELDS), &body, &error) ||
        !validate_invalidate_body(&body, &prefix, &error)) {
        json_object_destroy(&body);
        api_errors_write(error, "store_delete_by_prefix", out_response);
        return;
    }
    lock_store(context);
    status = store_delete_by_prefix(context->store, prefix, &deleted_count);
    unlock_store(context);
    json_object_destroy(&body);
    if (status != STORE_OK) {
        write_store_error(status, "store_delete_by_prefix", out_response);
        return;
    }
    json_buffer_append_text(&buffer, "{\"deletedCount\":");
    json_buffer_append_uint(&buffer, deleted_count);
    json_buffer_append_text(&buffer, "}");
    write_ok(&buffer, "store_delete_by_prefix", out_response);
}

void handlers_get_stats(const server_context_t *context, http_response_t *out_response) {
    store_stats_t stats;
    json_buffer_t buffer = {0};

    lock_store(context);
    store_get_stats(context->store, &stats);
    unlock_store(context);
    json_buffer_append_text(&buffer, "{\"size\":");
    json_buffer_append_uint(&buffer, stats.size);
    json_buffer_append_text(&buffer, ",\"capacity\":");
    json_buffer_append_uint(&buffer, stats.capacity);
    json_buffer_append_text(&buffer, ",\"hitCount\":");
    json_buffer_append_uint(&buffer, stats.hit_count);
    json_buffer_append_text(&buffer, ",\"missCount\":");
    json_buffer_append_uint(&buffer, stats.miss_count);
    json_buffer_append_text(&buffer, ",\"expiredCount\":");
    json_buffer_append_uint(&buffer, stats.expired_count);
    json_buffer_append_text(&buffer, "}");
    write_ok(&buffer, "store_get_stats", out_response);
}

void handlers_get_health(http_response_t *out_response) {
    json_buffer_t buffer = {0};

    json_buffer_append_text(&buffer, "{\"status\":\"ok\"}");
    write_ok(&buffer, "handlers_get_health", out_response);
}
