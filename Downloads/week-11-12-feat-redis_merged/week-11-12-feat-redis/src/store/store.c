#include "store.h"

#include <stdlib.h>
#include <string.h>

#include "clock.h"
#include "hash_table.h"

/*
 * 만료·무효화·통계를 맡는다. 네트워크·파일을 모른다. (협약 6장)
 * 동시성 제어는 하지 않는다. 여러 스레드에서 쓰면 호출한 쪽(server)이 잠근다. (협약 12장)
 */

#define NO_EXPIRY 0

/* 해시 테이블의 항목(item)으로 넣는 값 묶음. 키는 해시 테이블이 따로 보관한다 */
typedef struct {
    char *value;
    uint64_t expires_at_ms;    /* 단조 시계 기준. 만료가 없으면 NO_EXPIRY */
} store_item_t;

struct store {
    hash_table_t *hash_table;
    store_clock_fn now_ms;
    store_stats_t stats;       /* hit·miss·expired만 여기서 센다. size·capacity는 그때 계산 */
    size_t sweep_bucket;       /* store_remove_expired가 다음에 이어서 볼 칸 번호 */
};

/* store_delete_by_prefix가 해시 테이블 순회 중에 쓰는 상태 */
typedef struct {
    const char *prefix;
    size_t prefix_len;
    uint64_t now_ms;
    size_t deleted_count;
    size_t expired_count;
} prefix_match_context_t;

static void destroy_item(void *item) {
    store_item_t *store_item = item;

    if (store_item == NULL) {
        return;
    }
    free(store_item->value);
    store_item->value = NULL;
    free(store_item);
}

static char *copy_text(const char *text) {
    size_t text_len = strlen(text);
    char *copy = malloc(text_len + 1);

    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, text_len + 1);
    return copy;
}

/*
 * UTF-8 첫 바이트를 보고 글자 하나의 바이트 수와 허용 범위를 정한다. 잘못된 첫 바이트면 0.
 * 너무 길게 쓴 표현(overlong), 서로게이트(U+D800~DFFF), U+10FFFF 초과는 둘째 바이트 범위로 막는다.
 */
static size_t find_utf8_sequence_len(unsigned char first, unsigned char *out_second_min,
                                     unsigned char *out_second_max) {
    *out_second_min = 0x80;
    *out_second_max = 0xBF;
    if (first < 0x80) {
        return 1;
    }
    if (first >= 0xC2 && first <= 0xDF) {
        return 2;
    }
    if (first >= 0xE0 && first <= 0xEF) {
        *out_second_min = (first == 0xE0) ? 0xA0 : 0x80;
        *out_second_max = (first == 0xED) ? 0x9F : 0xBF;
        return 3;
    }
    if (first >= 0xF0 && first <= 0xF4) {
        *out_second_min = (first == 0xF0) ? 0x90 : 0x80;
        *out_second_max = (first == 0xF4) ? 0x8F : 0xBF;
        return 4;
    }
    return 0;
}

/* 키는 UTF-8 문자열이어야 한다 (협약 7.1) */
static bool is_valid_utf8(const unsigned char *text, size_t text_len) {
    size_t index = 0;

    while (index < text_len) {
        unsigned char second_min = 0;
        unsigned char second_max = 0;
        size_t sequence_len = find_utf8_sequence_len(text[index], &second_min, &second_max);
        if (sequence_len == 0 || index + sequence_len > text_len) {
            return false;
        }
        for (size_t i = 1; i < sequence_len; i++) {
            unsigned char min = (i == 1) ? second_min : 0x80;
            unsigned char max = (i == 1) ? second_max : 0xBF;
            if (text[index + i] < min || text[index + i] > max) {
                return false;
            }
        }
        index += sequence_len;
    }
    return true;
}

/* 키가 아주 길어도 MAX_KEY_BYTES + 1바이트까지만 센다 */
static bool is_valid_key(const char *key) {
    size_t key_len;

    if (key == NULL) {
        return false;
    }
    key_len = strnlen(key, MAX_KEY_BYTES + 1);
    if (key_len == 0 || key_len > MAX_KEY_BYTES) {
        return false;
    }
    return is_valid_utf8((const unsigned char *)key, key_len);
}

static bool is_valid_ttl(int64_t ttl_ms) {
    return ttl_ms >= 1 && ttl_ms <= MAX_TTL_MS;
}

/* 만료 시각 정각도 만료로 본다 (협약 9장) */
static bool is_item_expired(const store_item_t *item, uint64_t now_ms) {
    return item->expires_at_ms != NO_EXPIRY && item->expires_at_ms <= now_ms;
}

static uint64_t to_expires_at_ms(store_t *store, int64_t ttl_ms) {
    if (ttl_ms == STORE_NO_TTL) {
        return NO_EXPIRY;
    }
    return store->now_ms() + (uint64_t)ttl_ms;
}

/*
 * 살아 있는 항목을 찾는다. 만료된 항목을 만나면 그 자리에서 지운다. (협약 9.1 지연 삭제)
 * 판정과 삭제가 한 함수 안에서 끝나므로, 호출한 쪽이 잠금 하나 안에서 부르면 사이에 끼어들 틈이 없다.
 */
static store_item_t *find_live_item_at(store_t *store, const char *key, uint64_t now_ms) {
    store_item_t *item = hash_table_find(store->hash_table, key);

    if (item == NULL) {
        return NULL;
    }
    if (is_item_expired(item, now_ms)) {
        hash_table_remove(store->hash_table, key);
        store->stats.expired_count++;
        return NULL;
    }
    return item;
}

static store_item_t *find_live_item(store_t *store, const char *key) {
    return find_live_item_at(store, key, store->now_ms());
}

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
    store->hash_table = hash_table_create(initial_capacity, destroy_item);
    if (store->hash_table == NULL) {
        free(store);
        return NULL;
    }
    return store;
}

/* 저장된 모든 키·값과 store 자체를 해제한다. NULL이면 아무것도 안 한다. O(n) */
void store_destroy(store_t *store) {
    if (store == NULL) {
        return;
    }
    hash_table_destroy(store->hash_table);
    store->hash_table = NULL;
    free(store);
}

static store_status_t validate_set_input(const char *key, const char *value, int64_t ttl_ms) {
    if (!is_valid_key(key)) {
        return STORE_ERR_INVALID_KEY;
    }
    if (value == NULL) {
        return STORE_ERR_INVALID_VALUE;
    }
    if (strnlen(value, MAX_VALUE_BYTES + 1) > MAX_VALUE_BYTES) {
        return STORE_ERR_VALUE_TOO_LARGE;
    }
    if (ttl_ms != STORE_NO_TTL && !is_valid_ttl(ttl_ms)) {
        return STORE_ERR_INVALID_TTL;
    }
    return STORE_OK;
}

/* 성공하면 value_copy의 소유권이 테이블로 넘어간다. 실패하면 value_copy는 호출한 쪽이 해제한다 */
static store_status_t insert_new_item(store_t *store, const char *key, char *value_copy,
                                      uint64_t expires_at_ms) {
    store_item_t *item = malloc(sizeof(*item));

    if (item == NULL) {
        return STORE_ERR_NO_MEMORY;
    }
    item->value = value_copy;
    item->expires_at_ms = expires_at_ms;
    if (!hash_table_insert(store->hash_table, key, item)) {
        free(item);
        return STORE_ERR_NO_MEMORY;
    }
    return STORE_OK;
}

/*
 * 키와 값을 복사해서 저장한다. 이미 있으면 값과 만료를 모두 덮어쓴다. ttl_ms가 0이면 만료 없음.
 * 반환 : STORE_OK, 입력 오류(상태 변경 없음), STORE_ERR_NO_MEMORY
 * out_is_created : 새로 만들었으면 참 (만료된 키에 저장해도 참)
 * 메모리 : 호출한 쪽은 key, value를 바로 해제해도 된다
 * 시간 복잡도 : 평균 O(1), rehash가 일어나면 O(n)
 */
store_status_t store_set(store_t *store, const char *key, const char *value,
                         int64_t ttl_ms, bool *out_is_created) {
    store_status_t status = validate_set_input(key, value, ttl_ms);
    char *value_copy = NULL;
    store_item_t *item = NULL;

    if (status != STORE_OK) {
        return status;
    }
    value_copy = copy_text(value);
    if (value_copy == NULL) {
        return STORE_ERR_NO_MEMORY;
    }
    item = find_live_item(store, key);
    if (item != NULL) {
        free(item->value);
        item->value = value_copy;
        item->expires_at_ms = to_expires_at_ms(store, ttl_ms);
    } else {
        status = insert_new_item(store, key, value_copy, to_expires_at_ms(store, ttl_ms));
        if (status != STORE_OK) {
            free(value_copy);
            return status;
        }
    }
    if (out_is_created != NULL) {
        *out_is_created = (item == NULL);
    }
    return STORE_OK;
}

/*
 * 값의 복사본을 *out_value에 넣는다. 성공하면 hit_count, 없거나 만료면 miss_count를 올린다.
 * 반환 : STORE_OK, STORE_NOT_FOUND, STORE_ERR_INVALID_KEY, STORE_ERR_NO_MEMORY.
 *        out_value가 NULL이면 STORE_ERR_INVALID_VALUE (결과를 돌려줄 곳이 없다)
 * 메모리 : *out_value는 호출한 쪽이 free()한다. 실패하면 *out_value는 NULL
 * 시간 복잡도 : 평균 O(1) + 값 길이
 */
store_status_t store_get(store_t *store, const char *key, char **out_value) {
    store_item_t *item = NULL;

    if (out_value == NULL) {
        return STORE_ERR_INVALID_VALUE;
    }
    *out_value = NULL;
    if (!is_valid_key(key)) {
        return STORE_ERR_INVALID_KEY;
    }
    item = find_live_item(store, key);
    if (item == NULL) {
        store->stats.miss_count++;
        return STORE_NOT_FOUND;
    }
    *out_value = copy_text(item->value);
    if (*out_value == NULL) {
        return STORE_ERR_NO_MEMORY;
    }
    store->stats.hit_count++;
    return STORE_OK;
}

/*
 * 항목을 지운다. 없거나 만료된 키도 에러가 아니다.
 * out_is_deleted : 살아 있던 항목을 지웠으면 참
 * 시간 복잡도 : 평균 O(1)
 */
store_status_t store_delete(store_t *store, const char *key, bool *out_is_deleted) {
    bool is_deleted = false;

    if (!is_valid_key(key)) {
        return STORE_ERR_INVALID_KEY;
    }
    if (find_live_item(store, key) != NULL) {
        is_deleted = hash_table_remove(store->hash_table, key);
    }
    if (out_is_deleted != NULL) {
        *out_is_deleted = is_deleted;
    }
    return STORE_OK;
}

/* 살아 있는 항목이 있으면 *out_exists가 참. 만료된 항목은 지운다. 평균 O(1) */
store_status_t store_exists(store_t *store, const char *key, bool *out_exists) {
    bool exists = false;

    if (!is_valid_key(key)) {
        return STORE_ERR_INVALID_KEY;
    }
    exists = find_live_item(store, key) != NULL;
    if (out_exists != NULL) {
        *out_exists = exists;
    }
    return STORE_OK;
}

/*
 * 살아 있는 항목에 만료 시간을 지금부터 ttl_ms 뒤로 다시 건다. ttl_ms는 1~MAX_TTL_MS.
 * out_is_applied : 걸었으면 참, 없거나 만료였으면 거짓
 * 시간 복잡도 : 평균 O(1)
 */
store_status_t store_expire(store_t *store, const char *key, int64_t ttl_ms,
                            bool *out_is_applied) {
    store_item_t *item = NULL;

    if (!is_valid_key(key)) {
        return STORE_ERR_INVALID_KEY;
    }
    if (!is_valid_ttl(ttl_ms)) {
        return STORE_ERR_INVALID_TTL;
    }
    item = find_live_item(store, key);
    if (item != NULL) {
        item->expires_at_ms = to_expires_at_ms(store, ttl_ms);
    }
    if (out_is_applied != NULL) {
        *out_is_applied = (item != NULL);
    }
    return STORE_OK;
}

/*
 * 남은 보관 시간(밀리초)을 *out_ttl_ms에 넣는다. 만료가 없으면 STORE_TTL_NO_EXPIRY(-1).
 * 반환 : STORE_OK, 없거나 만료면 STORE_NOT_FOUND, STORE_ERR_INVALID_KEY
 * 시간 복잡도 : 평균 O(1)
 */
store_status_t store_ttl(store_t *store, const char *key, int64_t *out_ttl_ms) {
    store_item_t *item = NULL;
    int64_t ttl_ms = STORE_TTL_NO_EXPIRY;
    uint64_t now_ms = 0;

    if (!is_valid_key(key)) {
        return STORE_ERR_INVALID_KEY;
    }
    /* 시계를 한 번만 읽는다. 판정과 계산 사이에 시각이 바뀌면 음수가 나올 수 있기 때문이다 */
    now_ms = store->now_ms();
    item = find_live_item_at(store, key, now_ms);
    if (item == NULL) {
        return STORE_NOT_FOUND;
    }
    if (item->expires_at_ms != NO_EXPIRY) {
        /* 살아 있는 항목이므로 expires_at_ms > now_ms 이고, 차이는 MAX_TTL_MS 이하다 */
        ttl_ms = (int64_t)(item->expires_at_ms - now_ms);
    }
    if (out_ttl_ms != NULL) {
        *out_ttl_ms = ttl_ms;
    }
    return STORE_OK;
}

/*
 * 살아 있는 항목 수. 만료된 항목을 먼저 모두 정리한 뒤 센다. (협약 9.2)
 * 상태 변경 : 만료 항목을 지우고 expired_count를 올린다
 * 시간 복잡도 : O(capacity + n)
 */
size_t store_size(store_t *store) {
    store_remove_expired(store, SIZE_MAX);
    return hash_table_size(store->hash_table);
}

static bool is_prefix_match(const char *key, void *item, void *context) {
    prefix_match_context_t *match = context;

    if (strncmp(key, match->prefix, match->prefix_len) != 0) {
        return false;
    }
    /* 이미 만료된 키는 지우기는 하되 지운 개수가 아니라 만료 개수로 센다 (협약 8.2) */
    if (is_item_expired(item, match->now_ms)) {
        match->expired_count++;
    } else {
        match->deleted_count++;
    }
    return true;
}

/*
 * 접두사가 같은 키를 모두 지운다. 빈 접두사나 NULL은 STORE_ERR_INVALID_PREFIX.
 * out_deleted_count : 지운 살아 있던 항목 수 (만료 항목 제외)
 * 시간 복잡도 : 테이블 전체를 훑으므로 O(capacity + n)
 */
store_status_t store_delete_by_prefix(store_t *store, const char *prefix,
                                      size_t *out_deleted_count) {
    prefix_match_context_t match = {0};
    hash_table_remove_options_t options = {0};

    if (prefix == NULL || prefix[0] == '\0') {
        return STORE_ERR_INVALID_PREFIX;
    }
    match.prefix = prefix;
    match.prefix_len = strlen(prefix);
    match.now_ms = store->now_ms();
    options.is_match = is_prefix_match;
    options.context = &match;
    options.max_count = SIZE_MAX;
    hash_table_remove_matching(store->hash_table, &options, NULL);
    store->stats.expired_count += match.expired_count;
    if (out_deleted_count != NULL) {
        *out_deleted_count = match.deleted_count;
    }
    return STORE_OK;
}

static bool is_expired_match(const char *key, void *item, void *context) {
    (void)key;
    return is_item_expired(item, *(const uint64_t *)context);
}

/*
 * 만료된 항목을 최대 max_count개 지운다. 지난번에 멈춘 칸부터 이어서 한 바퀴까지 돈다.
 * 반환 : 정리한 개수 (expired_count에도 더한다)
 * 시간 복잡도 : O(capacity + n), 지우는 개수는 max_count 이하
 */
size_t store_remove_expired(store_t *store, size_t max_count) {
    uint64_t now_ms = store->now_ms();
    hash_table_remove_options_t options = {0};
    size_t removed_count = 0;

    options.is_match = is_expired_match;
    options.context = &now_ms;
    options.max_count = max_count;
    options.start_bucket = store->sweep_bucket;
    removed_count = hash_table_remove_matching(store->hash_table, &options,
                                               &store->sweep_bucket);
    store->stats.expired_count += removed_count;
    return removed_count;
}

/* 통계를 *out_stats에 복사한다. size를 세면서 만료 항목을 정리한다. O(capacity + n) */
void store_get_stats(store_t *store, store_stats_t *out_stats) {
    size_t size = 0;

    if (out_stats == NULL) {
        return;
    }
    size = store_size(store);
    *out_stats = store->stats;
    out_stats->size = size;
    out_stats->capacity = hash_table_capacity(store->hash_table);
}
