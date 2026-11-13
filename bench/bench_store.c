#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "store.h"

/*
 * 해시 테이블 자체 성능 : 저장·조회 100만 회 (협약 15.4 권장)
 * 네트워크를 빼고 엔진만 재기 위해 store를 직접 링크한다. 협약 6장 "bench는 store를 직접
 * 링크하지 않는다"의 예외로, Makefile과 docs/benchmark.md에 적어 둔다.
 * 실행 : make bench-store
 */

#define OPERATION_COUNT 1000000
#define KEY_BUF_BYTES 32
#define NS_PER_SECOND 1000000000.0
#define NS_PER_MS 1000000.0

static double now_ns(void) {
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec * NS_PER_SECOND + (double)now.tv_nsec;
}

static void print_result(const char *name, double elapsed_ns) {
    printf("| %s | %d | %.1f ms | %.1f ns | %.0f |\n", name, OPERATION_COUNT,
           elapsed_ns / NS_PER_MS, elapsed_ns / OPERATION_COUNT,
           OPERATION_COUNT / (elapsed_ns / NS_PER_SECOND));
}

/* 키 i를 저장한다. 처음 칸 16개에서 시작하므로 rehash 비용이 함께 들어간다 */
static int measure_set(store_t *store, const char *name) {
    char key[KEY_BUF_BYTES];
    double started = now_ns();

    for (int i = 0; i < OPERATION_COUNT; i++) {
        snprintf(key, sizeof(key), "key:%d", i);
        if (store_set(store, key, "value", STORE_NO_TTL, NULL) != STORE_OK) {
            return 1;
        }
    }
    print_result(name, now_ns() - started);
    return 0;
}

static int measure_get(store_t *store, const char *name, const char *key_format) {
    char key[KEY_BUF_BYTES];
    char *value = NULL;
    int found_count = 0;
    double started = now_ns();

    for (int i = 0; i < OPERATION_COUNT; i++) {
        snprintf(key, sizeof(key), key_format, i);
        if (store_get(store, key, &value) == STORE_OK) {
            found_count++;
            free(value);
        }
    }
    print_result(name, now_ns() - started);
    return found_count;
}

int main(void) {
    store_t *store = store_create(NULL);
    store_stats_t stats;

    if (store == NULL) {
        fprintf(stderr, "[bench_store] 저장소를 만들 메모리가 부족합니다\n");
        return 1;
    }
    printf("| 연산 | 횟수 | 전체 시간 | 1회 평균 | 초당 연산 |\n|---|---|---|---|---|\n");
    if (measure_set(store, "저장 (새 키, rehash 포함)") != 0 ||
        measure_set(store, "저장 (덮어쓰기)") != 0) {
        fprintf(stderr, "[bench_store] 저장에 실패했습니다\n");
        store_destroy(store);
        return 1;
    }
    int hit_count = measure_get(store, "조회 (있는 키)", "key:%d");
    int miss_hit_count = measure_get(store, "조회 (없는 키)", "missing:%d");
    store_get_stats(store, &stats);
    printf("\n항목 수 %zu, 칸 수 %zu, 있는 키 적중 %d, 없는 키 적중 %d\n", stats.size,
           stats.capacity, hit_count, miss_hit_count);
    store_destroy(store);
    return 0;
}
