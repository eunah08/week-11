#include "sweeper.h"

#include <stdatomic.h>
#include <time.h>

#define STOP_CHECK_MS 100
static pthread_t sweeper_thread;
static atomic_bool is_running;
static bool has_thread;

static void *sweep_loop(void *context) {
    server_context_t *server = context;
    unsigned elapsed_ms = 0;
    while (atomic_load(&is_running)) {
        struct timespec delay = {0, STOP_CHECK_MS * 1000000L};
        nanosleep(&delay, NULL);
        elapsed_ms += STOP_CHECK_MS;
        if (elapsed_ms >= SWEEP_INTERVAL_MS && atomic_load(&is_running)) {
            pthread_mutex_lock(&server->mutex);
            store_remove_expired(server->store, SWEEP_BATCH_SIZE);
            pthread_mutex_unlock(&server->mutex);
            elapsed_ms = 0;
        }
    }
    return NULL;
}

int sweeper_start(server_context_t *server) {
    if (has_thread) { return -1; }
    atomic_store(&is_running, true);
    if (pthread_create(&sweeper_thread, NULL, sweep_loop, server) != 0) {
        atomic_store(&is_running, false);
        return -1;
    }
    has_thread = true;
    return 0;
}

void sweeper_stop(void) {
    if (has_thread) {
        atomic_store(&is_running, false);
        pthread_join(sweeper_thread, NULL);
        has_thread = false;
    }
}
