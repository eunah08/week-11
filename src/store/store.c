#include "store.h"

#include <stdlib.h>
#include <string.h>

#include "clock.h"
#include "hash_table.h"

/* 만료·무효화·통계를 맡는다. 네트워크·파일을 모른다. (협약 6장) */

#define NO_EXPIRY 0
#define MIN_EXPIRE_TTL_MS 1

struct store {
    hash_table_t *hash_table;
    store_clock_fn now_ms;
    store_stats_t stats;
};

/* ---------- 입력 검증 (협약 7.1, 8.1) ---------- */

/* NULL, 빈 키, MAX_KEY_BYTES 초과는 잘못된 키다 */
static bool is_valid_key(const char *key) {
    if (key == NULL || key[0] == '\0') {
        return false;
    }
    return strnlen(key, MAX_KEY_BYTES + 1) <= MAX_KEY_BYTES;
}

static store_status_t validate_value(const char *value) {
    if (value == NULL) {
        return STORE_ERR_INVALID_VALUE;
    }
    if (strnlen(value, MAX_VALUE_BYTES + 1) > MAX_VALUE_BYTES) {
        return STORE_ERR_VALUE_TOO_LARGE;
    }
    return STORE_OK;
}

/* store_set은 0(만료 없음)을 허용한다 (협약 8.3) */
static bool is_valid_set_ttl(int64_t ttl_ms) {
    return ttl_ms >= STORE_NO_TTL && ttl_ms <= MAX_TTL_MS;
}

/* store_expire는 1~MAX_TTL_MS만 허용한다 */
static bool is_valid_expire_ttl(int64_t ttl_ms) {
    return ttl_ms >= MIN_EXPIRE_TTL_MS && ttl_ms <= MAX_TTL_MS;
}

/* ---------- 만료 처리 (협약 9장) ---------- */

/* 만료 시각 정각도 만료로 본다 */
static bool is_expired(store_t *store, const hash_table_entry_t *entry) {
    if (entry->expires_at_ms == NO_EXPIRY) {
        return false;
    }
    return entry->expires_at_ms <= store->now_ms();
}

static uint64_t to_expires_at_ms(store_t *store, int64_t ttl_ms) {
    if (ttl_ms == STORE_NO_TTL) {
        return NO_EXPIRY;
    }
    return store->now_ms() + (uint64_t)ttl_ms;
}

static void remove_expired_entry(store_t *store, hash_table_entry_t *entry) {
    hash_table_remove_entry(store->hash_table, entry->key);
    store->stats.expired_count++;
}

/*
 * 살아 있는 항목을 찾는다. 없으면 NULL.
 * 만료된 항목을 만나면 그 자리에서 지우고 없는 키처럼 NULL을 돌려준다. (협약 9.1 지연 삭제)
 */
static hash_table_entry_t *find_live_entry(store_t *store, const char *key) {
    hash_table_entry_t *entry = hash_table_find_entry(store->hash_table, key);
    if (entry == NULL) {
        return NULL;
    }
    if (is_expired(store, entry)) {
        remove_expired_entry(store, entry);
        return NULL;
    }
    return entry;
}

/* ---------- 공개 함수 ---------- */

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

/* store와 저장된 모든 키·값 메모리를 해제한다 */
void store_destroy(store_t *store) {
    if (store == NULL) {
        return;
    }
    hash_table_destroy(store->hash_table);
    store->hash_table = NULL;
    free(store);
}

/*
 * 키·값을 복사해 저장한다. 이미 있으면 값과 만료 기한을 모두 덮어쓴다. (협약 8.3)
 * ttl_ms가 STORE_NO_TTL이면 만료 없음. 만료된 키에 저장하면 새로 만든 것으로 본다.
 * 검증에 실패하면 상태를 바꾸지 않는다. 메모리 부족이면 STORE_ERR_NO_MEMORY.
 * 해제 책임 : 넘긴 문자열은 호출한 쪽이 바로 해제해도 된다. 평균 O(1)
 */
store_status_t store_set(store_t *store, const char *key, const char *value,
                         int64_t ttl_ms, bool *out_is_created) {
    if (!is_valid_key(key)) {
        return STORE_ERR_INVALID_KEY;
    }
    store_status_t value_status = validate_value(value);
    if (value_status != STORE_OK) {
        return value_status;
    }
    if (!is_valid_set_ttl(ttl_ms)) {
        return STORE_ERR_INVALID_TTL;
    }

    bool is_created = false;
    hash_table_entry_t *entry = find_live_entry(store, key);
    if (entry != NULL) {
        if (!hash_table_update_value(entry, value)) {
            return STORE_ERR_NO_MEMORY;
        }
    } else {
        entry = hash_table_insert_entry(store->hash_table, key, value);
        if (entry == NULL) {
            return STORE_ERR_NO_MEMORY;
        }
        is_created = true;
    }
    entry->expires_at_ms = to_expires_at_ms(store, ttl_ms);

    if (out_is_created != NULL) {
        *out_is_created = is_created;
    }
    return STORE_OK;
}

/*
 * 값의 복사본을 *out_value에 넣는다. 없거나 만료면 STORE_NOT_FOUND. (협약 8.2)
 * 실패하면 *out_value는 NULL. out_value 자체는 NULL을 받지 않는다. (협약 8.1)
 * 성공하면 hit_count, STORE_NOT_FOUND이면 miss_count를 1 올린다.
 * 해제 책임 : 호출한 쪽이 *out_value를 free()로 해제한다. 평균 O(1)
 */
store_status_t store_get(store_t *store, const char *key, char **out_value) {
    *out_value = NULL;
    if (!is_valid_key(key)) {
        return STORE_ERR_INVALID_KEY;
    }

    hash_table_entry_t *entry = find_live_entry(store, key);
    if (entry == NULL) {
        store->stats.miss_count++;
        return STORE_NOT_FOUND;
    }

    char *value_copy = strdup(entry->value);
    if (value_copy == NULL) {
        return STORE_ERR_NO_MEMORY;
    }
    store->stats.hit_count++;
    *out_value = value_copy;
    return STORE_OK;
}

/* 항목을 지운다. 없었거나 만료였으면 *out_is_deleted는 false. 평균 O(1) */
store_status_t store_delete(store_t *store, const char *key, bool *out_is_deleted) {
    if (!is_valid_key(key)) {
        return STORE_ERR_INVALID_KEY;
    }

    bool is_deleted = false;
    if (find_live_entry(store, key) != NULL) {
        is_deleted = hash_table_remove_entry(store->hash_table, key);
    }

    if (out_is_deleted != NULL) {
        *out_is_deleted = is_deleted;
    }
    return STORE_OK;
}

/* 살아 있는 항목이 있는지 *out_exists에 넣는다. 평균 O(1) */
store_status_t store_exists(store_t *store, const char *key, bool *out_exists) {
    if (!is_valid_key(key)) {
        return STORE_ERR_INVALID_KEY;
    }

    bool exists = find_live_entry(store, key) != NULL;
    if (out_exists != NULL) {
        *out_exists = exists;
    }
    return STORE_OK;
}

/*
 * 이미 있는 항목에 지금부터 ttl_ms 뒤 만료를 다시 건다. ttl_ms는 1~MAX_TTL_MS.
 * 없거나 만료였으면 *out_is_applied는 false. 평균 O(1)
 */
store_status_t store_expire(store_t *store, const char *key, int64_t ttl_ms,
                            bool *out_is_applied) {
    if (!is_valid_key(key)) {
        return STORE_ERR_INVALID_KEY;
    }
    if (!is_valid_expire_ttl(ttl_ms)) {
        return STORE_ERR_INVALID_TTL;
    }

    bool is_applied = false;
    hash_table_entry_t *entry = find_live_entry(store, key);
    if (entry != NULL) {
        entry->expires_at_ms = to_expires_at_ms(store, ttl_ms);
        is_applied = true;
    }

    if (out_is_applied != NULL) {
        *out_is_applied = is_applied;
    }
    return STORE_OK;
}

/*
 * 남은 보관 시간(밀리초)을 *out_ttl_ms에 넣는다. 만료 없음은 STORE_TTL_NO_EXPIRY.
 * 없거나 만료면 STORE_NOT_FOUND. 평균 O(1)
 */
store_status_t store_ttl(store_t *store, const char *key, int64_t *out_ttl_ms) {
    if (!is_valid_key(key)) {
        return STORE_ERR_INVALID_KEY;
    }

    hash_table_entry_t *entry = find_live_entry(store, key);
    if (entry == NULL) {
        return STORE_NOT_FOUND;
    }

    int64_t ttl_ms = STORE_TTL_NO_EXPIRY;
    if (entry->expires_at_ms != NO_EXPIRY) {
        ttl_ms = (int64_t)(entry->expires_at_ms - store->now_ms());
    }
    if (out_ttl_ms != NULL) {
        *out_ttl_ms = ttl_ms;
    }
    return STORE_OK;
}

/* 만료 항목을 먼저 모두 정리한 뒤 센다. 그래서 만료 항목은 세지 않는다. O(n) (협약 9.2) */
size_t store_size(store_t *store) {
    store_remove_expired(store, SIZE_MAX);
    return hash_table_size(store->hash_table);
}

static bool has_prefix(const char *key, const char *prefix) {
    return strncmp(key, prefix, strlen(prefix)) == 0;
}

/* bucket_index번 칸에서 접두사가 같은 살아 있는 항목을 지우고, 지운 개수를 돌려준다 */
static size_t delete_bucket_by_prefix(store_t *store, size_t bucket_index, const char *prefix) {
    size_t deleted_count = 0;
    hash_table_entry_t *entry = hash_table_get_bucket(store->hash_table, bucket_index);

    while (entry != NULL) {
        hash_table_entry_t *next_entry = entry->next;   /* 지우면 entry를 못 쓰므로 미리 기억 */
        if (has_prefix(entry->key, prefix)) {
            if (is_expired(store, entry)) {
                remove_expired_entry(store, entry);   /* 만료 키는 지운 개수에 세지 않는다 */
            } else {
                hash_table_remove_entry(store->hash_table, entry->key);
                deleted_count++;
            }
        }
        entry = next_entry;
    }
    return deleted_count;
}

/*
 * 접두사가 같은 키를 모두 지운다. NULL이나 빈 접두사는 STORE_ERR_INVALID_PREFIX. (협약 8.4)
 * 테이블 전체를 훑으므로 O(n)
 */
store_status_t store_delete_by_prefix(store_t *store, const char *prefix,
                                      size_t *out_deleted_count) {
    if (prefix == NULL || prefix[0] == '\0') {
        return STORE_ERR_INVALID_PREFIX;
    }

    size_t deleted_count = 0;
    size_t capacity = hash_table_capacity(store->hash_table);
    for (size_t i = 0; i < capacity; i++) {
        deleted_count += delete_bucket_by_prefix(store, i, prefix);
    }

    if (out_deleted_count != NULL) {
        *out_deleted_count = deleted_count;
    }
    return STORE_OK;
}

/* bucket_index번 칸의 만료 항목을 최대 max_count개 지우고, 지운 개수를 돌려준다 */
static size_t remove_expired_in_bucket(store_t *store, size_t bucket_index, size_t max_count) {
    size_t removed_count = 0;
    hash_table_entry_t *entry = hash_table_get_bucket(store->hash_table, bucket_index);

    while (entry != NULL && removed_count < max_count) {
        hash_table_entry_t *next_entry = entry->next;   /* 지우면 entry를 못 쓰므로 미리 기억 */
        if (is_expired(store, entry)) {
            remove_expired_entry(store, entry);
            removed_count++;
        }
        entry = next_entry;
    }
    return removed_count;
}

/*
 * 만료된 항목을 최대 max_count개 정리하고 정리한 개수를 돌려준다. (협약 9.2)
 * 0번 칸부터 차례로 훑는다. 최악 O(n)
 */
size_t store_remove_expired(store_t *store, size_t max_count) {
    size_t removed_count = 0;
    size_t capacity = hash_table_capacity(store->hash_table);

    for (size_t i = 0; i < capacity && removed_count < max_count; i++) {
        removed_count += remove_expired_in_bucket(store, i, max_count - removed_count);
    }
    return removed_count;
}

/* 통계를 *out_stats에 복사한다. size를 세면서 만료 항목이 정리된다. O(n) */
void store_get_stats(store_t *store, store_stats_t *out_stats) {
    if (out_stats == NULL) {
        return;
    }
    size_t size = store_size(store);
    *out_stats = store->stats;
    out_stats->size = size;
    out_stats->capacity = hash_table_capacity(store->hash_table);
}
