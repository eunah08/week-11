#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "store.h"

/*
 * store.h 공개 함수만 쓴다. 다른 팀원 구현에도 그대로 돌릴 수 있어야 한다. (협약 15장)
 * TTL은 가짜 시계를 끼워서 기다리지 않고 검증한다.
 */

#define TEST_START_MS 1000
#define MANY_KEY_COUNT 10000

static uint64_t fake_now_ms = TEST_START_MS;
static int failed_count = 0;

static uint64_t get_fake_now_ms(void) {
    return fake_now_ms;
}

#define CHECK(condition)                                                          \
    do {                                                                          \
        if (!(condition)) {                                                       \
            fprintf(stderr, "  실패 %s:%d %s\n", __FILE__, __LINE__, #condition); \
            failed_count++;                                                       \
        }                                                                         \
    } while (0)

static store_t *create_test_store(void) {
    store_options_t options = {0, get_fake_now_ms};

    fake_now_ms = TEST_START_MS;
    return store_create(&options);
}

/* 값을 꺼내 expected와 같은지 본다. expected가 NULL이면 "없음"이어야 한다 */
static int has_value(store_t *store, const char *key, const char *expected) {
    char *value = NULL;
    store_status_t status = store_get(store, key, &value);
    int is_same = 0;

    if (expected == NULL) {
        is_same = (status == STORE_NOT_FOUND && value == NULL);
    } else {
        is_same = (status == STORE_OK && value != NULL && strcmp(value, expected) == 0);
    }
    free(value);
    return is_same;
}

static char *create_filled_text(size_t length) {
    char *text = malloc(length + 1);

    if (text != NULL) {
        memset(text, 'a', length);
        text[length] = '\0';
    }
    return text;
}

static void test_create_and_destroy(void) {
    store_t *store = create_test_store();

    CHECK(store != NULL);
    CHECK(store_size(store) == 0);
    store_destroy(store);
    store_destroy(NULL);
}

static void test_set_and_get(void) {
    store_t *store = create_test_store();
    bool is_created = false;

    CHECK(store_set(store, "a", "1", STORE_NO_TTL, &is_created) == STORE_OK);
    CHECK(is_created);
    CHECK(has_value(store, "a", "1"));
    CHECK(store_set(store, "a", "2", STORE_NO_TTL, &is_created) == STORE_OK);
    CHECK(!is_created);
    CHECK(has_value(store, "a", "2"));
    CHECK(has_value(store, "missing", NULL));
    CHECK(store_size(store) == 1);
    store_destroy(store);
}

static void test_empty_value_is_not_missing(void) {
    store_t *store = create_test_store();

    CHECK(store_set(store, "empty", "", STORE_NO_TTL, NULL) == STORE_OK);
    CHECK(has_value(store, "empty", ""));
    store_destroy(store);
}

static void test_set_copies_input(void) {
    store_t *store = create_test_store();
    char key[] = "copy";
    char value[] = "before";

    store_set(store, key, value, STORE_NO_TTL, NULL);
    strcpy(value, "after!");
    key[0] = 'X';
    CHECK(has_value(store, "copy", "before"));
    store_destroy(store);
}

static void test_delete(void) {
    store_t *store = create_test_store();
    bool is_deleted = false;

    store_set(store, "a", "1", STORE_NO_TTL, NULL);
    CHECK(store_delete(store, "a", &is_deleted) == STORE_OK);
    CHECK(is_deleted);
    CHECK(has_value(store, "a", NULL));
    CHECK(store_delete(store, "a", &is_deleted) == STORE_OK);
    CHECK(!is_deleted);
    CHECK(store_delete(store, "never", &is_deleted) == STORE_OK);
    CHECK(!is_deleted);
    CHECK(store_set(store, "a", "again", STORE_NO_TTL, &is_deleted) == STORE_OK);
    CHECK(is_deleted);    /* 지운 키를 다시 저장하면 새로 만든 것 */
    CHECK(has_value(store, "a", "again"));
    store_destroy(store);
}

static void test_exists(void) {
    store_t *store = create_test_store();
    bool exists = true;

    CHECK(store_exists(store, "a", &exists) == STORE_OK);
    CHECK(!exists);
    store_set(store, "a", "1", 10, NULL);
    CHECK(store_exists(store, "a", &exists) == STORE_OK);
    CHECK(exists);
    fake_now_ms += 10;
    CHECK(store_exists(store, "a", &exists) == STORE_OK);
    CHECK(!exists);
    store_destroy(store);
}

static void test_invalid_key(void) {
    store_t *store = create_test_store();
    char *key_256 = create_filled_text(MAX_KEY_BYTES);
    char *key_257 = create_filled_text(MAX_KEY_BYTES + 1);
    char *value = NULL;

    CHECK(store_set(store, NULL, "v", STORE_NO_TTL, NULL) == STORE_ERR_INVALID_KEY);
    CHECK(store_set(store, "", "v", STORE_NO_TTL, NULL) == STORE_ERR_INVALID_KEY);
    CHECK(store_set(store, key_256, "v", STORE_NO_TTL, NULL) == STORE_OK);
    CHECK(store_set(store, key_257, "v", STORE_NO_TTL, NULL) == STORE_ERR_INVALID_KEY);
    CHECK(store_set(store, "\xff", "v", STORE_NO_TTL, NULL) == STORE_ERR_INVALID_KEY);
    CHECK(store_set(store, "\xc0\xaf", "v", STORE_NO_TTL, NULL) == STORE_ERR_INVALID_KEY);
    CHECK(store_set(store, "\xed\xa0\x80", "v", STORE_NO_TTL, NULL) == STORE_ERR_INVALID_KEY);
    CHECK(store_set(store, "\xe2\x82", "v", STORE_NO_TTL, NULL) == STORE_ERR_INVALID_KEY);
    CHECK(store_get(store, "", &value) == STORE_ERR_INVALID_KEY);
    CHECK(value == NULL);
    CHECK(store_delete(store, NULL, NULL) == STORE_ERR_INVALID_KEY);
    CHECK(store_exists(store, key_257, NULL) == STORE_ERR_INVALID_KEY);
    CHECK(store_expire(store, "", 10, NULL) == STORE_ERR_INVALID_KEY);
    CHECK(store_ttl(store, NULL, NULL) == STORE_ERR_INVALID_KEY);
    CHECK(store_size(store) == 1);
    free(key_256);
    free(key_257);
    store_destroy(store);
}

static void test_invalid_value(void) {
    store_t *store = create_test_store();
    char *value_max = create_filled_text(MAX_VALUE_BYTES);
    char *value_over = create_filled_text(MAX_VALUE_BYTES + 1);

    CHECK(store_set(store, "a", NULL, STORE_NO_TTL, NULL) == STORE_ERR_INVALID_VALUE);
    CHECK(store_set(store, "a", value_max, STORE_NO_TTL, NULL) == STORE_OK);
    CHECK(has_value(store, "a", value_max));
    CHECK(store_set(store, "a", value_over, STORE_NO_TTL, NULL) == STORE_ERR_VALUE_TOO_LARGE);
    CHECK(has_value(store, "a", value_max));
    free(value_max);
    free(value_over);
    store_destroy(store);
}

static void test_ttl_expiry_boundary(void) {
    store_t *store = create_test_store();

    store_set(store, "a", "1", 100, NULL);
    fake_now_ms += 99;
    CHECK(has_value(store, "a", "1"));
    fake_now_ms += 1;    /* 만료 시각 정각 */
    CHECK(has_value(store, "a", NULL));
    fake_now_ms += 1000;
    CHECK(has_value(store, "a", NULL));
    store_destroy(store);
}

static void test_set_without_ttl_clears_expiry(void) {
    store_t *store = create_test_store();
    int64_t ttl_ms = 0;

    store_set(store, "a", "1", 100, NULL);
    store_set(store, "a", "2", STORE_NO_TTL, NULL);
    CHECK(store_ttl(store, "a", &ttl_ms) == STORE_OK);
    CHECK(ttl_ms == STORE_TTL_NO_EXPIRY);
    fake_now_ms += 1000;
    CHECK(has_value(store, "a", "2"));
    store_destroy(store);
}

static void test_set_replaces_ttl(void) {
    store_t *store = create_test_store();
    int64_t ttl_ms = 0;

    store_set(store, "a", "1", 100, NULL);
    fake_now_ms += 50;
    store_set(store, "a", "2", 300, NULL);
    CHECK(store_ttl(store, "a", &ttl_ms) == STORE_OK);
    CHECK(ttl_ms == 300);
    fake_now_ms += 299;
    CHECK(has_value(store, "a", "2"));
    store_destroy(store);
}

static void test_expire_and_ttl(void) {
    store_t *store = create_test_store();
    bool is_applied = false;
    int64_t ttl_ms = 0;

    store_set(store, "a", "1", STORE_NO_TTL, NULL);
    CHECK(store_ttl(store, "a", &ttl_ms) == STORE_OK);
    CHECK(ttl_ms == STORE_TTL_NO_EXPIRY);
    CHECK(store_expire(store, "a", 500, &is_applied) == STORE_OK);
    CHECK(is_applied);
    fake_now_ms += 120;
    CHECK(store_ttl(store, "a", &ttl_ms) == STORE_OK);
    CHECK(ttl_ms == 380);
    fake_now_ms += 380;
    CHECK(store_ttl(store, "a", &ttl_ms) == STORE_NOT_FOUND);
    CHECK(store_expire(store, "a", 500, &is_applied) == STORE_OK);
    CHECK(!is_applied);    /* 만료된 키에 expire는 거짓 */
    CHECK(store_expire(store, "never", 500, &is_applied) == STORE_OK);
    CHECK(!is_applied);
    CHECK(store_ttl(store, "never", &ttl_ms) == STORE_NOT_FOUND);
    store_destroy(store);
}

static void test_invalid_ttl_keeps_old_state(void) {
    store_t *store = create_test_store();
    int64_t ttl_ms = 0;

    store_set(store, "a", "1", 100, NULL);
    CHECK(store_set(store, "a", "2", -1, NULL) == STORE_ERR_INVALID_TTL);
    CHECK(store_set(store, "a", "2", (int64_t)MAX_TTL_MS + 1, NULL) == STORE_ERR_INVALID_TTL);
    CHECK(store_expire(store, "a", 0, NULL) == STORE_ERR_INVALID_TTL);
    CHECK(store_expire(store, "a", -5, NULL) == STORE_ERR_INVALID_TTL);
    CHECK(store_set(store, "a", "max", MAX_TTL_MS, NULL) == STORE_OK);
    CHECK(store_set(store, "a", "1", 100, NULL) == STORE_OK);
    CHECK(store_set(store, "a", "2", -1, NULL) == STORE_ERR_INVALID_TTL);
    CHECK(has_value(store, "a", "1"));
    CHECK(store_ttl(store, "a", &ttl_ms) == STORE_OK);
    CHECK(ttl_ms == 100);
    store_destroy(store);
}

static void test_set_on_expired_key_is_created(void) {
    store_t *store = create_test_store();
    bool is_created = false;

    store_set(store, "a", "old", 10, NULL);
    fake_now_ms += 10;
    CHECK(store_set(store, "a", "new", STORE_NO_TTL, &is_created) == STORE_OK);
    CHECK(is_created);
    CHECK(has_value(store, "a", "new"));
    store_destroy(store);
}

static void test_delete_expired_key_is_false(void) {
    store_t *store = create_test_store();
    bool is_deleted = true;

    store_set(store, "a", "1", 10, NULL);
    fake_now_ms += 10;
    CHECK(store_delete(store, "a", &is_deleted) == STORE_OK);
    CHECK(!is_deleted);
    store_destroy(store);
}

/* 만료 판정과 정리 사이에 같은 키를 다시 저장하면 새 값이 남아야 한다 (협약 15.1 지연 정리) */
static void test_resave_before_sweep_keeps_new_value(void) {
    store_t *store = create_test_store();

    store_set(store, "a", "old", 10, NULL);
    fake_now_ms += 10;
    store_set(store, "a", "new", STORE_NO_TTL, NULL);
    CHECK(store_remove_expired(store, SIZE_MAX) == 0);
    CHECK(has_value(store, "a", "new"));
    store_destroy(store);
}

static void test_delete_by_prefix(void) {
    store_t *store = create_test_store();
    size_t deleted_count = 99;

    store_set(store, "product:1", "a", STORE_NO_TTL, NULL);
    store_set(store, "product:2", "b", STORE_NO_TTL, NULL);
    store_set(store, "product:3", "c", 10, NULL);
    store_set(store, "user:1", "d", STORE_NO_TTL, NULL);
    store_set(store, "productX", "e", STORE_NO_TTL, NULL);
    fake_now_ms += 10;    /* product:3 만료 */
    CHECK(store_delete_by_prefix(store, "product:", &deleted_count) == STORE_OK);
    CHECK(deleted_count == 2);
    CHECK(has_value(store, "product:1", NULL));
    CHECK(has_value(store, "user:1", "d"));
    CHECK(has_value(store, "productX", "e"));
    CHECK(store_size(store) == 2);
    CHECK(store_delete_by_prefix(store, "", &deleted_count) == STORE_ERR_INVALID_PREFIX);
    CHECK(store_delete_by_prefix(store, NULL, &deleted_count) == STORE_ERR_INVALID_PREFIX);
    CHECK(store_delete_by_prefix(store, "none:", &deleted_count) == STORE_OK);
    CHECK(deleted_count == 0);
    store_destroy(store);
}

static void test_remove_expired_with_limit(void) {
    store_t *store = create_test_store();
    char key[32];

    for (int i = 0; i < 10; i++) {
        snprintf(key, sizeof(key), "ttl:%d", i);
        store_set(store, key, "v", 10, NULL);
    }
    store_set(store, "keep", "v", STORE_NO_TTL, NULL);
    fake_now_ms += 10;
    CHECK(store_remove_expired(store, 3) == 3);
    CHECK(store_remove_expired(store, 0) == 0);
    CHECK(store_remove_expired(store, SIZE_MAX) == 7);
    CHECK(store_remove_expired(store, SIZE_MAX) == 0);
    CHECK(store_size(store) == 1);
    CHECK(has_value(store, "keep", "v"));
    store_destroy(store);
}

static void test_many_keys_after_rehash(void) {
    store_options_t options = {4, get_fake_now_ms};
    store_t *store = store_create(&options);
    store_stats_t stats;
    char key[32];
    char value[32];
    int lost_count = 0;

    store_get_stats(store, &stats);
    CHECK(stats.capacity >= 4);
    size_t first_capacity = stats.capacity;
    for (int i = 0; i < MANY_KEY_COUNT; i++) {
        snprintf(key, sizeof(key), "key:%d", i);
        snprintf(value, sizeof(value), "value:%d", i);
        store_set(store, key, value, STORE_NO_TTL, NULL);
    }
    for (int i = 0; i < MANY_KEY_COUNT; i++) {
        snprintf(key, sizeof(key), "key:%d", i);
        snprintf(value, sizeof(value), "value:%d", i);
        lost_count += !has_value(store, key, value);
    }
    store_get_stats(store, &stats);
    CHECK(lost_count == 0);
    CHECK(stats.size == MANY_KEY_COUNT);
    CHECK(stats.capacity > first_capacity);
    CHECK(stats.capacity >= stats.size);
    store_destroy(store);
}

static void test_size_and_stats(void) {
    store_t *store = create_test_store();
    store_stats_t stats;

    store_get_stats(store, &stats);
    CHECK(stats.size == 0 && stats.hit_count == 0 && stats.miss_count == 0);
    store_set(store, "a", "1", STORE_NO_TTL, NULL);
    store_set(store, "b", "2", 10, NULL);
    store_set(store, "c", "3", 10, NULL);
    CHECK(store_size(store) == 3);
    CHECK(has_value(store, "a", "1"));
    CHECK(has_value(store, "zz", NULL));
    fake_now_ms += 10;
    CHECK(has_value(store, "b", NULL));    /* 지연 삭제 + miss */
    CHECK(store_size(store) == 1);         /* c는 size가 정리 */
    store_delete(store, "a", NULL);
    store_get_stats(store, &stats);
    CHECK(stats.size == 0);
    CHECK(stats.hit_count == 1);
    CHECK(stats.miss_count == 2);
    CHECK(stats.expired_count == 2);
    store_get_stats(store, NULL);
    store_destroy(store);
}

static void test_empty_store_operations(void) {
    store_t *store = create_test_store();
    size_t deleted_count = 99;
    store_stats_t stats;

    CHECK(has_value(store, "a", NULL));
    CHECK(store_delete(store, "a", NULL) == STORE_OK);
    CHECK(store_delete_by_prefix(store, "a", &deleted_count) == STORE_OK);
    CHECK(deleted_count == 0);
    CHECK(store_remove_expired(store, 10) == 0);
    store_get_stats(store, &stats);
    CHECK(stats.size == 0);
    store_destroy(store);
}

static void test_utf8_and_symbol_keys(void) {
    store_t *store = create_test_store();
    const char *keys[] = {"한글:키", "emoji:😀", "a/b?c%d", "공백 있는 키", "user:42:profile"};
    size_t key_count = sizeof(keys) / sizeof(keys[0]);

    for (size_t i = 0; i < key_count; i++) {
        CHECK(store_set(store, keys[i], keys[i], STORE_NO_TTL, NULL) == STORE_OK);
    }
    for (size_t i = 0; i < key_count; i++) {
        CHECK(has_value(store, keys[i], keys[i]));
    }
    store_destroy(store);
}

int main(void) {
    test_create_and_destroy();
    test_set_and_get();
    test_empty_value_is_not_missing();
    test_set_copies_input();
    test_delete();
    test_exists();
    test_invalid_key();
    test_invalid_value();
    test_ttl_expiry_boundary();
    test_set_without_ttl_clears_expiry();
    test_set_replaces_ttl();
    test_expire_and_ttl();
    test_invalid_ttl_keeps_old_state();
    test_set_on_expired_key_is_created();
    test_delete_expired_key_is_false();
    test_resave_before_sweep_keeps_new_value();
    test_delete_by_prefix();
    test_remove_expired_with_limit();
    test_many_keys_after_rehash();
    test_size_and_stats();
    test_empty_store_operations();
    test_utf8_and_symbol_keys();

    if (failed_count > 0) {
        printf("test_store : %d개 실패\n", failed_count);
        return 1;
    }
    printf("test_store : 모두 통과\n");
    return 0;
}
