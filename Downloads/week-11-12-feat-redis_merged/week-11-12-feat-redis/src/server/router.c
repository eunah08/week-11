#include "router.h"

#include <stdio.h>
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
 * 선택 엔드포인트 (협약 11.2)
 * GET    /keys/{key}/exists
 * GET    /health
 */

#define KEYS_PREFIX "/keys/"

typedef enum {
    ROUTE_NONE,
    ROUTE_KEY,
    ROUTE_KEY_TTL,
    ROUTE_KEY_EXISTS,
    ROUTE_INVALIDATE,
    ROUTE_STATS,
    ROUTE_HEALTH
} route_t;

/* 경로에서 찾은 경로 종류와, 키가 있다면 인코딩된 키의 위치 */
typedef struct {
    route_t route;
    const char *raw_key;
    size_t raw_key_len;
} route_match_t;

static bool is_same_text(const char *text, size_t text_len, const char *expected) {
    return text_len == strlen(expected) && memcmp(text, expected, text_len) == 0;
}

/* "/keys/" 뒤 : "{key}", "{key}/ttl", "{key}/exists". 키 안의 '/'는 %2F로 인코딩되어 온다 */
static route_match_t match_key_route(const char *rest, size_t rest_len) {
    route_match_t match = {ROUTE_NONE, rest, rest_len};
    const char *slash = memchr(rest, '/', rest_len);

    if (slash == NULL) {
        match.route = ROUTE_KEY;
        return match;
    }
    match.raw_key_len = (size_t)(slash - rest);
    size_t suffix_len = rest_len - match.raw_key_len - 1;
    if (is_same_text(slash + 1, suffix_len, "ttl")) {
        match.route = ROUTE_KEY_TTL;
    } else if (is_same_text(slash + 1, suffix_len, "exists")) {
        match.route = ROUTE_KEY_EXISTS;
    }
    return match;
}

/* 쿼리(?…)는 이 서버에서 쓰지 않으므로 떼고 본다 */
static route_match_t match_route(const char *path) {
    size_t path_len = strcspn(path, "?");
    route_match_t match = {ROUTE_NONE, NULL, 0};
    size_t keys_prefix_len = strlen(KEYS_PREFIX);

    if (path_len >= keys_prefix_len && memcmp(path, KEYS_PREFIX, keys_prefix_len) == 0) {
        return match_key_route(path + keys_prefix_len, path_len - keys_prefix_len);
    }
    if (is_same_text(path, path_len, "/invalidate")) {
        match.route = ROUTE_INVALIDATE;
    } else if (is_same_text(path, path_len, "/stats")) {
        match.route = ROUTE_STATS;
    } else if (is_same_text(path, path_len, "/health")) {
        match.route = ROUTE_HEALTH;
    }
    return match;
}

static bool is_method_allowed(route_t route, const char *method) {
    bool is_get = strcmp(method, "GET") == 0;
    bool is_put = strcmp(method, "PUT") == 0;

    switch (route) {
    case ROUTE_KEY: return is_get || is_put || strcmp(method, "DELETE") == 0;
    case ROUTE_KEY_TTL: return is_get || is_put;
    case ROUTE_INVALIDATE: return strcmp(method, "POST") == 0;
    case ROUTE_KEY_EXISTS:
    case ROUTE_STATS:
    case ROUTE_HEALTH: return is_get;
    case ROUTE_NONE: break;
    }
    return false;
}

/*
 * 키를 풀어 *out_key에 넣는다. 성공하면 참이고 *out_key는 free().
 * 실패하면 *out_error : 빈 키·잘못된 인코딩·NUL은 INVALID_KEY, 메모리 부족은 INTERNAL
 */
static bool decode_key(const route_match_t *match, char **out_key, api_error_id_t *out_error) {
    char *key = NULL;
    size_t key_len = 0;
    http_decode_status_t status = HTTP_DECODE_INVALID;

    *out_error = API_ERROR_INVALID_KEY;
    if (match->raw_key_len > 0) {
        status = http_decode_percent(match->raw_key, match->raw_key_len, &key, &key_len);
    }
    if (status == HTTP_DECODE_NO_MEMORY) {
        fprintf(stderr, "[router_handle_request] 키를 풀 메모리가 부족합니다\n");
        *out_error = API_ERROR_INTERNAL;
    }
    if (status != HTTP_DECODE_OK) {
        return false;
    }
    if (key_len == 0 || memchr(key, '\0', key_len) != NULL) {
        free(key);
        return false;
    }
    *out_key = key;
    return true;
}

static void dispatch_key_route(const server_context_t *context, const http_request_t *request,
                               route_t route, const char *key, http_response_t *out_response) {
    const char *method = request->method;

    if (route == ROUTE_KEY_TTL) {
        if (strcmp(method, "PUT") == 0) {
            handlers_put_ttl(context, key, request, out_response);
        } else {
            handlers_get_ttl(context, key, out_response);
        }
    } else if (route == ROUTE_KEY_EXISTS) {
        handlers_get_exists(context, key, out_response);
    } else if (strcmp(method, "PUT") == 0) {
        handlers_put_key(context, key, request, out_response);
    } else if (strcmp(method, "GET") == 0) {
        handlers_get_key(context, key, out_response);
    } else {
        handlers_delete_key(context, key, out_response);
    }
}

void router_handle_request(const server_context_t *context, const http_request_t *request,
                           http_response_t *out_response) {
    route_match_t match = match_route(request->path);
    char *key = NULL;
    api_error_id_t key_error = API_ERROR_INVALID_KEY;

    if (match.route == ROUTE_NONE) {
        api_errors_write(API_ERROR_ROUTE_NOT_FOUND, "router_handle_request", out_response);
    } else if (!is_method_allowed(match.route, request->method)) {
        api_errors_write(API_ERROR_METHOD_NOT_ALLOWED, "router_handle_request", out_response);
    } else if (match.route == ROUTE_INVALIDATE) {
        handlers_post_invalidate(context, request, out_response);
    } else if (match.route == ROUTE_STATS) {
        handlers_get_stats(context, out_response);
    } else if (match.route == ROUTE_HEALTH) {
        handlers_get_health(out_response);
    } else if (!decode_key(&match, &key, &key_error)) {
        api_errors_write(key_error, "router_handle_request", out_response);
    } else {
        dispatch_key_route(context, request, match.route, key, out_response);
        free(key);
    }
}
