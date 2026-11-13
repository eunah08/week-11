#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "api_errors.h"
#include "handlers.h"
#include "http.h"
#include "router.h"
#include "store.h"
#include "sweeper.h"

/*
 * 서버 시작과 종료. (협약 11.5, 12장)
 * 동시성 구조 : 연결마다 스레드 하나 + store 전체에 잠금 하나 (전체 잠금)
 */

#define DEFAULT_PORT 6380
#define LISTEN_BACKLOG 128
#define ACCEPT_POLL_MS 200        /* 종료 신호를 확인하는 주기 */
#define CLIENT_TIMEOUT_MS 5000    /* 멈춘 클라이언트를 기다리는 최대 시간 */

typedef struct {
    handlers_context_t handlers_context;
    pthread_mutex_t store_lock;
    pthread_mutex_t connection_lock;
    pthread_cond_t connection_done;
    size_t active_connection_count;   /* 종료할 때 모든 연결이 끝나길 기다리려고 센다 */
} server_t;

typedef struct {
    server_t *server;
    int client_fd;
} connection_t;

static volatile sig_atomic_t is_shutdown_requested = 0;

static void handle_shutdown_signal(int signal_number) {
    (void)signal_number;
    is_shutdown_requested = 1;
}

/* 끊긴 연결에 쓸 때 죽지 않게 SIGPIPE를 무시하고, SIGINT·SIGTERM은 종료 요청으로 받는다 */
static void install_signal_handlers(void) {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_shutdown_signal;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    signal(SIGPIPE, SIG_IGN);
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

static api_error_kind_t to_read_error_kind(http_read_status_t read_status) {
    if (read_status == HTTP_READ_TOO_LARGE) {
        return API_ERROR_PAYLOAD_TOO_LARGE;
    }
    if (read_status == HTTP_READ_NO_MEMORY) {
        return API_ERROR_INTERNAL_ERROR;
    }
    return API_ERROR_INVALID_BODY;
}

/* 요청 하나를 읽고 응답한다. 끊긴 연결에는 응답하지 않는다 */
static void respond_to_request(const handlers_context_t *context, int client_fd) {
    http_request_t request;
    http_response_t response = {0};

    http_read_status_t read_status = http_read_request(client_fd, &request);
    if (read_status == HTTP_READ_CLOSED) {
        return;
    }
    if (read_status == HTTP_READ_OK) {
        router_route(context, &request, &response);
    } else {
        api_errors_set_response(&response, to_read_error_kind(read_status), "http_read_request");
    }
    http_write_response(client_fd, &response);
    http_response_destroy(&response);
    http_request_destroy(&request);
}

static void finish_connection(server_t *server) {
    pthread_mutex_lock(&server->connection_lock);
    server->active_connection_count--;
    pthread_cond_broadcast(&server->connection_done);
    pthread_mutex_unlock(&server->connection_lock);
}

static void *handle_connection(void *argument) {
    connection_t *connection = argument;
    server_t *server = connection->server;

    respond_to_request(&server->handlers_context, connection->client_fd);
    close(connection->client_fd);
    free(connection);
    finish_connection(server);
    return NULL;
}

/* 연결 하나를 맡을 스레드를 띄운다. 실패하면 false (호출한 쪽이 client_fd를 닫는다) */
static bool start_connection_thread(server_t *server, int client_fd) {
    connection_t *connection = malloc(sizeof(*connection));
    if (connection == NULL) {
        fprintf(stderr, "[start_connection_thread] 연결을 맡을 메모리가 부족합니다\n");
        return false;
    }
    connection->server = server;
    connection->client_fd = client_fd;

    pthread_mutex_lock(&server->connection_lock);
    server->active_connection_count++;
    pthread_mutex_unlock(&server->connection_lock);

    pthread_t thread;
    if (pthread_create(&thread, NULL, handle_connection, connection) != 0) {
        free(connection);
        finish_connection(server);
        return false;
    }
    pthread_detach(thread);
    return true;
}

/* 종료 신호가 올 때까지 연결을 받는다. poll로 기다려서 주기적으로 신호를 확인한다 */
static void run_accept_loop(server_t *server, int listen_fd) {
    struct pollfd listen_poll = {listen_fd, POLLIN, 0};

    while (!is_shutdown_requested) {
        if (poll(&listen_poll, 1, ACCEPT_POLL_MS) <= 0) {
            continue;
        }
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd < 0) {
            continue;
        }
        http_set_timeout(client_fd, CLIENT_TIMEOUT_MS);
        if (!start_connection_thread(server, client_fd)) {
            close(client_fd);
        }
    }
}

static void wait_for_connections(server_t *server) {
    pthread_mutex_lock(&server->connection_lock);
    while (server->active_connection_count > 0) {
        pthread_cond_wait(&server->connection_done, &server->connection_lock);
    }
    pthread_mutex_unlock(&server->connection_lock);
}

static void init_server(server_t *server, store_t *store) {
    pthread_mutex_init(&server->store_lock, NULL);
    pthread_mutex_init(&server->connection_lock, NULL);
    pthread_cond_init(&server->connection_done, NULL);
    server->active_connection_count = 0;
    server->handlers_context.store = store;
    server->handlers_context.store_lock = &server->store_lock;
}

static void destroy_server(server_t *server) {
    pthread_cond_destroy(&server->connection_done);
    pthread_mutex_destroy(&server->connection_lock);
    pthread_mutex_destroy(&server->store_lock);
}

/* 소켓을 열고 요청을 받다가, 종료 신호가 오면 새 요청 → 정리 작업 → 남은 연결 순으로 멈춘다 */
static int run_server(server_t *server, int port) {
    int listen_fd = open_listen_socket(port);
    if (listen_fd < 0) {
        fprintf(stderr, "[run_server] 포트 %d를 열 수 없습니다\n", port);
        return 1;
    }
    if (sweeper_start(server->handlers_context.store, &server->store_lock) != 0) {
        fprintf(stderr, "[run_server] 만료 정리 스레드를 시작할 수 없습니다\n");
        close(listen_fd);
        return 1;
    }

    printf("미니 레디스 서버 시작 (포트 %d)\n", port);
    fflush(stdout);
    run_accept_loop(server, listen_fd);

    close(listen_fd);
    sweeper_stop();
    wait_for_connections(server);
    printf("미니 레디스 서버 종료\n");
    return 0;
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    server_t server;

    if (argc > 1) {
        port = atoi(argv[1]);
    }

    store_t *store = store_create(NULL);
    if (store == NULL) {
        fprintf(stderr, "[main] 저장소를 만들 메모리가 부족합니다\n");
        return 1;
    }

    install_signal_handlers();
    init_server(&server, store);
    int exit_code = run_server(&server, port);
    destroy_server(&server);
    store_destroy(store);
    return exit_code;
}
