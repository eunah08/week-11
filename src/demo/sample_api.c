#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "http.h"
#include "json.h"
#include "mini_redis_client.h"
#include "slow_source.h"

/*
 * 캐시 샘플 API. 미니 레디스에는 HTTP API로만 접근한다. (협약 6장, 13장)
 * GET /products/{id}            캐시 사용 (Cache-Aside)
 * GET /products/{id}?cache=off  캐시 사용 안 함
 * PUT /products/{id}            원본 수정 + product:{id} 무효화. 본문 {"name": "..."}
 * GET /demo/stats               {"originCallCount": n}
 *
 * 실행 : sample_api [포트] [미니 레디스 포트]
 */

#define DEFAULT_PORT 3000
#define DEFAULT_MINI_REDIS_PORT 6380
#define MINI_REDIS_HOST "127.0.0.1"
#define CACHE_TTL_MS 60000
#define LISTEN_BACKLOG 128
#define CLIENT_TIMEOUT_MS 5000
#define MAX_PRODUCT_ID_DIGITS 18
#define CACHE_KEY_BYTES 64
#define SMALL_BODY_BYTES 512
#define PRODUCTS_PATH_PREFIX "/products/"
#define DEMO_STATS_PATH "/demo/stats"
#define CACHE_OFF_QUERY "cache=off"
#define CACHE_KEY_PREFIX "product:"
#define X_CACHE_HIT "X-Cache: HIT\r\n"
#define X_CACHE_MISS "X-Cache: MISS\r\n"
#define HTTP_STATUS_OK 200
#define HTTP_STATUS_BAD_REQUEST 400
#define HTTP_STATUS_NOT_FOUND 404
#define HTTP_STATUS_METHOD_NOT_ALLOWED 405
#define HTTP_STATUS_INTERNAL_ERROR 500

typedef struct {
    const mini_redis_client_t *mini_redis;
    int client_fd;
} connection_t;

/* ---------- 응답 만들기 ---------- */

/* 협약 11.3과 같은 모양의 에러 응답 */
static void set_error_response(http_response_t *out_response, int http_status, const char *code,
                               const char *message) {
    char body[SMALL_BODY_BYTES];
    snprintf(body, sizeof(body), "{\"error\": {\"code\": \"%s\", \"message\": \"%s\"}}", code,
             message);
    out_response->http_status = http_status;
    out_response->body = strdup(body);
}

static void set_internal_error(http_response_t *out_response, const char *function_name) {
    char message[SMALL_BODY_BYTES];
    snprintf(message, sizeof(message), "[%s] 서버 내부 오류입니다", function_name);
    set_error_response(out_response, HTTP_STATUS_INTERNAL_ERROR, "INTERNAL_ERROR", message);
}

/* 해제 책임 : body의 소유권이 out_response로 넘어간다 */
static void set_ok_response(http_response_t *out_response, char *body, const char *x_cache) {
    if (body == NULL) {
        set_internal_error(out_response, "set_ok_response");
        return;
    }
    out_response->http_status = HTTP_STATUS_OK;
    out_response->body = body;
    out_response->extra_headers = x_cache;
}

/* ---------- 상품 ---------- */

/* Cache-Aside : 캐시를 보고, 없으면 원본에서 가져와 캐시에 채운다 (협약 13.2) */
static void respond_product_with_cache(const mini_redis_client_t *mini_redis,
                                       const char *product_id, http_response_t *out_response) {
    char cache_key[CACHE_KEY_BYTES];
    char *cached_product = NULL;
    snprintf(cache_key, sizeof(cache_key), "%s%s", CACHE_KEY_PREFIX, product_id);

    mini_redis_client_status_t cache_status =
        mini_redis_client_get(mini_redis, cache_key, &cached_product);
    if (cache_status == MINI_REDIS_CLIENT_OK) {
        set_ok_response(out_response, cached_product, X_CACHE_HIT);
        return;
    }

    char *product = slow_source_get_product(product_id);
    if (product == NULL) {
        set_internal_error(out_response, "slow_source_get_product");   /* 실패는 캐시하지 않는다 */
        return;
    }
    /* 미니 레디스가 꺼져 있으면(ERROR) 다시 기다리지 않고 원본으로만 응답한다 */
    if (cache_status == MINI_REDIS_CLIENT_NOT_FOUND) {
        mini_redis_client_set(mini_redis, cache_key, product, CACHE_TTL_MS);
    }
    set_ok_response(out_response, product, X_CACHE_MISS);
}

static void respond_product_without_cache(const char *product_id, http_response_t *out_response) {
    char *product = slow_source_get_product(product_id);
    if (product == NULL) {
        set_internal_error(out_response, "slow_source_get_product");
        return;
    }
    set_ok_response(out_response, product, NULL);
}

/* {"name": "..."} 본문에서 이름을 꺼낸다. 해제 책임 : 호출한 쪽이 free() */
static char *read_name_field(const http_request_t *request, http_response_t *out_response) {
    json_object_t body;
    json_parse_status_t parse_status = json_parse_object(request->body, request->body_len, &body);
    if (parse_status != JSON_PARSE_OK) {
        bool is_invalid_json = parse_status == JSON_PARSE_INVALID_JSON;
        const char *code = is_invalid_json ? "INVALID_JSON" : "INVALID_BODY";
        set_error_response(out_response, HTTP_STATUS_BAD_REQUEST, code,
                           "[read_name_field] 본문은 {\\\"name\\\": 문자열} 이어야 합니다");
        return NULL;
    }

    const json_field_t *name_field = json_find_field(&body, "name");
    char *name = NULL;
    bool is_valid = body.field_count == 1 && name_field != NULL
                    && name_field->type == JSON_TYPE_STRING
                    && strlen(name_field->string_value) == name_field->string_len;
    if (is_valid) {
        name = strdup(name_field->string_value);
    }
    json_object_destroy(&body);
    if (!is_valid) {
        set_error_response(out_response, HTTP_STATUS_BAD_REQUEST, "INVALID_BODY",
                           "[read_name_field] 본문은 {\\\"name\\\": 문자열} 이어야 합니다");
    } else if (name == NULL) {
        set_internal_error(out_response, "read_name_field");
    }
    return name;
}

/* 원본을 고친 뒤 캐시를 지운다. 둘 다 끝난 뒤 성공을 돌려준다 (협약 13.3) */
static void update_product(const mini_redis_client_t *mini_redis, const char *product_id,
                           const http_request_t *request, http_response_t *out_response) {
    char cache_key[CACHE_KEY_BYTES];
    char *name = read_name_field(request, out_response);
    if (name == NULL) {
        return;
    }
    bool is_updated = slow_source_update_product(product_id, name);
    free(name);
    if (!is_updated) {
        set_internal_error(out_response, "slow_source_update_product");
        return;
    }

    snprintf(cache_key, sizeof(cache_key), "%s%s", CACHE_KEY_PREFIX, product_id);
    mini_redis_client_delete(mini_redis, cache_key);   /* 꺼져 있어도 서비스는 계속한다 */
    set_ok_response(out_response, strdup("{\"updated\": true}"), NULL);
}

/* ---------- 라우팅 ---------- */

/* "/products/" 뒤가 1~MAX_PRODUCT_ID_DIGITS자리 숫자인지 확인하고 id를 복사한다 */
static bool read_product_id(const char *path, char *out_product_id) {
    size_t prefix_len = strlen(PRODUCTS_PATH_PREFIX);
    if (strncmp(path, PRODUCTS_PATH_PREFIX, prefix_len) != 0) {
        return false;
    }
    const char *id_start = path + prefix_len;
    size_t id_len = 0;
    while (id_start[id_len] >= '0' && id_start[id_len] <= '9') {
        id_len++;
    }
    bool is_end = id_start[id_len] == '\0' || id_start[id_len] == '?';
    if (id_len == 0 || id_len > MAX_PRODUCT_ID_DIGITS || !is_end) {
        return false;
    }
    memcpy(out_product_id, id_start, id_len);
    out_product_id[id_len] = '\0';
    return true;
}

static bool is_cache_off(const char *path) {
    const char *query = strchr(path, '?');
    return query != NULL && strcmp(query + 1, CACHE_OFF_QUERY) == 0;
}

static void route_product(const mini_redis_client_t *mini_redis, const http_request_t *request,
                          const char *product_id, http_response_t *out_response) {
    if (strcmp(request->method, "PUT") == 0) {
        update_product(mini_redis, product_id, request, out_response);
    } else if (strcmp(request->method, "GET") != 0) {
        set_error_response(out_response, HTTP_STATUS_METHOD_NOT_ALLOWED, "METHOD_NOT_ALLOWED",
                           "[route_product] 허용하지 않는 메서드입니다");
    } else if (is_cache_off(request->path)) {
        respond_product_without_cache(product_id, out_response);
    } else {
        respond_product_with_cache(mini_redis, product_id, out_response);
    }
}

static void respond_demo_stats(http_response_t *out_response) {
    char body[SMALL_BODY_BYTES];
    snprintf(body, sizeof(body), "{\"originCallCount\": %llu}",
             (unsigned long long)slow_source_get_origin_call_count());
    set_ok_response(out_response, strdup(body), NULL);
}

static void route_demo_request(const mini_redis_client_t *mini_redis,
                               const http_request_t *request, http_response_t *out_response) {
    char product_id[MAX_PRODUCT_ID_DIGITS + 1];

    if (read_product_id(request->path, product_id)) {
        route_product(mini_redis, request, product_id, out_response);
    } else if (strcmp(request->path, DEMO_STATS_PATH) == 0 && strcmp(request->method, "GET") == 0) {
        respond_demo_stats(out_response);
    } else {
        set_error_response(out_response, HTTP_STATUS_NOT_FOUND, "ROUTE_NOT_FOUND",
                           "[route_demo_request] 정의되지 않은 경로입니다");
    }
}

/* ---------- 연결 처리 ---------- */

static void *handle_connection(void *argument) {
    connection_t *connection = argument;
    http_request_t request;
    http_response_t response = {0};

    http_read_status_t read_status = http_read_request(connection->client_fd, &request);
    if (read_status == HTTP_READ_OK) {
        route_demo_request(connection->mini_redis, &request, &response);
    } else if (read_status != HTTP_READ_CLOSED) {
        set_error_response(&response, HTTP_STATUS_BAD_REQUEST, "INVALID_BODY",
                           "[http_read_request] 요청 형식이 올바르지 않습니다");
    }
    if (read_status != HTTP_READ_CLOSED) {
        http_write_response(connection->client_fd, &response);
    }

    http_response_destroy(&response);
    http_request_destroy(&request);
    close(connection->client_fd);
    free(connection);
    return NULL;
}

static bool start_connection_thread(const mini_redis_client_t *mini_redis, int client_fd) {
    connection_t *connection = malloc(sizeof(*connection));
    if (connection == NULL) {
        return false;
    }
    connection->mini_redis = mini_redis;
    connection->client_fd = client_fd;

    pthread_t thread;
    if (pthread_create(&thread, NULL, handle_connection, connection) != 0) {
        free(connection);
        return false;
    }
    pthread_detach(thread);
    return true;
}

static int open_listen_socket(int port) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        return -1;
    }
    int reuse_address = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_address, sizeof(reuse_address));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((uint16_t)port);
    if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0
        || listen(listen_fd, LISTEN_BACKLOG) < 0) {
        close(listen_fd);
        return -1;
    }
    return listen_fd;
}

int main(int argc, char *argv[]) {
    int port = argc > 1 ? atoi(argv[1]) : DEFAULT_PORT;
    mini_redis_client_t mini_redis = {MINI_REDIS_HOST, DEFAULT_MINI_REDIS_PORT};
    if (argc > 2) {
        mini_redis.port = atoi(argv[2]);
    }

    signal(SIGPIPE, SIG_IGN);
    int listen_fd = open_listen_socket(port);
    if (listen_fd < 0) {
        fprintf(stderr, "[main] 포트 %d를 열 수 없습니다\n", port);
        return 1;
    }
    printf("샘플 API 시작 (포트 %d, 미니 레디스 포트 %d)\n", port, mini_redis.port);
    fflush(stdout);

    while (true) {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd < 0) {
            continue;
        }
        http_set_timeout(client_fd, CLIENT_TIMEOUT_MS);
        if (!start_connection_thread(&mini_redis, client_fd)) {
            close(client_fd);
        }
    }
}
