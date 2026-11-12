#include <stdio.h>
#include <stdlib.h>

#include "slow_source.h"

/*
 * 캐시 샘플 API. 미니 레디스에는 HTTP API로만 접근한다. (협약 6장, 13장)
 * GET /products/{id}            캐시 사용 (Cache-Aside)
 * GET /products/{id}?cache=off  캐시 사용 안 함
 * PUT /products/{id}            원본 수정 + product:{id} 무효화
 * GET /demo/stats               {"originCallCount": n}
 */

#define DEFAULT_PORT 3000
#define CACHE_TTL_MS 60000

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;

    if (argc > 1) {
        port = atoi(argv[1]);
    }

    /* TODO(이니셜): HTTP 서버, X-Cache 헤더, 미니 레디스가 꺼져 있을 때 원본으로 응답 */
    printf("샘플 API (포트 %d) - 아직 구현하지 않았다\n", port);
    return 0;
}
