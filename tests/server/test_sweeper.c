#include "sweeper.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>

static uint64_t fake_now_ms = 100;
static size_t clock_call_count;
static uint64_t get_fake_now_ms(void) { clock_call_count++; return fake_now_ms; }

static bool wait_for_sweep(server_context_t *server, size_t before) {
    bool has_swept = false;
    for (size_t i = 0; i < 150; i++) {
        struct timespec delay = {0, 10000000}; nanosleep(&delay, NULL);
        pthread_mutex_lock(&server->mutex);
        has_swept = clock_call_count > before;
        pthread_mutex_unlock(&server->mutex);
        if (has_swept) { break; }
    }
    return has_swept;
}

int main(void) {
    store_options_t options = {0, get_fake_now_ms};
    server_context_t server = {store_create(&options), PTHREAD_MUTEX_INITIALIZER};
    assert(server.store != NULL);
    for (size_t i = 0; i < SWEEP_BATCH_SIZE + 5; i++) {
        char key[32]; snprintf(key, sizeof(key), "expired:%zu", i);
        assert(store_set(server.store, key, "v", 1, NULL) == STORE_OK);
    }
    fake_now_ms = 101;
    size_t before = clock_call_count;
    assert(sweeper_start(&server) == 0);
    bool has_swept = wait_for_sweep(&server, before);
    sweeper_stop(); sweeper_stop();
    assert(has_swept);
    /* 통계 호출이 대신 만료 정리하지 않게 시계를 되돌려 물리 삭제를 관찰한다. */
    fake_now_ms = 100;
    store_stats_t stats; store_get_stats(server.store, &stats);
    assert(stats.expired_count == SWEEP_BATCH_SIZE && stats.size == 5);
    store_destroy(server.store); pthread_mutex_destroy(&server.mutex);
    puts("test_sweeper: scheduled cleanup, 200-item bound, joined shutdown OK");
    return 0;
}
