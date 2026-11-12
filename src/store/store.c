#include "store.h"

#include <stdlib.h>

#include "clock.h"
#include "hash_table.h"

/* 만료·무효화·통계를 맡는다. 네트워크·파일을 모른다. (협약 6장) */

#define NO_EXPIRY 0

struct store {
    hash_table_t *hash_table;
    store_clock_fn now_ms;
    store_stats_t stats;
};

/* 메모리가 부족하면 NULL. 해제 책임 : 호출한 쪽이 store_destroy로 해제 */
store_t *store_create(const store_options_t *options) {
    size_t initial_capacity = 0;
    store_t *store = calloc(1, sizeof(*store));
    if (store == NULL) {
        return NULL;
    }

    store->now_ms = clock_now_ms;
    if (options != NULL) {
        initial_capacity = options->initial_capacity;
        if (options->now_ms != NULL) {
            store->now_ms = options->now_ms;
        }
    }

    store->hash_table = hash_table_create(initial_capacity);
    if (store->hash_table == NULL) {
        free(store);
        return NULL;
    }
    return store;
}

void store_destroy(store_t *store) {
    if (store == NULL) {
        return;
    }
    hash_table_destroy(store->hash_table);
    store->hash_table = NULL;
    free(store);
}

store_status_t store_set(store_t *store, const char *key, const char *value,
                         int64_t ttl_ms, bool *out_is_created) {
    /* TODO(이니셜): 협약 8.3, 9장 */
    (void)store;
    (void)key;
    (void)value;
    (void)ttl_ms;
    (void)out_is_created;
    return STORE_OK;
}

store_status_t store_get(store_t *store, const char *key, char **out_value) {
    /* TODO(이니셜): 협약 8.2, 9.1 지연 삭제. 값 복사본은 호출한 쪽이 free() */
    (void)store;
    (void)key;
    if (out_value != NULL) {
        *out_value = NULL;
    }
    return STORE_NOT_FOUND;
}

store_status_t store_delete(store_t *store, const char *key, bool *out_is_deleted) {
    /* TODO(이니셜): 협약 8.2 */
    (void)store;
    (void)key;
    (void)out_is_deleted;
    return STORE_OK;
}

store_status_t store_exists(store_t *store, const char *key, bool *out_exists) {
    /* TODO(이니셜): 협약 8.2 */
    (void)store;
    (void)key;
    (void)out_exists;
    return STORE_OK;
}

store_status_t store_expire(store_t *store, const char *key, int64_t ttl_ms,
                            bool *out_is_applied) {
    /* TODO(이니셜): 협약 8.2, 9장 */
    (void)store;
    (void)key;
    (void)ttl_ms;
    (void)out_is_applied;
    return STORE_OK;
}

store_status_t store_ttl(store_t *store, const char *key, int64_t *out_ttl_ms) {
    /* TODO(이니셜): 협약 8.2. 만료 없음은 STORE_TTL_NO_EXPIRY */
    (void)store;
    (void)key;
    (void)out_ttl_ms;
    return STORE_NOT_FOUND;
}

size_t store_size(store_t *store) {
    /* TODO(이니셜): 만료 항목은 세지 않는다 (협약 9.2) */
    return hash_table_size(store->hash_table);
}

store_status_t store_delete_by_prefix(store_t *store, const char *prefix,
                                      size_t *out_deleted_count) {
    /* TODO(이니셜): 협약 8.4. 빈 접두사는 STORE_ERR_INVALID_PREFIX */
    (void)store;
    (void)prefix;
    (void)out_deleted_count;
    return STORE_OK;
}

size_t store_remove_expired(store_t *store, size_t max_count) {
    /* TODO(이니셜): 협약 9.2 */
    (void)store;
    (void)max_count;
    return 0;
}

void store_get_stats(store_t *store, store_stats_t *out_stats) {
    if (out_stats == NULL) {
        return;
    }
    *out_stats = store->stats;
    out_stats->size = store_size(store);
    out_stats->capacity = hash_table_capacity(store->hash_table);
}
