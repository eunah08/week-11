#include "router.h"
#include "handlers.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static bool is_allowed(route_kind_t kind, const char *method) {
    if (kind == ROUTE_STATS) { return strcmp(method, "GET") == 0; }
    if (kind == ROUTE_INVALIDATE) { return strcmp(method, "POST") == 0; }
    return strcmp(method, "GET") == 0 || strcmp(method, "PUT") == 0
           || (kind == ROUTE_VALUE && strcmp(method, "DELETE") == 0);
}

static int parse_route(const char *path, route_t *route, char **out_key) {
    if (strcmp(path, "/stats") == 0) { route->kind = ROUTE_STATS; return 0; }
    if (strcmp(path, "/invalidate") == 0) { route->kind = ROUTE_INVALIDATE; return 0; }
    if (strncmp(path, "/keys/", 6) != 0) { return 404; }
    const char *key = path + 6;
    size_t len = strlen(key);
    route->kind = ROUTE_VALUE;
    if (len >= 4 && strcmp(key + len - 4, "/ttl") == 0) {
        route->kind = ROUTE_TTL; len -= 4;
    }
    if (memchr(key, '/', len) != NULL || memchr(key, '?', len) != NULL) { return 404; }
    if (len == 0) { return 400; }
    *out_key = http_decode_key(key, len);
    if (*out_key == NULL && errno == ENOMEM) { return 500; }
    if (*out_key == NULL || strlen(*out_key) == 0 || strlen(*out_key) > MAX_KEY_BYTES) {
        return 400;
    }
    route->key = *out_key;
    return 0;
}

void router_dispatch(const http_request_t *request, http_response_t *out_response, void *context) {
    route_t route = {0};
    char *key = NULL;
    int status = parse_route(request->path, &route, &key);
    if (status != 0) {
        http_response_error(out_response, status,
                              status == 400 ? "INVALID_KEY"
                              : status == 500 ? "INTERNAL_ERROR" : "ROUTE_NOT_FOUND",
                              "router_dispatch");
    } else if (!is_allowed(route.kind, request->method)) {
        http_response_error(out_response, 405, "METHOD_NOT_ALLOWED", "router_dispatch");
    } else {
        handlers_dispatch(context, &route, request, out_response);
    }
    free(key);
}
