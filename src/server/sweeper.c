#include "sweeper.h"

#include <stdbool.h>
#include <time.h>

#define MS_PER_SECOND 1000
#define NS_PER_MS 1000000

/* 정리 스레드는 서버에 하나뿐이라 상태를 이 파일 안에 둔다 */
typedef struct {
    store_t *store;
    pthread_mutex_t *store_lock;
    pthread_t thread;
    bool is_running;
    bool is_stop_requested;
    pthread_mutex_t state_lock;   /* is_stop_requested를 두 스레드가 함께 보므로 잠근다 */
} sweeper_t;

static sweeper_t sweeper = {NULL, NULL, 0, false, false, PTHREAD_MUTEX_INITIALIZER};

static bool is_stop_requested(void) {
    pthread_mutex_lock(&sweeper.state_lock);
    bool is_requested = sweeper.is_stop_requested;
    pthread_mutex_unlock(&sweeper.state_lock);
    return is_requested;
}

static void sleep_ms(int duration_ms) {
    struct timespec duration;
    duration.tv_sec = duration_ms / MS_PER_SECOND;
    duration.tv_nsec = (long)(duration_ms % MS_PER_SECOND) * NS_PER_MS;
    nanosleep(&duration, NULL);
}

static void *run_sweeper(void *argument) {
    (void)argument;
    while (!is_stop_requested()) {
        sleep_ms(SWEEP_INTERVAL_MS);
        pthread_mutex_lock(sweeper.store_lock);
        store_remove_expired(sweeper.store, SWEEP_BATCH_SIZE);
        pthread_mutex_unlock(sweeper.store_lock);
    }
    return NULL;
}

int sweeper_start(store_t *store, pthread_mutex_t *store_lock) {
    sweeper.store = store;
    sweeper.store_lock = store_lock;
    sweeper.is_stop_requested = false;
    if (pthread_create(&sweeper.thread, NULL, run_sweeper, NULL) != 0) {
        return -1;
    }
    sweeper.is_running = true;
    return 0;
}

void sweeper_stop(void) {
    if (!sweeper.is_running) {
        return;
    }
    pthread_mutex_lock(&sweeper.state_lock);
    sweeper.is_stop_requested = true;
    pthread_mutex_unlock(&sweeper.state_lock);

    pthread_join(sweeper.thread, NULL);   /* 자고 있던 만큼(최대 SWEEP_INTERVAL_MS) 기다린다 */
    sweeper.is_running = false;
}
