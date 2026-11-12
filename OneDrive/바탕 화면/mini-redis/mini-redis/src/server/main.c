#include <stdio.h>
#include <stdlib.h>

#include "store.h"
#include "sweeper.h"

#define DEFAULT_PORT 6380

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    store_t *store = NULL;

    if (argc > 1) {
        port = atoi(argv[1]);
    }

    store = store_create(NULL);
    if (store == NULL) {
        fprintf(stderr, "[main] 저장소를 만들 메모리가 부족합니다\n");
        return 1;
    }

    /* TODO(이니셜): SIGPIPE 무시, SIGINT·SIGTERM 처리, 소켓 열기, 요청 루프 (협약 11.5, 12장) */
    printf("미니 레디스 서버 (포트 %d) - 아직 요청 처리를 구현하지 않았다\n", port);

    sweeper_stop();
    store_destroy(store);
    return 0;
}
