#include "handlers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api_errors.h"
#include "json.h"

/* 입력 검증은 상태를 바꾸기 전에 모두 끝낸다. (협약 11.4) */

#define HTTP_STATUS_OK 200
#define SMALL_BODY_BYTES 256
#define BODY_WRAPPER_BYTES 8      /* {"": } 와 NUL */
#define MIN_API_TTL_MS 1
#define VALUE_FIELD "value"
#define TTL_FIELD "ttlMs"
#define PREFIX_FIELD "prefix"

/* ---------- 응답 만들기 ---------- */

static void set_json_response(http_response_t *out_response, char *body) {
    if (body == NULL) {
        api_errors_set_response(out_response, API_ERROR_INTERNAL_ERROR, "handlers");
        return;
    }
    out_response->http_status = HTTP_STATUS_OK;
    out_response->body = body;
}

static void set_store_error(http_response_t *out_response, store_status_t status,
                            const char *function_name) {
    api_errors_set_response(out_response, api_errors_from_store_status(status), function_name);
}

/* {"name": true}. 해제 책임 : 호출한 쪽이 free() */
static char *create_bool_body(const char *name, bool flag) {
    char body[SMALL_BODY_BYTES];
    snprintf(body, sizeof(body), "{\"%s\": %s}", name, flag ? "true" : "false");
    return strdup(body);
}

/* {"name": 123}. 해제 책임 : 호출한 쪽이 free() */
static char *create_number_body(const char *name, long long number) {
    char body[SMALL_BODY_BYTES];
    snprintf(body, sizeof(body), "{\"%s\": %lld}", name, number);
    return strdup(body);
}

/* {"name": "이스케이프한 문자열"}. 해제 책임 : 호출한 쪽이 free() */
static char *create_string_body(const char *name, const char *text) {
    char *escaped = json_escape_string(text);
    if (escaped == NULL) {
        return NULL;
    }
    size_t body_size = strlen(name) + strlen(escaped) + BODY_WRAPPER_BYTES;
    char *body = malloc(body_size);
    if (body != NULL) {
        snprintf(body, body_size, "{\"%s\": %s}", name, escaped);
    }
    free(escaped);
    return body;
}

/* ---------- 본문 검증 ---------- */

static bool is_allowed_field(const json_field_t *field, const char *first_name,
                             const char *second_name) {
    if (json_is_field_named(field, first_name)) {
        return true;
    }
    return second_name != NULL && json_is_field_named(field, second_name);
}

/* 허용한 이름만 있고, 같은 이름이 두 번 나오지 않는지 */
static bool has_only_allowed_fields(const json_object_t *object, const char *first_name,
                                    const char *second_name) {
    size_t max_field_count = second_name != NULL ? 2 : 1;
    if (object->field_count > max_field_count || json_has_duplicate_field(object)) {
        return false;
    }
    for (size_t i = 0; i < object->field_count; i++) {
        if (!is_allowed_field(&object->fields[i], first_name, second_name)) {
            return false;
        }
    }
    return true;
}

/*
 * 본문을 최상위 JSON 객체로 해석하고 정의되지 않은 필드·중복 필드를 거부한다.
 * second_name은 허용할 두 번째 필드 이름. 없으면 NULL.
 * 해제 책임 : 성공하면 호출한 쪽이 json_object_destroy
 */
static bool parse_request_body(const http_request_t *request, const char *first_name,
                               const char *second_name, json_object_t *out_object,
                               api_error_kind_t *out_error_kind) {
    json_parse_status_t status = json_parse_object(request->body, request->body_len, out_object);
    if (status == JSON_PARSE_NO_MEMORY) {
        *out_error_kind = API_ERROR_INTERNAL_ERROR;
        return false;
    }
    if (status == JSON_PARSE_INVALID_JSON) {
        *out_error_kind = API_ERROR_INVALID_JSON;
        return false;
    }
    if (status == JSON_PARSE_NOT_OBJECT
        || !has_only_allowed_fields(out_object, first_name, second_name)) {
        json_object_destroy(out_object);
        *out_error_kind = API_ERROR_INVALID_BODY;
        return false;
    }
    return true;
}

/* NUL이 들어 있지 않은 문자열 필드인지 (협약 7.1) */
static bool is_plain_string(const json_field_t *field) {
    return field->type == JSON_TYPE_STRING && strlen(field->string_value) == field->string_len;
}

/* ttlMs는 1~MAX_TTL_MS의 정수 값. 1.0은 허용하고 1.5는 거부한다 (협약 11.4) */
static bool read_ttl_ms(const json_field_t *field, int64_t *out_ttl_ms) {
    if (field->type != JSON_TYPE_NUMBER) {
        return false;
    }
    double number = field->number_value;
    if (number < MIN_API_TTL_MS || number > MAX_TTL_MS) {
        return false;
    }
    if ((double)(int64_t)number != number) {
        return false;
    }
    *out_ttl_ms = (int64_t)number;
    return true;
}

/* PUT /keys/{key}의 필드를 꺼낸다. *out_value는 body 안을 가리키므로 body보다 먼저 해제하지 않는다 */
static bool read_put_key_fields(const json_object_t *body, const char **out_value,
                                int64_t *out_ttl_ms, api_error_kind_t *out_error_kind) {
    const json_field_t *value_field = json_find_field(body, VALUE_FIELD);
    const json_field_t *ttl_field = json_find_field(body, TTL_FIELD);

    if (value_field == NULL) {
        *out_error_kind = API_ERROR_INVALID_BODY;
        return false;
    }
    if (!is_plain_string(value_field)) {
        *out_error_kind = API_ERROR_INVALID_VALUE;
        return false;
    }
    *out_ttl_ms = STORE_NO_TTL;
    if (ttl_field != NULL && !read_ttl_ms(ttl_field, out_ttl_ms)) {
        *out_error_kind = API_ERROR_INVALID_TTL;
        return false;
    }
    *out_value = value_field->string_value;
    return true;
}

/* PUT /keys/{key}/ttl의 필드를 꺼낸다 */
static bool read_put_ttl_fields(const json_object_t *body, int64_t *out_ttl_ms,
                                api_error_kind_t *out_error_kind) {
    const json_field_t *ttl_field = json_find_field(body, TTL_FIELD);
    if (ttl_field == NULL) {
        *out_error_kind = API_ERROR_INVALID_BODY;
        return false;
    }
    if (!read_ttl_ms(ttl_field, out_ttl_ms)) {
        *out_error_kind = API_ERROR_INVALID_TTL;
        return false;
    }
    return true;
}

/* ---------- 핸들러 ---------- */

void handlers_put_key(const handlers_context_t *context, const char *key,
                      const http_request_t *request, http_response_t *out_response) {
    json_object_t body;
    api_error_kind_t error_kind = API_ERROR_INTERNAL_ERROR;
    if (!parse_request_body(request, VALUE_FIELD, TTL_FIELD, &body, &error_kind)) {
        api_errors_set_response(out_response, error_kind, "handlers_put_key");
        return;
    }

    const char *value = NULL;
    int64_t ttl_ms = STORE_NO_TTL;
    bool is_created = false;
    store_status_t status = STORE_OK;
    bool is_valid = read_put_key_fields(&body, &value, &ttl_ms, &error_kind);
    if (is_valid) {
        pthread_mutex_lock(context->store_lock);
        status = store_set(context->store, key, value, ttl_ms, &is_created);
        pthread_mutex_unlock(context->store_lock);
    }
    json_object_destroy(&body);

    if (!is_valid) {
        api_errors_set_response(out_response, error_kind, "handlers_put_key");
    } else if (status != STORE_OK) {
        set_store_error(out_response, status, "store_set");
    } else {
        set_json_response(out_response, create_bool_body("created", is_created));
    }
}

void handlers_get_key(const handlers_context_t *context, const char *key,
                      http_response_t *out_response) {
    char *value = NULL;

    pthread_mutex_lock(context->store_lock);
    store_status_t status = store_get(context->store, key, &value);
    pthread_mutex_unlock(context->store_lock);

    if (status != STORE_OK) {
        set_store_error(out_response, status, "store_get");
        return;
    }
    set_json_response(out_response, create_string_body("value", value));
    free(value);
}

void handlers_delete_key(const handlers_context_t *context, const char *key,
                         http_response_t *out_response) {
    bool is_deleted = false;

    pthread_mutex_lock(context->store_lock);
    store_status_t status = store_delete(context->store, key, &is_deleted);
    pthread_mutex_unlock(context->store_lock);

    if (status != STORE_OK) {
        set_store_error(out_response, status, "store_delete");
        return;
    }
    set_json_response(out_response, create_bool_body("deleted", is_deleted));
}

void handlers_put_ttl(const handlers_context_t *context, const char *key,
                      const http_request_t *request, http_response_t *out_response) {
    json_object_t body;
    api_error_kind_t error_kind = API_ERROR_INTERNAL_ERROR;
    int64_t ttl_ms = 0;
    if (!parse_request_body(request, TTL_FIELD, NULL, &body, &error_kind)) {
        api_errors_set_response(out_response, error_kind, "handlers_put_ttl");
        return;
    }
    bool is_valid = read_put_ttl_fields(&body, &ttl_ms, &error_kind);
    json_object_destroy(&body);
    if (!is_valid) {
        api_errors_set_response(out_response, error_kind, "handlers_put_ttl");
        return;
    }

    bool is_applied = false;
    pthread_mutex_lock(context->store_lock);
    store_status_t status = store_expire(context->store, key, ttl_ms, &is_applied);
    pthread_mutex_unlock(context->store_lock);

    if (status != STORE_OK) {
        set_store_error(out_response, status, "store_expire");
        return;
    }
    set_json_response(out_response, create_bool_body("applied", is_applied));
}

void handlers_get_ttl(const handlers_context_t *context, const char *key,
                      http_response_t *out_response) {
    int64_t ttl_ms = 0;

    pthread_mutex_lock(context->store_lock);
    store_status_t status = store_ttl(context->store, key, &ttl_ms);
    pthread_mutex_unlock(context->store_lock);

    if (status != STORE_OK) {
        set_store_error(out_response, status, "store_ttl");
        return;
    }
    set_json_response(out_response, create_number_body("ttlMs", (long long)ttl_ms));
}

void handlers_post_invalidate(const handlers_context_t *context, const http_request_t *request,
                              http_response_t *out_response) {
    json_object_t body;
    api_error_kind_t error_kind = API_ERROR_INTERNAL_ERROR;
    if (!parse_request_body(request, PREFIX_FIELD, NULL, &body, &error_kind)) {
        api_errors_set_response(out_response, error_kind, "handlers_post_invalidate");
        return;
    }
    const json_field_t *prefix_field = json_find_field(&body, PREFIX_FIELD);
    if (prefix_field == NULL || !is_plain_string(prefix_field)) {
        error_kind = prefix_field == NULL ? API_ERROR_INVALID_BODY : API_ERROR_INVALID_PREFIX;
        json_object_destroy(&body);
        api_errors_set_response(out_response, error_kind, "handlers_post_invalidate");
        return;
    }

    size_t deleted_count = 0;
    pthread_mutex_lock(context->store_lock);
    store_status_t status =
        store_delete_by_prefix(context->store, prefix_field->string_value, &deleted_count);
    pthread_mutex_unlock(context->store_lock);
    json_object_destroy(&body);

    if (status != STORE_OK) {
        set_store_error(out_response, status, "store_delete_by_prefix");
        return;
    }
    set_json_response(out_response, create_number_body("deletedCount", (long long)deleted_count));
}

void handlers_get_stats(const handlers_context_t *context, http_response_t *out_response) {
    store_stats_t stats;
    char body[SMALL_BODY_BYTES];

    pthread_mutex_lock(context->store_lock);
    store_get_stats(context->store, &stats);
    pthread_mutex_unlock(context->store_lock);

    snprintf(body, sizeof(body),
             "{\"size\": %zu, \"capacity\": %zu, \"hitCount\": %llu, \"missCount\": %llu, "
             "\"expiredCount\": %llu}",
             stats.size, stats.capacity, (unsigned long long)stats.hit_count,
             (unsigned long long)stats.miss_count, (unsigned long long)stats.expired_count);
    set_json_response(out_response, strdup(body));
}
