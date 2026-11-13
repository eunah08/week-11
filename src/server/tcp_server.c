#include "tcp_server.h"

#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define LISTEN_BACKLOG 128
#define ACCEPT_POLL_TIMEOUT_MS 200    /* 이 간격마다 종료 신호를 확인한다 */
#define MS_PER_SECOND 1000
#define US_PER_MS 1000
#define NS_PER_MS 1000000L
#define ACCEPT_BACKOFF_MS 100L

/* 받은 연결을 워커에게 넘기는 원형 큐 */
typedef struct {
    int fds[TCP_SERVER_QUEUE_CAPACITY];
    size_t head;
    size_t count;
    bool is_stopping;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
} connection_queue_t;

typedef struct {
    connection_queue_t queue;
    const tcp_server_options_t *options;
} worker_pool_t;

/* 신호 처리기에서는 이 값만 바꾼다 (async-signal-safe) */
static volatile sig_atomic_t is_stop_requested = 0;

static void request_stop(int signal_number) {
    (void)signal_number;
    is_stop_requested = 1;
}

/* SA_RESTART를 주지 않아 poll·accept가 신호로 깨어나게 한다 */
static void install_signal_handlers(void) {
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = request_stop;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    signal(SIGPIPE, SIG_IGN);
    is_stop_requested = 0;
}

static int open_listener(int port) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int reuse = 1;
    struct sockaddr_in address;

    if (listen_fd < 0) {
        return -1;
    }
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((uint16_t)port);
    if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0 ||
        listen(listen_fd, LISTEN_BACKLOG) < 0) {
        close(listen_fd);
        return -1;
    }
    return listen_fd;
}

static void set_socket_timeouts(int fd) {
    struct timeval timeout = {TCP_SERVER_SOCKET_TIMEOUT_MS / MS_PER_SECOND,
                              (TCP_SERVER_SOCKET_TIMEOUT_MS % MS_PER_SECOND) * US_PER_MS};

    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

/* 큐가 가득 차 있으면 거짓 (호출한 쪽이 연결을 닫는다) */
static bool push_connection(connection_queue_t *queue, int client_fd) {
    bool is_pushed = false;

    pthread_mutex_lock(&queue->mutex);
    if (queue->count < TCP_SERVER_QUEUE_CAPACITY) {
        queue->fds[(queue->head + queue->count) % TCP_SERVER_QUEUE_CAPACITY] = client_fd;
        queue->count++;
        is_pushed = true;
        pthread_cond_signal(&queue->not_empty);
    }
    pthread_mutex_unlock(&queue->mutex);
    return is_pushed;
}

/* 연결이 올 때까지 기다린다. 멈추는 중이고 큐가 비었으면 -1 */
static int pop_connection(connection_queue_t *queue) {
    int client_fd = -1;

    pthread_mutex_lock(&queue->mutex);
    while (queue->count == 0 && !queue->is_stopping) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }
    if (queue->count > 0) {
        client_fd = queue->fds[queue->head];
        queue->head = (queue->head + 1) % TCP_SERVER_QUEUE_CAPACITY;
        queue->count--;
    }
    pthread_mutex_unlock(&queue->mutex);
    return client_fd;
}

static void *run_worker(void *argument) {
    worker_pool_t *pool = argument;
    int client_fd = -1;

    while ((client_fd = pop_connection(&pool->queue)) >= 0) {
        pool->options->handle_connection(client_fd, pool->options->context);
    }
    return NULL;
}

/* 워커를 모두 깨워 남은 연결을 처리하게 하고 join한다 */
static void stop_workers(worker_pool_t *pool, pthread_t *workers, size_t started_count) {
    pthread_mutex_lock(&pool->queue.mutex);
    pool->queue.is_stopping = true;
    pthread_cond_broadcast(&pool->queue.not_empty);
    pthread_mutex_unlock(&pool->queue.mutex);
    for (size_t i = 0; i < started_count; i++) {
        pthread_join(workers[i], NULL);
    }
}

static size_t start_workers(worker_pool_t *pool, pthread_t *workers) {
    size_t started_count = 0;

    while (started_count < pool->options->worker_count) {
        if (pthread_create(&workers[started_count], NULL, run_worker, pool) != 0) {
            break;
        }
        started_count++;
    }
    return started_count;
}

static void accept_one(int listen_fd, connection_queue_t *queue) {
    int client_fd = accept(listen_fd, NULL, NULL);
    struct timespec backoff = {0, ACCEPT_BACKOFF_MS * NS_PER_MS};

    if (client_fd < 0) {
        /* fd가 모자라면 poll이 곧바로 다시 깨므로, 잠깐 쉬어 CPU를 계속 쓰지 않게 한다 */
        if (errno == EMFILE || errno == ENFILE) {
            nanosleep(&backoff, NULL);
        }
        return;    /* 신호로 끊겼거나 상대가 먼저 끊음. 다음 반복에서 다시 본다 */
    }
    set_socket_timeouts(client_fd);
    if (!push_connection(queue, client_fd)) {
        fprintf(stderr, "[tcp_server_run] 연결 큐가 가득 차 연결을 닫습니다\n");
        close(client_fd);
    }
}

static void accept_until_stop(int listen_fd, connection_queue_t *queue) {
    struct pollfd listen_poll = {listen_fd, POLLIN, 0};

    while (!is_stop_requested) {
        int ready_count = poll(&listen_poll, 1, ACCEPT_POLL_TIMEOUT_MS);
        if (ready_count > 0 && !is_stop_requested) {
            accept_one(listen_fd, queue);
        }
    }
}

static int serve(int listen_fd, worker_pool_t *pool) {
    pthread_t *workers = calloc(pool->options->worker_count, sizeof(*workers));
    size_t started_count = 0;

    if (workers == NULL) {
        fprintf(stderr, "[tcp_server_run] 워커 목록을 만들 메모리가 부족합니다\n");
        return -1;
    }
    started_count = start_workers(pool, workers);
    if (started_count == pool->options->worker_count) {
        accept_until_stop(listen_fd, &pool->queue);
    } else {
        fprintf(stderr, "[tcp_server_run] 워커 스레드를 만들지 못했습니다\n");
    }
    stop_workers(pool, workers, started_count);
    free(workers);
    return started_count == pool->options->worker_count ? 0 : -1;
}

int tcp_server_run(const tcp_server_options_t *options) {
    worker_pool_t pool;
    int listen_fd = -1;
    int result = 0;

    install_signal_handlers();
    listen_fd = open_listener(options->port);
    if (listen_fd < 0) {
        fprintf(stderr, "[tcp_server_run] 포트 %d를 열지 못했습니다: %s\n", options->port,
                strerror(errno));
        return -1;
    }
    memset(&pool, 0, sizeof(pool));
    pool.options = options;
    pthread_mutex_init(&pool.queue.mutex, NULL);
    pthread_cond_init(&pool.queue.not_empty, NULL);
    result = serve(listen_fd, &pool);
    close(listen_fd);
    pthread_cond_destroy(&pool.queue.not_empty);
    pthread_mutex_destroy(&pool.queue.mutex);
    return result;
}
