#include "sweeper.h"

#include <stdbool.h>
#include <time.h>

#define MS_PER_SECOND 1000
#define NS_PER_MS 1000000
#define NS_PER_SECOND 1000000000L

static pthread_t sweeper_thread;
static bool is_running = false;
static bool is_stop_requested = false;
static pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t stop_signal;    /* sweeper_start에서 단조 시계 기준으로 만든다 */
static store_t *target_store = NULL;
static pthread_mutex_t *target_store_mutex = NULL;

/* stop_signal을 CLOCK_MONOTONIC으로 만들었으므로 마감도 단조 시계로 계산한다 (시스템 시각 변경에 영향 없음) */
static struct timespec find_next_deadline(void) {
    struct timespec deadline;

    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += SWEEP_INTERVAL_MS / MS_PER_SECOND;
    deadline.tv_nsec += (long)(SWEEP_INTERVAL_MS % MS_PER_SECOND) * NS_PER_MS;
    if (deadline.tv_nsec >= NS_PER_SECOND) {
        deadline.tv_sec++;
        deadline.tv_nsec -= NS_PER_SECOND;
    }
    return deadline;
}

static void remove_expired_once(void) {
    pthread_mutex_lock(target_store_mutex);
    store_remove_expired(target_store, SWEEP_BATCH_SIZE);
    pthread_mutex_unlock(target_store_mutex);
}

/* 멈추라는 신호를 기다리다가 시간이 되면 한 번 정리한다. 신호를 받으면 바로 끝낸다 */
static void *run_sweeper(void *argument) {
    (void)argument;
    pthread_mutex_lock(&state_mutex);
    while (!is_stop_requested) {
        struct timespec deadline = find_next_deadline();
        int wait_result = 0;
        while (!is_stop_requested && wait_result == 0) {
            wait_result = pthread_cond_timedwait(&stop_signal, &state_mutex, &deadline);
        }
        if (is_stop_requested) {
            break;
        }
        pthread_mutex_unlock(&state_mutex);
        remove_expired_once();    /* 정리하는 동안 state_mutex를 쥐지 않아 stop이 막히지 않는다 */
        pthread_mutex_lock(&state_mutex);
    }
    pthread_mutex_unlock(&state_mutex);
    return NULL;
}

/* 기다리는 시간을 단조 시계로 재는 조건 변수를 만든다. 성공하면 참 */
static bool init_stop_signal(void) {
    pthread_condattr_t attributes;
    bool is_ok = false;

    if (pthread_condattr_init(&attributes) != 0) {
        return false;
    }
    is_ok = pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC) == 0 &&
            pthread_cond_init(&stop_signal, &attributes) == 0;
    pthread_condattr_destroy(&attributes);
    return is_ok;
}

int sweeper_start(store_t *store, pthread_mutex_t *store_mutex) {
    int result = -1;

    pthread_mutex_lock(&state_mutex);
    if (!is_running && init_stop_signal()) {
        target_store = store;
        target_store_mutex = store_mutex;
        is_stop_requested = false;
        is_running = pthread_create(&sweeper_thread, NULL, run_sweeper, NULL) == 0;
        if (!is_running) {
            pthread_cond_destroy(&stop_signal);
        }
        result = is_running ? 0 : -1;
    }
    pthread_mutex_unlock(&state_mutex);
    return result;
}

void sweeper_stop(void) {
    pthread_mutex_lock(&state_mutex);
    if (!is_running) {
        pthread_mutex_unlock(&state_mutex);
        return;
    }
    is_stop_requested = true;
    pthread_cond_signal(&stop_signal);
    pthread_mutex_unlock(&state_mutex);
    pthread_join(sweeper_thread, NULL);
    pthread_mutex_lock(&state_mutex);
    is_running = false;
    pthread_cond_destroy(&stop_signal);
    target_store = NULL;
    target_store_mutex = NULL;
    pthread_mutex_unlock(&state_mutex);
}
