#include "sweeper.h"

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#define WAIT_STEP_NS 10000000L
#define WAIT_STEP_LIMIT 150
#define EXTRA_EXPIRED_COUNT 5

static uint64_t fake_now_ms = 100;
static size_t clock_call_count;
static pthread_mutex_t store_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint64_t get_fake_now_ms(void) {
    clock_call_count++;
    return fake_now_ms;
}

/* sweeper가 store를 한 번이라도 건드렸는지(시계를 읽었는지) 최대 1.5초 기다린다 */
static bool wait_for_sweep(size_t clock_call_count_before) {
    bool has_swept = false;

    for (size_t i = 0; i < WAIT_STEP_LIMIT; i++) {
        struct timespec delay = {0, WAIT_STEP_NS};
        nanosleep(&delay, NULL);
        pthread_mutex_lock(&store_mutex);
        has_swept = clock_call_count > clock_call_count_before;
        pthread_mutex_unlock(&store_mutex);
        if (has_swept) {
            break;
        }
    }
    return has_swept;
}

int main(void) {
    store_options_t options = {0, get_fake_now_ms};
    store_t *store = store_create(&options);
    assert(store != NULL);

    for (size_t i = 0; i < SWEEP_BATCH_SIZE + EXTRA_EXPIRED_COUNT; i++) {
        char key[32];
        snprintf(key, sizeof(key), "expired:%zu", i);
        assert(store_set(store, key, "v", 1, NULL) == STORE_OK);
    }
    fake_now_ms = 101;

    size_t clock_call_count_before = clock_call_count;
    assert(sweeper_start(store, &store_mutex) == 0);
    bool has_swept = wait_for_sweep(clock_call_count_before);
    sweeper_stop();
    sweeper_stop();
    assert(has_swept);

    /* 통계 호출이 대신 만료 정리하지 않게 시계를 되돌려, sweeper가 지운 개수만 본다 */
    fake_now_ms = 100;
    store_stats_t stats;
    store_get_stats(store, &stats);
    assert(stats.expired_count == SWEEP_BATCH_SIZE);
    assert(stats.size == EXTRA_EXPIRED_COUNT);

    store_destroy(store);
    pthread_mutex_destroy(&store_mutex);
    puts("test_sweeper : 주기적 정리, 한 번에 200개 상한, 종료 시 join 확인");
    return 0;
}
