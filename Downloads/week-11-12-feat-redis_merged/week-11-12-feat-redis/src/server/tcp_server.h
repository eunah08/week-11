#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <stddef.h>

/*
 * 연결 받기와 워커 스레드 풀. (협약 11.5, 12장)
 * 메인 스레드는 accept만 하고, 받은 연결을 큐에 넣는다. 워커 worker_count개가 큐에서 꺼내
 * handle_connection을 부른다. HTTP도 store도 모른다. 미니 레디스와 샘플 API가 같이 쓴다.
 */

#define TCP_SERVER_QUEUE_CAPACITY 1024    /* 큐가 가득 차면 새 연결은 바로 닫는다 */
#define TCP_SERVER_SOCKET_TIMEOUT_MS 5000 /* 느린 상대가 워커를 오래 붙잡지 못하게 한다 */

/* 연결 하나를 처리하고 client_fd를 닫는다. 여러 워커에서 동시에 불린다 */
typedef void (*tcp_server_handle_fn)(int client_fd, void *context);

typedef struct {
    int port;
    size_t worker_count;
    tcp_server_handle_fn handle_connection;
    void *context;    /* handle_connection에 그대로 넘긴다 */
} tcp_server_options_t;

/*
 * SIGINT·SIGTERM을 받을 때까지 연결을 받는다. SIGPIPE는 무시하도록 바꾼다.
 * 신호를 받으면 새 연결을 멈추고, 이미 받은 연결은 처리한 뒤, 워커를 모두 join하고 돌아온다.
 * 반환 : 정상 종료 0, 소켓·스레드를 만들지 못하면 -1 (stderr에 이유를 남긴다)
 */
int tcp_server_run(const tcp_server_options_t *options);

#endif
