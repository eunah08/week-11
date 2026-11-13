#include "router.h"

#include <stdlib.h>
#include <string.h>

#include "api_errors.h"

/*
 * 필수 엔드포인트 (협약 11.1)
 * PUT    /keys/{key}
 * GET    /keys/{key}
 * DELETE /keys/{key}
 * PUT    /keys/{key}/ttl
 * GET    /keys/{key}/ttl
 * POST   /invalidate
 * GET    /stats
 *
 * 키 안의 '/'는 %2F로 인코딩되어 오므로, 인코딩되지 않은 '/'는 경로 구분자로만 본다.
 */

#define KEYS_PATH_PREFIX "/keys/"
#define TTL_PATH_SUFFIX "/ttl"
#define INVALIDATE_PATH "/invalidate"
#define STATS_PATH "/stats"

static bool is_method(const http_request_t *request, const char *method) {
    return strcmp(request->method, method) == 0;
}

/* 쿼리(?) 앞까지의 경로 길이 */
static size_t get_path_len(const char *path) {
    const char *query = strchr(path, '?');
    return query != NULL ? (size_t)(query - path) : strlen(path);
}

static bool is_path(const char *path, size_t path_len, const char *expected) {
    return strlen(expected) == path_len && strncmp(path, expected, path_len) == 0;
}

static bool has_ttl_suffix(const char *key_text, size_t key_text_len) {
    size_t suffix_len = strlen(TTL_PATH_SUFFIX);
    if (key_text_len < suffix_len) {
        return false;
    }
    return strncmp(key_text + key_text_len - suffix_len, TTL_PATH_SUFFIX, suffix_len) == 0;
}

static bool is_allowed_key_method(const http_request_t *request, bool is_ttl_path) {
    if (is_method(request, "PUT") || is_method(request, "GET")) {
        return true;
    }
    return !is_ttl_path && is_method(request, "DELETE");
}

static void dispatch_key_route(const handlers_context_t *context, const http_request_t *request,
                               const char *key, http_response_t *out_response) {
    if (is_method(request, "PUT")) {
        handlers_put_key(context, key, request, out_response);
    } else if (is_method(request, "GET")) {
        handlers_get_key(context, key, out_response);
    } else {
        handlers_delete_key(context, key, out_response);
    }
}

static void dispatch_ttl_route(const handlers_context_t *context, const http_request_t *request,
                               const char *key, http_response_t *out_response) {
    if (is_method(request, "PUT")) {
        handlers_put_ttl(context, key, request, out_response);
    } else {
        handlers_get_ttl(context, key, out_response);
    }
}

/* /keys/ 로 시작하는 요청. 키 부분을 디코딩해서 키 또는 TTL 핸들러로 보낸다 */
static void route_keys(const handlers_context_t *context, const http_request_t *request,
                       http_response_t *out_response) {
    const char *key_text = request->path + strlen(KEYS_PATH_PREFIX);
    size_t key_text_len = get_path_len(request->path) - strlen(KEYS_PATH_PREFIX);
    bool is_ttl_path = has_ttl_suffix(key_text, key_text_len);
    if (is_ttl_path) {
        key_text_len -= strlen(TTL_PATH_SUFFIX);
    }
    if (memchr(key_text, '/', key_text_len) != NULL) {
        api_errors_set_response(out_response, API_ERROR_ROUTE_NOT_FOUND, "router_route");
        return;
    }
    if (!is_allowed_key_method(request, is_ttl_path)) {
        api_errors_set_response(out_response, API_ERROR_METHOD_NOT_ALLOWED, "router_route");
        return;
    }

    char *key = NULL;
    http_decode_status_t decode_status = http_decode_url(key_text, key_text_len, &key);
    if (decode_status == HTTP_DECODE_NO_MEMORY) {
        api_errors_set_response(out_response, API_ERROR_INTERNAL_ERROR, "http_decode_url");
    } else if (decode_status == HTTP_DECODE_INVALID) {
        api_errors_set_response(out_response, API_ERROR_INVALID_KEY, "http_decode_url");
    } else if (is_ttl_path) {
        dispatch_ttl_route(context, request, key, out_response);
    } else {
        dispatch_key_route(context, request, key, out_response);
    }
    free(key);
}

void router_route(const handlers_context_t *context, const http_request_t *request,
                  http_response_t *out_response) {
    const char *path = request->path;
    size_t path_len = get_path_len(path);
    size_t keys_prefix_len = strlen(KEYS_PATH_PREFIX);

    if (path_len >= keys_prefix_len && strncmp(path, KEYS_PATH_PREFIX, keys_prefix_len) == 0) {
        route_keys(context, request, out_response);
    } else if (is_path(path, path_len, INVALIDATE_PATH) && is_method(request, "POST")) {
        handlers_post_invalidate(context, request, out_response);
    } else if (is_path(path, path_len, STATS_PATH) && is_method(request, "GET")) {
        handlers_get_stats(context, out_response);
    } else if (is_path(path, path_len, INVALIDATE_PATH) || is_path(path, path_len, STATS_PATH)) {
        api_errors_set_response(out_response, API_ERROR_METHOD_NOT_ALLOWED, "router_route");
    } else {
        api_errors_set_response(out_response, API_ERROR_ROUTE_NOT_FOUND, "router_route");
    }
}
