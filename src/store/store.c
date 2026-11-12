#include "store.h"

#include <stdlib.h>
#include <string.h>

#include "clock.h"
#include "hash_table.h"

#define NO_EXPIRY 0

struct store {
    hash_table_t *hash_table;
    store_clock_fn now_ms;
    store_stats_t stats;
};

typedef struct {
    store_t *store;
    uint64_t now_ms;
    const char *prefix;
    size_t deleted_count;
} visit_context_t;

static bool is_utf8_key(const char *key) {
    const unsigned char *cursor = (const unsigned char *)key;
    while (*cursor != 0) {
        unsigned char first = *cursor++;
        if (first < 0x80) { continue; }
        unsigned count = first >= 0xf0 ? 3 : first >= 0xe0 ? 2 : 1;
        uint32_t code = first & (count == 3 ? 7 : count == 2 ? 15 : 31);
        uint32_t minimum = count == 3 ? 0x10000 : count == 2 ? 0x800 : 0x80;
        if (first < 0xc2 || first > 0xf4) { return false; }
        for (unsigned i = 0; i < count; i++) {
            unsigned char next = *cursor++;
            if ((next & 0xc0) != 0x80) { return false; }
            code = (code << 6) | (next & 0x3f);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) {
            return false;
        }
    }
    return true;
}

static store_status_t validate_key(const char *key) {
    if (key == NULL || key[0] == '\0' || strlen(key) > MAX_KEY_BYTES
        || !is_utf8_key(key)) {
        return STORE_ERR_INVALID_KEY;
    }
    return STORE_OK;
}

static store_status_t validate_set(const char *key, const char *value, int64_t ttl_ms) {
    store_status_t status = validate_key(key);
    if (status != STORE_OK) {
        return status;
    }
    if (value == NULL) {
        return STORE_ERR_INVALID_VALUE;
    }
    if (strlen(value) > MAX_VALUE_BYTES) {
        return STORE_ERR_VALUE_TOO_LARGE;
    }
    if (ttl_ms < 0 || ttl_ms > MAX_TTL_MS) {
        return STORE_ERR_INVALID_TTL;
    }
    return STORE_OK;
}

static bool is_expired(const hash_entry_t *entry, uint64_t now_ms) {
    return entry->expires_at_ms != NO_EXPIRY && entry->expires_at_ms <= now_ms;
}

static hash_entry_t *find_live_entry(store_t *store, const char *key, uint64_t now_ms) {
    hash_entry_t *entry = hash_table_find_entry(store->hash_table, key);
    if (entry != NULL && is_expired(entry, now_ms)) {
        hash_table_remove_entry(store->hash_table, key);
        store->stats.expired_count++;
        return NULL;
    }
    return entry;
}

/* options는 빌린 입력. O(capacity), 실패 NULL. 호출자가 destroy로 해제한다. */
store_t *store_create(const store_options_t *options) {
    store_t *store = calloc(1, sizeof(*store));
    if (store == NULL) {
        return NULL;
    }
    store->now_ms = options && options->now_ms ? options->now_ms : clock_now_ms;
    store->hash_table = hash_table_create(options ? options->initial_capacity : 0);
    if (store->hash_table == NULL) {
        free(store);
        return NULL;
    }
    return store;
}

/* O(capacity+n). 모든 저장 메모리 해제. NULL 허용. */
void store_destroy(store_t *store) {
    if (store != NULL) {
        hash_table_destroy(store->hash_table);
        store->hash_table = NULL;
        free(store);
    }
}

/* 입력을 복사한다. 검증 실패 시 상태 불변. 평균 O(1), 확장 O(n). */
store_status_t store_set(store_t *store, const char *key, const char *value,
                         int64_t ttl_ms, bool *out_is_created) {
    store_status_t status = validate_set(key, value, ttl_ms);
    if (status != STORE_OK) {
        return status;
    }
    uint64_t now_ms = store->now_ms();
    hash_entry_t *entry = hash_table_find_entry(store->hash_table, key);
    bool was_expired = entry != NULL && is_expired(entry, now_ms);
    bool is_created = entry == NULL || was_expired;
    uint64_t expiry = ttl_ms == STORE_NO_TTL ? NO_EXPIRY : now_ms + (uint64_t)ttl_ms;
    if (!hash_table_put(store->hash_table, key, value, expiry)) {
        return STORE_ERR_NO_MEMORY;
    }
    if (was_expired) {
        store->stats.expired_count++;
    }
    if (out_is_created != NULL) {
        *out_is_created = is_created;
    }
    return STORE_OK;
}

/* 평균 O(1)+값 길이. 복사본은 호출자가 free. 실패 시 out_value=NULL. */
store_status_t store_get(store_t *store, const char *key, char **out_value) {
    if (out_value != NULL) {
        *out_value = NULL;
    }
    store_status_t status = validate_key(key);
    if (status != STORE_OK) {
        return status;
    }
    if (out_value == NULL) {
        return STORE_ERR_INVALID_VALUE;
    }
    hash_entry_t *entry = find_live_entry(store, key, store->now_ms());
    if (entry == NULL) {
        store->stats.miss_count++;
        return STORE_NOT_FOUND;
    }
    *out_value = strdup(entry->value);
    if (*out_value == NULL) {
        return STORE_ERR_NO_MEMORY;
    }
    store->stats.hit_count++;
    return STORE_OK;
}

/* 평균 O(1), 최악 O(n). 부재도 OK. 삭제된 키·값은 store가 해제한다. */
store_status_t store_delete(store_t *store, const char *key, bool *out_is_deleted) {
    store_status_t status = validate_key(key);
    if (status != STORE_OK) {
        return status;
    }
    bool is_deleted = find_live_entry(store, key, store->now_ms()) != NULL;
    if (is_deleted) {
        hash_table_remove_entry(store->hash_table, key);
    }
    if (out_is_deleted != NULL) {
        *out_is_deleted = is_deleted;
    }
    return STORE_OK;
}

/* 평균 O(1). 만료를 지연 삭제하며 통계 hit/miss는 변경하지 않는다. */
store_status_t store_exists(store_t *store, const char *key, bool *out_exists) {
    store_status_t status = validate_key(key);
    if (status != STORE_OK) {
        return status;
    }
    bool has_entry = find_live_entry(store, key, store->now_ms()) != NULL;
    if (out_exists != NULL) {
        *out_exists = has_entry;
    }
    return STORE_OK;
}

/* 평균 O(1). 검증 후 TTL만 교체. 부재는 OK+false. 메모리 반환 없음. */
store_status_t store_expire(store_t *store, const char *key, int64_t ttl_ms,
                            bool *out_is_applied) {
    store_status_t status = validate_key(key);
    if (status != STORE_OK) {
        return status;
    }
    if (ttl_ms < 1 || ttl_ms > MAX_TTL_MS) {
        return STORE_ERR_INVALID_TTL;
    }
    uint64_t now_ms = store->now_ms();
    hash_entry_t *entry = find_live_entry(store, key, now_ms);
    if (entry != NULL) {
        entry->expires_at_ms = now_ms + (uint64_t)ttl_ms;
    }
    if (out_is_applied != NULL) {
        *out_is_applied = entry != NULL;
    }
    return STORE_OK;
}

/* 평균 O(1). 만료 지연 삭제, 부재 NOT_FOUND. 만료 없음 -1. */
store_status_t store_ttl(store_t *store, const char *key, int64_t *out_ttl_ms) {
    store_status_t status = validate_key(key);
    if (status != STORE_OK) {
        return status;
    }
    uint64_t now_ms = store->now_ms();
    hash_entry_t *entry = find_live_entry(store, key, now_ms);
    if (entry == NULL) {
        return STORE_NOT_FOUND;
    }
    if (out_ttl_ms != NULL) {
        *out_ttl_ms = entry->expires_at_ms == NO_EXPIRY ? STORE_TTL_NO_EXPIRY
                      : (int64_t)(entry->expires_at_ms - now_ms);
    }
    return STORE_OK;
}

static bool remove_if_expired(const hash_entry_t *entry, void *context) {
    visit_context_t *visit = context;
    if (is_expired(entry, visit->now_ms)) {
        visit->store->stats.expired_count++;
        return true;
    }
    return false;
}

/* O(capacity+n), 최대 max_count개만 해제. 만료 판단과 삭제는 호출자 동기화 안. */
size_t store_remove_expired(store_t *store, size_t max_count) {
    visit_context_t visit = {store, store->now_ms(), NULL, 0};
    return hash_table_visit(store->hash_table, remove_if_expired, &visit, max_count);
}

/* O(capacity+n). 만료 항목을 정리한 정확한 개수. 새 메모리 없음. */
size_t store_size(store_t *store) {
    store_remove_expired(store, SIZE_MAX);
    return hash_table_size(store->hash_table);
}

static bool remove_if_prefix(const hash_entry_t *entry, void *context) {
    visit_context_t *visit = context;
    if (remove_if_expired(entry, context)) {
        return true;
    }
    if (strncmp(entry->key, visit->prefix, strlen(visit->prefix)) == 0) {
        visit->deleted_count++;
        return true;
    }
    return false;
}

/* O(capacity+n). 검증 실패 시 불변. 만료 키는 삭제 개수에서 제외. */
store_status_t store_delete_by_prefix(store_t *store, const char *prefix,
                                      size_t *out_deleted_count) {
    if (prefix == NULL || prefix[0] == '\0') {
        return STORE_ERR_INVALID_PREFIX;
    }
    visit_context_t visit = {store, store->now_ms(), prefix, 0};
    hash_table_visit(store->hash_table, remove_if_prefix, &visit, SIZE_MAX);
    if (out_deleted_count != NULL) {
        *out_deleted_count = visit.deleted_count;
    }
    return STORE_OK;
}

/* O(capacity+n). 정리 이후 통계 복사. out_stats=NULL은 결과 생략. */
void store_get_stats(store_t *store, store_stats_t *out_stats) {
    if (out_stats == NULL) {
        return;
    }
    size_t size = store_size(store);
    *out_stats = store->stats;
    out_stats->size = size;
    out_stats->capacity = hash_table_capacity(store->hash_table);
}
