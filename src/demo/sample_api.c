#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "http.h"
#include "json.h"
#include "mini_redis_client.h"
#include "slow_source.h"
#include "tcp_server.h"

/*
 * 캐시 샘플 API. 미니 레디스에는 HTTP API로만 접근한다. (협약 6장, 13장)
 * GET /products/{id}            캐시 사용 (Cache-Aside)
 * GET /products/{id}?cache=off  캐시 사용 안 함
 * PUT /products/{id}            원본 수정 + product:{id} 무효화
 * GET /demo/stats               {"originCallCount": n}
 *
 * 실행 : sample_api [포트] [미니 레디스 포트] [CACHE_TTL_MS]
 */

#define DEFAULT_PORT 3000
#define DEFAULT_REDIS_PORT 6380
#define DEFAULT_REDIS_HOST "127.0.0.1"
#define CACHE_TTL_MS 60000
#define REDIS_TIMEOUT_MS 1000       /* 미니 레디스가 이만큼 답이 없으면 원본으로 응답한다 */
#define WORKER_COUNT 16             /* 원본이 300ms씩 붙잡으므로 미니 레디스보다 많이 둔다 */
#define MAX_PRODUCT_ID_DIGITS 7     /* SLOW_SOURCE_MAX_PRODUCT_ID = 1000000 */
#define CACHE_KEY_BYTES 32
#define PRODUCTS_PREFIX "/products/"
#define CACHE_OFF_PARAM "cache=off"
#define X_CACHE_HIT "X-Cache: HIT\r\n"
#define X_CACHE_MISS "X-Cache: MISS\r\n"
#define DECIMAL_BASE 10
#define MIN_PORT 1
#define MAX_PORT 65535

typedef struct {
    mini_redis_client_t client;
    int64_t cache_ttl_ms;
} demo_context_t;

/* {"error": {"code", "message": "[function_name] 설명"}} — 미니 레디스(협약 11.3)와 같은 모양 */
static void write_error(int status_code, const char *code, const char *function_name,
                        http_response_t *out_response) {
    json_buffer_t buffer = {0};

    json_buffer_append_text(&buffer, "{\"error\":{\"code\":\"");
    json_buffer_append_text(&buffer, code);
    json_buffer_append_text(&buffer, "\",\"message\":\"[");
    json_buffer_append_text(&buffer, function_name);
    json_buffer_append_text(&buffer, "] 요청을 처리하지 못했습니다\"}}");
    out_response->status_code = buffer.has_error ? 500 : status_code;
    out_response->body = buffer.data;
    out_response->body_len = buffer.len;
}

/* 본문의 소유권을 응답으로 넘긴다 */
static void write_body(char *body, const char *extra_headers, http_response_t *out_response) {
    out_response->status_code = 200;
    out_response->extra_headers = extra_headers;
    out_response->body = body;
    out_response->body_len = strlen(body);
}

static void build_cache_key(int64_t id, char *out_key, size_t key_bytes) {
    snprintf(out_key, key_bytes, "product:%lld", (long long)id);
}

/* 원본에서 읽고, 캐시가 살아 있으면(can_cache) 결과를 TTL과 함께 저장한다. 실패는 캐시하지 않는다 */
static void respond_from_origin(const demo_context_t *context, int64_t id, bool can_cache,
                                http_response_t *out_response) {
    char *product_json = NULL;
    char cache_key[CACHE_KEY_BYTES];
    slow_source_status_t status = slow_source_get_product(id, &product_json);

    if (status != SLOW_SOURCE_OK) {
        write_error(status == SLOW_SOURCE_NOT_FOUND ? 404 : 500,
                    status == SLOW_SOURCE_NOT_FOUND ? "PRODUCT_NOT_FOUND" : "INTERNAL_ERROR",
                    "slow_source_get_product", out_response);
        return;
    }
    if (can_cache) {
        build_cache_key(id, cache_key, sizeof(cache_key));
        mini_redis_client_set(&context->client, cache_key, product_json, context->cache_ttl_ms);
    }
    write_body(product_json, NULL, out_response);
}

/*
 * Cache-Aside (협약 13.2) : 캐시에 있으면 HIT, 없으면 원본에서 가져와 채우고 MISS.
 * 미니 레디스가 꺼져 있거나 답이 없으면 원본으로 바로 응답한다 (캐시 장애가 서비스 장애가 되지 않게).
 */
static void handle_get_product(const demo_context_t *context, int64_t id, bool use_cache,
                               http_response_t *out_response) {
    char cache_key[CACHE_KEY_BYTES];
    char *cached_value = NULL;
    mini_redis_client_status_t cache_status = MINI_REDIS_CLIENT_ERROR;

    if (!use_cache) {
        respond_from_origin(context, id, false, out_response);
        return;
    }
    build_cache_key(id, cache_key, sizeof(cache_key));
    cache_status = mini_redis_client_get(&context->client, cache_key, &cached_value);
    if (cache_status == MINI_REDIS_CLIENT_OK) {
        write_body(cached_value, X_CACHE_HIT, out_response);
        return;
    }
    respond_from_origin(context, id, cache_status == MINI_REDIS_CLIENT_NOT_FOUND, out_response);
    if (out_response->status_code == 200) {
        out_response->extra_headers = X_CACHE_MISS;
    }
}

static bool has_only_fields(const json_object_t *body, const char *first, const char *second) {
    for (size_t i = 0; i < body->field_count; i++) {
        const char *name = body->fields[i].name;
        if (strcmp(name, first) != 0 && strcmp(name, second) != 0) {
            return false;
        }
    }
    return true;
}

/* PUT 본문 {"name": 문자열, "price": 0 이상의 정수} 검증. 실패하면 에러 code */
static const char *validate_product_body(const http_request_t *request, json_object_t *out_body,
                                         int64_t *out_price) {
    const char *text = request->body != NULL ? request->body : "";
    json_status_t status = json_parse_object(text, request->body_len, out_body);
    const json_field_t *name = NULL;

    if (status == JSON_ERR_NO_MEMORY) {
        return "INTERNAL_ERROR";
    }
    if (status != JSON_OK) {
        return status == JSON_ERR_SYNTAX ? "INVALID_JSON" : "INVALID_BODY";
    }
    name = json_object_find(out_body, "name");
    if (!has_only_fields(out_body, "name", "price") || name == NULL ||
        name->type != JSON_TYPE_STRING || memchr(name->text, '\0', name->text_len) != NULL ||
        !json_field_to_int64(json_object_find(out_body, "price"), out_price) || *out_price < 0) {
        json_object_destroy(out_body);
        return "INVALID_BODY";
    }
    return NULL;
}

/* 원본을 고친 뒤 캐시를 지운다. 둘 다 끝난 뒤에 성공을 돌려준다 (협약 13.3) */
static void handle_put_product(const demo_context_t *context, int64_t id,
                               const http_request_t *request, http_response_t *out_response) {
    json_object_t body;
    int64_t price = 0;
    char cache_key[CACHE_KEY_BYTES];
    char *product_json = NULL;
    const char *error_code = validate_product_body(request, &body, &price);
    slow_source_status_t status = SLOW_SOURCE_OK;

    if (error_code != NULL) {
        bool is_no_memory = strcmp(error_code, "INTERNAL_ERROR") == 0;
        write_error(is_no_memory ? 500 : 400, error_code, "handle_put_product", out_response);
        return;
    }
    status = slow_source_update_product(id, json_object_find(&body, "name")->text, price,
                                        &product_json);
    json_object_destroy(&body);
    if (status != SLOW_SOURCE_OK) {
        write_error(status == SLOW_SOURCE_NOT_FOUND ? 404 : 500,
                    status == SLOW_SOURCE_NOT_FOUND ? "PRODUCT_NOT_FOUND" : "INTERNAL_ERROR",
                    "slow_source_update_product", out_response);
        return;
    }
    build_cache_key(id, cache_key, sizeof(cache_key));
    if (mini_redis_client_delete(&context->client, cache_key) != MINI_REDIS_CLIENT_OK) {
        fprintf(stderr, "[handle_put_product] 캐시 %s를 지우지 못했습니다\n", cache_key);
    }
    write_body(product_json, NULL, out_response);
}

static void handle_demo_stats(http_response_t *out_response) {
    json_buffer_t buffer = {0};

    json_buffer_append_text(&buffer, "{\"originCallCount\":");
    json_buffer_append_uint(&buffer, slow_source_get_origin_call_count());
    json_buffer_append_text(&buffer, "}");
    if (buffer.has_error) {
        json_buffer_destroy(&buffer);
        write_error(500, "INTERNAL_ERROR", "handle_demo_stats", out_response);
        return;
    }
    write_body(buffer.data, NULL, out_response);
}

/* "/products/" 뒤의 숫자만 id로 받는다. 숫자가 아니거나 너무 길면 거짓 */
static bool parse_product_id(const char *text, size_t text_len, int64_t *out_id) {
    int64_t id = 0;

    if (text_len == 0 || text_len > MAX_PRODUCT_ID_DIGITS) {
        return false;
    }
    for (size_t i = 0; i < text_len; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
        id = id * DECIMAL_BASE + (text[i] - '0');
    }
    *out_id = id;
    return true;
}

/* 쿼리를 '&'로 나눠 cache=off가 정확히 있는지 본다 */
static bool is_cache_off(const char *query) {
    size_t param_len = strlen(CACHE_OFF_PARAM);

    while (query != NULL && *query != '\0') {
        size_t part_len = strcspn(query, "&");
        if (part_len == param_len && strncmp(query, CACHE_OFF_PARAM, param_len) == 0) {
            return true;
        }
        query = (query[part_len] == '&') ? query + part_len + 1 : NULL;
    }
    return false;
}

static void route_product(const demo_context_t *context, const http_request_t *request,
                          http_response_t *out_response) {
    const char *id_text = request->path + strlen(PRODUCTS_PREFIX);
    size_t id_len = strcspn(id_text, "?");
    const char *query = id_text[id_len] == '?' ? id_text + id_len + 1 : NULL;
    int64_t id = 0;

    if (!parse_product_id(id_text, id_len, &id)) {
        write_error(400, "INVALID_PRODUCT_ID", "route_product", out_response);
    } else if (strcmp(request->method, "GET") == 0) {
        handle_get_product(context, id, !is_cache_off(query), out_response);
    } else if (strcmp(request->method, "PUT") == 0) {
        handle_put_product(context, id, request, out_response);
    } else {
        write_error(405, "METHOD_NOT_ALLOWED", "route_product", out_response);
    }
}

static void route_request(const demo_context_t *context, const http_request_t *request,
                          http_response_t *out_response) {
    size_t path_len = strcspn(request->path, "?");

    if (strncmp(request->path, PRODUCTS_PREFIX, strlen(PRODUCTS_PREFIX)) == 0) {
        route_product(context, request, out_response);
    } else if (path_len == strlen("/demo/stats") &&
               strncmp(request->path, "/demo/stats", path_len) == 0) {
        if (strcmp(request->method, "GET") == 0) {
            handle_demo_stats(out_response);
        } else {
            write_error(405, "METHOD_NOT_ALLOWED", "route_request", out_response);
        }
    } else {
        write_error(404, "ROUTE_NOT_FOUND", "route_request", out_response);
    }
}

static void handle_connection(int client_fd, void *context) {
    http_request_t request;
    http_response_t response = {0, NULL, NULL, 0};
    http_read_status_t status = http_read_request(client_fd, &request);

    if (status == HTTP_READ_CLOSED) {
        close(client_fd);
        return;
    }
    if (status == HTTP_READ_OK) {
        route_request(context, &request, &response);
    } else if (status == HTTP_READ_TOO_LARGE) {
        write_error(413, "PAYLOAD_TOO_LARGE", "http_read_request", &response);
    } else {
        write_error(400, "INVALID_BODY", "http_read_request", &response);
    }
    http_write_response(client_fd, &response);
    http_response_destroy(&response);
    http_request_destroy(&request);
    http_close_connection(client_fd);
}

/* 정수 인자를 읽는다. 숫자가 아니거나 [min, max] 밖이면 거짓 */
static bool parse_int_arg(const char *text, long min, long max, long *out_value) {
    char *end = NULL;
    long value = 0;

    errno = 0;
    value = strtol(text, &end, DECIMAL_BASE);
    if (errno != 0 || end == text || *end != '\0' || value < min || value > max) {
        return false;
    }
    *out_value = value;
    return true;
}

/* 실행 인자 [포트] [미니 레디스 포트] [CACHE_TTL_MS]. 잘못되면 거짓 */
static bool read_args(int argc, char *argv[], long *out_port, demo_context_t *out_context) {
    long redis_port = DEFAULT_REDIS_PORT;
    long cache_ttl_ms = CACHE_TTL_MS;

    *out_port = DEFAULT_PORT;
    if ((argc > 1 && !parse_int_arg(argv[1], MIN_PORT, MAX_PORT, out_port)) ||
        (argc > 2 && !parse_int_arg(argv[2], MIN_PORT, MAX_PORT, &redis_port)) ||
        (argc > 3 && !parse_int_arg(argv[3], 1, 2147483647L, &cache_ttl_ms))) {
        return false;
    }
    out_context->client.host = DEFAULT_REDIS_HOST;
    out_context->client.port = (int)redis_port;
    out_context->client.timeout_ms = REDIS_TIMEOUT_MS;
    out_context->cache_ttl_ms = cache_ttl_ms;
    return true;
}

int main(int argc, char *argv[]) {
    demo_context_t context;
    long port = DEFAULT_PORT;
    tcp_server_options_t options;
    int result = 0;

    if (!read_args(argc, argv, &port, &context)) {
        fprintf(stderr, "사용법 : sample_api [포트] [미니 레디스 포트] [CACHE_TTL_MS]\n");
        return 1;
    }
    options = (tcp_server_options_t){(int)port, WORKER_COUNT, handle_connection, &context};
    printf("샘플 API 시작 (포트 %ld, 미니 레디스 %s:%d, 캐시 TTL %lldms)\n", port,
           context.client.host, context.client.port, (long long)context.cache_ttl_ms);
    fflush(stdout);
    result = tcp_server_run(&options);
    slow_source_destroy();
    printf("샘플 API 종료\n");
    return result == 0 ? 0 : 1;
}
