#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache_client.h"
#include "http.h"
#include "json.h"
#include "slow_source.h"

#define DEFAULT_PORT 3000
#define DEFAULT_REDIS_PORT 6380
#define CACHE_TTL_MS 60000
#define MAX_PRODUCT_ID_BYTES 248
#define MAX_PRODUCT_VALUE_BYTES 1048576
#define MAX_CONFIG_MS 2147483647

typedef struct {
    slow_source_t *source;
    cache_client_t cache;
} demo_context_t;

static void respond_product(http_response_t *response, const char *value) {
    char *quoted = json_quote(value);
    if (quoted == NULL) { return; }
    size_t len = strlen(quoted) + sizeof("{\"value\":}");
    response->body = malloc(len);
    if (response->body != NULL) { snprintf(response->body, len, "{\"value\":%s}", quoted); }
    response->status = 200;
    free(quoted);
}

static void get_product(demo_context_t *demo, const char *id, bool has_cache,
                         http_response_t *response) {
    char *cached = has_cache ? cache_client_get(&demo->cache, id) : NULL;
    if (cached != NULL) {
        respond_product(response, cached);
        free(cached);
        response->extra_headers = "X-Cache: HIT\r\n";
        return;
    }
    response->extra_headers = "X-Cache: MISS\r\n";
    char *value = NULL;
    source_status_t status = slow_source_get_product(demo->source, id, &value);
    if (status == SOURCE_FAILED) {
        http_response_error(response, 503, "ORIGIN_UNAVAILABLE", "get_product"); return;
    }
    if (status != SOURCE_OK) {
        http_response_error(response, 500, "INTERNAL_ERROR", "get_product"); return;
    }
    respond_product(response, value);
    if (has_cache && response->body != NULL) {
        cache_client_set(&demo->cache, id, value);
    }
    free(value);
}

static const char *validate_update(const json_object_t *object) {
    if (!object->is_object || object->has_duplicate || object->has_overflow
        || object->count != 1 || object->fields[0].name_len != 5
        || strcmp(object->fields[0].name, "value") != 0) { return "INVALID_BODY"; }
    const json_field_t *value = &object->fields[0];
    if (value->kind != JSON_STRING || strlen(value->string) != value->string_len) {
        return "INVALID_VALUE";
    }
    if (value->string_len > MAX_PRODUCT_VALUE_BYTES) { return "VALUE_TOO_LARGE"; }
    return NULL;
}

static void apply_update(demo_context_t *demo, const char *id, const char *value,
                          http_response_t *response) {
    if (slow_source_update_product(demo->source, id, value) != SOURCE_OK) {
        http_response_error(response, 500, "INTERNAL_ERROR", "slow_source_update_product");
    } else if (cache_client_delete(&demo->cache, id) != 0) {
        http_response_error(response, 503, "CACHE_UNAVAILABLE", "cache_client_delete");
    } else {
        http_response_json(response, 200, "{\"updated\":true}");
    }
}

static void update_product(demo_context_t *demo, const char *id,
                            const http_request_t *request, http_response_t *response) {
    json_object_t object;
    int status = json_parse_object(request->body, request->body_len, &object);
    if (status != 0) {
        http_response_error(response, status == -2 ? 500 : 400,
                              status == -2 ? "INTERNAL_ERROR" : "INVALID_JSON", "update_product");
        return;
    }
    const char *error = validate_update(&object);
    if (error != NULL) {
        http_response_error(response, strcmp(error, "VALUE_TOO_LARGE") == 0 ? 413 : 400,
                              error, "update_product");
    } else {
        apply_update(demo, id, object.fields[0].string, response);
    }
    json_object_destroy(&object);
}

static void dispatch_product(demo_context_t *demo, const http_request_t *request,
                              http_response_t *response) {
    const char *start = request->path + strlen("/products/");
    const char *query = strchr(start, '?');
    size_t len = query ? (size_t)(query - start) : strlen(start);
    char *id = http_decode_key(start, len);
    if (id == NULL && errno == ENOMEM) {
        http_response_error(response, 500, "INTERNAL_ERROR", "dispatch_product");
    } else if (id == NULL || strlen(id) == 0 || strlen(id) > MAX_PRODUCT_ID_BYTES) {
        http_response_error(response, 400, "INVALID_KEY", "dispatch_product");
    } else if (query != NULL && strcmp(query, "?cache=off") != 0) {
        http_response_error(response, 400, "INVALID_BODY", "dispatch_product");
    } else if (strcmp(request->method, "GET") == 0) {
        get_product(demo, id, query == NULL, response);
    } else if (strcmp(request->method, "PUT") == 0) {
        update_product(demo, id, request, response);
    } else {
        http_response_error(response, 405, "METHOD_NOT_ALLOWED", "dispatch_product");
    }
    free(id);
}

static void demo_dispatch(const http_request_t *request, http_response_t *response, void *context) {
    demo_context_t *demo = context;
    if (strcmp(request->path, "/demo/stats") == 0) {
        if (strcmp(request->method, "GET") != 0) {
            http_response_error(response, 405, "METHOD_NOT_ALLOWED", "demo_dispatch"); return;
        }
        char body[96];
        snprintf(body, sizeof(body), "{\"originCallCount\":%" PRIu64 "}",
                 slow_source_get_origin_call_count(demo->source));
        http_response_json(response, 200, body);
    } else if (strncmp(request->path, "/products/", strlen("/products/")) == 0) {
        dispatch_product(demo, request, response);
    } else {
        http_response_error(response, 404, "ROUTE_NOT_FOUND", "demo_dispatch");
    }
}

static bool parse_options(int argc, char *argv[], long *options) {
    if (argc > 5) { return false; }
    for (int i = 1; i < argc; i++) {
        char *end;
        options[i - 1] = strtol(argv[i], &end, 10);
        long maximum = i <= 2 ? 65535 : MAX_CONFIG_MS;
        long minimum = i == 4 ? 0 : 1;
        if (*end != '\0' || options[i - 1] < minimum || options[i - 1] > maximum) {
            return false;
        }
    }
    return true;
}

int main(int argc, char *argv[]) {
    long options[] = {DEFAULT_PORT, DEFAULT_REDIS_PORT, CACHE_TTL_MS, SLOW_SOURCE_DELAY_MS};
    if (!parse_options(argc, argv, options)) {
        fprintf(stderr, "사용법: %s [port [redis_port [ttl_ms [origin_delay_ms]]]]\n", argv[0]);
        return 1;
    }
    demo_context_t demo = {slow_source_create((unsigned)options[3]),
                            {(int)options[1], options[2]}};
    if (demo.source == NULL) { fprintf(stderr, "[main] 메모리가 부족합니다\n"); return 1; }
    printf("샘플 API 127.0.0.1:%ld (캐시 %ld, TTL %ldms, 원본 %ldms)\n",
           options[0], options[1], options[2], options[3]); fflush(stdout);
    int status = http_serve((int)options[0], demo_dispatch, &demo);
    slow_source_destroy(demo.source);
    return status != 0;
}
