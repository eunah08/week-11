#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "api_errors.h"
#include "handlers.h"
#include "http.h"
#include "router.h"
#include "store.h"
#include "sweeper.h"
#include "tcp_server.h"

/*
 * 미니 레디스 서버 시작과 종료. (협약 11.5, 12장)
 * 실행 : mini_redis [포트]
 * 동시성 구조 : 워커 스레드 WORKER_COUNT개 + store 전체를 잠그는 mutex 하나.
 */

#define DEFAULT_PORT 6380
#define WORKER_COUNT 8
#define MIN_PORT 1
#define MAX_PORT 65535
#define DECIMAL_BASE 10

/* HTTP 읽기 실패를 에러 응답으로 바꾼다. 연결이 끊긴 경우(CLOSED)는 부르지 않는다 */
static api_error_id_t to_read_error(http_read_status_t status) {
    switch (status) {
    case HTTP_READ_TOO_LARGE: return API_ERROR_PAYLOAD_TOO_LARGE;
    case HTTP_READ_NO_MEMORY: return API_ERROR_INTERNAL;
    default: return API_ERROR_INVALID_BODY;    /* 요청 형식 오류, chunked */
    }
}

/* 워커 스레드에서 연결 하나를 처리한다 : 읽기 → 라우팅 → 쓰기 → 닫기 */
static void handle_connection(int client_fd, void *context) {
    http_request_t request;
    http_response_t response = {0, NULL, NULL, 0};
    http_read_status_t status = http_read_request(client_fd, &request);

    if (status == HTTP_READ_CLOSED) {
        close(client_fd);
        return;
    }
    if (status == HTTP_READ_OK) {
        router_handle_request(context, &request, &response);
    } else {
        api_errors_write(to_read_error(status), "http_read_request", &response);
    }
    http_write_response(client_fd, &response);
    http_response_destroy(&response);
    http_request_destroy(&request);
    http_close_connection(client_fd);
}

/* 포트 인자를 읽는다. 숫자가 아니거나 범위 밖이면 -1 */
static int parse_port(const char *text) {
    char *end = NULL;
    long port = 0;

    errno = 0;
    port = strtol(text, &end, DECIMAL_BASE);
    if (errno != 0 || end == text || *end != '\0' || port < MIN_PORT || port > MAX_PORT) {
        return -1;
    }
    return (int)port;
}

static int run_server(int port, store_t *store) {
    pthread_mutex_t store_mutex = PTHREAD_MUTEX_INITIALIZER;
    server_context_t context = {store, &store_mutex};
    tcp_server_options_t options = {port, WORKER_COUNT, handle_connection, &context};
    int result = 0;

    if (sweeper_start(store, &store_mutex) != 0) {
        fprintf(stderr, "[main] 만료 정리 스레드를 시작하지 못했습니다\n");
        return 1;
    }
    printf("미니 레디스 서버 시작 (포트 %d, 워커 %d개)\n", port, WORKER_COUNT);
    fflush(stdout);
    result = tcp_server_run(&options);
    sweeper_stop();
    pthread_mutex_destroy(&store_mutex);
    printf("미니 레디스 서버 종료\n");
    return result == 0 ? 0 : 1;
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    store_t *store = NULL;
    int exit_code = 0;

    if (argc > 1) {
        port = parse_port(argv[1]);
        if (port < 0) {
            fprintf(stderr, "[main] 포트는 %d~%d 사이의 정수여야 합니다\n", MIN_PORT, MAX_PORT);
            return 1;
        }
    }
    store = store_create(NULL);
    if (store == NULL) {
        fprintf(stderr, "[main] 저장소를 만들 메모리가 부족합니다\n");
        return 1;
    }
    exit_code = run_server(port, store);
    store_destroy(store);
    return exit_code;
}
