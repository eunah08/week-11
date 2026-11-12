#include <stdio.h>
#include <stdlib.h>

#include "store.h"

/*
 * store.h 공개 함수만 쓴다. 다른 팀원 구현에도 그대로 돌릴 수 있어야 한다. (협약 15장)
 * TTL은 가짜 시계를 끼워서 기다리지 않고 검증한다.
 */

static uint64_t fake_now_ms = 0;
static int failed_count = 0;

static uint64_t get_fake_now_ms(void) {
    return fake_now_ms;
}

#define CHECK(condition)                                                    \
    do {                                                                    \
        if (!(condition)) {                                                 \
            fprintf(stderr, "  실패 %s:%d %s\n", __FILE__, __LINE__, #condition); \
            failed_count++;                                                 \
        }                                                                   \
    } while (0)

static store_t *create_test_store(void) {
    store_options_t options = {0, get_fake_now_ms};
    fake_now_ms = 0;
    return store_create(&options);
}

static void test_create_and_destroy(void) {
    store_t *store = create_test_store();
    CHECK(store != NULL);
    CHECK(store_size(store) == 0);
    store_destroy(store);
}

/* TODO(이니셜): 협약 15.1 필수 항목과 15.5 엣지 케이스를 추가한다 */

int main(void) {
    test_create_and_destroy();

    if (failed_count > 0) {
        printf("test_store : %d개 실패\n", failed_count);
        return 1;
    }
    printf("test_store : 모두 통과\n");
    return 0;
}
