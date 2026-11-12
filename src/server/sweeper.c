#include "sweeper.h"

int sweeper_start(store_t *store) {
    /* TODO(이니셜): SWEEP_INTERVAL_MS마다 store_remove_expired(SWEEP_BATCH_SIZE) 호출 */
    (void)store;
    return 0;
}

void sweeper_stop(void) {
    /* TODO(이니셜): 스레드라면 join까지 한다 */
}
