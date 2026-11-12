#include "handlers.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "api_errors.h"
#include "json.h"

typedef struct {
    const char *value;
    const char *prefix;
    int64_t ttl_ms;
} validated_body_t;
typedef struct {
    store_status_t status;
    bool changed;
    int64_t ttl_ms;
    size_t deleted_count;
    char *value;
    store_stats_t stats;
} store_result_t;

static const char *route_function(const route_t *route, const http_request_t *request) {
    if (route->kind == ROUTE_STATS) { return "store_get_stats"; }
    if (route->kind == ROUTE_INVALIDATE) { return "store_delete_by_prefix"; }
    if (route->kind == ROUTE_TTL) {
        return strcmp(request->method, "GET") == 0 ? "store_ttl" : "store_expire";
    }
    if (strcmp(request->method, "GET") == 0) { return "store_get"; }
    return strcmp(request->method, "PUT") == 0 ? "store_set" : "store_delete";
}

static bool has_valid_fields(const json_object_t *object, route_kind_t kind) {
    if (!object->is_object || object->has_duplicate || object->has_overflow) { return false; }
    for (size_t i = 0; i < object->count; i++) {
        const json_field_t *field = &object->fields[i];
        if (strlen(field->name) != field->name_len) { return false; }
        bool allowed = kind == ROUTE_VALUE
            ? strcmp(field->name, "value") == 0 || strcmp(field->name, "ttlMs") == 0
            : strcmp(field->name, kind == ROUTE_TTL ? "ttlMs" : "prefix") == 0;
        if (!allowed) { return false; }
    }
    return true;
}

static const char *validate_fields(const json_object_t *object, route_kind_t kind,
                                    validated_body_t *body) {
    if (!has_valid_fields(object, kind)) { return "INVALID_BODY"; }
    const json_field_t *value = json_find_field(object, "value");
    const json_field_t *prefix = json_find_field(object, "prefix");
    const json_field_t *ttl = json_find_field(object, "ttlMs");
    if ((kind == ROUTE_VALUE && value == NULL) || (kind == ROUTE_TTL && ttl == NULL)
        || (kind == ROUTE_INVALIDATE && prefix == NULL)) { return "INVALID_BODY"; }
    if (value != NULL) {
        if (value->kind != JSON_STRING || strlen(value->string) != value->string_len) {
            return "INVALID_VALUE";
        }
        if (value->string_len > MAX_VALUE_BYTES) { return "VALUE_TOO_LARGE"; }
        body->value = value->string;
    }
    if (prefix != NULL) {
        if (prefix->kind != JSON_STRING || prefix->string_len == 0
            || strlen(prefix->string) != prefix->string_len) { return "INVALID_PREFIX"; }
        body->prefix = prefix->string;
    }
    if (ttl != NULL) {
        if (ttl->kind != JSON_NUMBER || !ttl->is_integer || !isfinite(ttl->number)
            || ttl->number < 1
            || ttl->number > MAX_TTL_MS || floor(ttl->number) != ttl->number) {
            return "INVALID_TTL";
        }
        body->ttl_ms = (int64_t)ttl->number;
    }
    return NULL;
}

static void execute_value(server_context_t *server, const route_t *route,
                            const http_request_t *request, const validated_body_t *body,
                            store_result_t *result) {
    if (strcmp(request->method, "PUT") == 0) {
        result->status = store_set(server->store, route->key, body->value,
                                    body->ttl_ms, &result->changed);
    } else if (strcmp(request->method, "GET") == 0) {
        result->status = store_get(server->store, route->key, &result->value);
    } else {
        result->status = store_delete(server->store, route->key, &result->changed);
    }
}

static void execute_store(server_context_t *server, const route_t *route,
                           const http_request_t *request, const validated_body_t *body,
                           store_result_t *result) {
    pthread_mutex_lock(&server->mutex);
    switch (route->kind) {
    case ROUTE_VALUE: execute_value(server, route, request, body, result); break;
    case ROUTE_TTL:
        if (strcmp(request->method, "GET") == 0) {
            result->status = store_ttl(server->store, route->key, &result->ttl_ms);
        } else {
            result->status = store_expire(server->store, route->key, body->ttl_ms,
                                          &result->changed);
        }
        break;
    case ROUTE_INVALIDATE:
        result->status = store_delete_by_prefix(server->store, body->prefix,
                                                &result->deleted_count);
        break;
    case ROUTE_STATS: store_get_stats(server->store, &result->stats); break;
    }
    pthread_mutex_unlock(&server->mutex);
}

static void respond_value(http_response_t *response, const char *value) {
    char *quoted = json_quote(value);
    if (quoted == NULL) { return; }
    size_t len = strlen(quoted) + sizeof("{\"value\":}");
    response->body = malloc(len);
    if (response->body != NULL) { snprintf(response->body, len, "{\"value\":%s}", quoted); }
    free(quoted);
    response->status = 200;
}

static void respond_result(const route_t *route, const http_request_t *request,
                            const store_result_t *result, http_response_t *response) {
    if (result->status != STORE_OK) {
        api_error_t error = api_errors_from_store_status(result->status);
        http_response_error(response, error.http_status, error.code,
                              route_function(route, request));
        return;
    }
    char body[512];
    if (route->kind == ROUTE_VALUE && strcmp(request->method, "GET") == 0) {
        respond_value(response, result->value); return;
    }
    if (route->kind == ROUTE_STATS) {
        snprintf(body, sizeof(body), "{\"size\":%zu,\"capacity\":%zu,\"hitCount\":%" PRIu64
                 ",\"missCount\":%" PRIu64 ",\"expiredCount\":%" PRIu64 "}", result->stats.size,
                 result->stats.capacity, result->stats.hit_count, result->stats.miss_count,
                 result->stats.expired_count);
    } else if (route->kind == ROUTE_INVALIDATE) {
        snprintf(body, sizeof(body), "{\"deletedCount\":%zu}", result->deleted_count);
    } else if (route->kind == ROUTE_TTL && strcmp(request->method, "GET") == 0) {
        snprintf(body, sizeof(body), "{\"ttlMs\":%" PRId64 "}", result->ttl_ms);
    } else {
        const char *field = route->kind == ROUTE_TTL ? "applied"
                            : strcmp(request->method, "PUT") == 0 ? "created" : "deleted";
        snprintf(body, sizeof(body), "{\"%s\":%s}", field, result->changed ? "true" : "false");
    }
    http_response_json(response, 200, body);
}

static bool parse_body(const route_t *route, const http_request_t *request,
                        json_object_t *object, validated_body_t *body,
                        http_response_t *response) {
    int status = json_parse_object(request->body, request->body_len, object);
    if (status != 0) {
        http_response_error(response, status == -2 ? 500 : 400,
                              status == -2 ? "INTERNAL_ERROR" : "INVALID_JSON",
                              route_function(route, request));
        return false;
    }
    const char *error = validate_fields(object, route->kind, body);
    if (error != NULL) {
        http_response_error(response, strcmp(error, "VALUE_TOO_LARGE") == 0 ? 413 : 400,
                              error, route_function(route, request));
        return false;
    }
    return true;
}

void handlers_dispatch(server_context_t *server, const route_t *route,
                        const http_request_t *request, http_response_t *out_response) {
    json_object_t object = {0};
    validated_body_t body = {0};
    bool has_body = strcmp(request->method, "PUT") == 0 || route->kind == ROUTE_INVALIDATE;
    if (!has_body || parse_body(route, request, &object, &body, out_response)) {
        store_result_t result = {0};
        execute_store(server, route, request, &body, &result);
        respond_result(route, request, &result, out_response);
        free(result.value);
    }
    json_object_destroy(&object);
}
