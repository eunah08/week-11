#include <stdio.h>
#include <stdlib.h>

#include "handlers.h"
#include "router.h"
#include "sweeper.h"

#define DEFAULT_PORT 6380

int main(int argc, char *argv[]) {
    char *end = NULL;
    long port = argc > 1 ? strtol(argv[1], &end, 10) : DEFAULT_PORT;
    if (argc > 2 || port < 1 || port > 65535 || (end != NULL && *end != '\0')) {
        fprintf(stderr, "사용법: %s [port:1~65535]\n", argv[0]); return 1;
    }
    server_context_t server = {store_create(NULL), PTHREAD_MUTEX_INITIALIZER};
    if (server.store == NULL) {
        fprintf(stderr, "[main] 저장소를 만들 메모리가 부족합니다\n"); return 1;
    }
    if (sweeper_start(&server) != 0) {
        store_destroy(server.store); pthread_mutex_destroy(&server.mutex); return 1;
    }
    printf("미니 레디스 127.0.0.1:%ld\n", port); fflush(stdout);
    int status = http_serve((int)port, router_dispatch, &server);
    sweeper_stop();
    store_destroy(server.store);
    pthread_mutex_destroy(&server.mutex);
    return status != 0;
}
