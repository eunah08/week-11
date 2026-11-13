#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "store.h"

/*
 * store.h 공개 함수만 쓴다. 다른 팀원 구현에도 그대로 돌릴 수 있어야 한다. (협약 15장)
 * TTL은 가짜 시계를 끼워서 기다리지 않고 검증한다.
 */

#define MANY_KEY_COUNT 100
#define SMALL_CAPACITY 4
#define KEY_BUF_BYTES 32

static uint64_t fake_now_ms = 0;
static int failed_count = 0;

static uint64_t get_fake_now_ms(void) {
    return fake_now_ms;
}

#define CHECK(condition)                                                    \
    do {                                                                    \
        if (!(condition)) {                                                 \
            fprintf(stderr, "  실패 %s:%d %s\n", __FILE__, __LINE__, #condition); \
            failed_count++;                                                 \
        }                                                                   \
    } while (0)

static store_t *create_test_store_with_capacity(size_t initial_capacity) {
    store_options_t options = {initial_capacity, get_fake_now_ms};
    fake_now_ms = 0;
    return store_create(&options);
}

static store_t *create_test_store(void) {
    return create_test_store_with_capacity(0);
}

static void make_key(char *key_buf, int number) {
    snprintf(key_buf, KEY_BUF_BYTES, "key:%d", number);
}

/* 키의 값이 expected_value와 같은지 확인한다 */
static bool has_value(store_t *store, const char *key, const char *expected_value) {
    char *value = NULL;
    store_status_t status = store_get(store, key, &value);
    bool is_same = status == STORE_OK && strcmp(value, expected_value) == 0;
    free(value);
    return is_same;
}

static bool is_missing(store_t *store, const char *key) {
    char *value = NULL;
    store_status_t status = store_get(store, key, &value);
    bool is_null = value == NULL;
    free(value);
    return status == STORE_NOT_FOUND && is_null;
}

/* length바이트짜리 문자열. 해제 책임 : 호출한 쪽이 free() */
static char *create_text(size_t length) {
    char *text = malloc(length + 1);
    if (text == NULL) {
        return NULL;
    }
    memset(text, 'a', length);
    text[length] = '\0';
    return text;
}

static void test_create_and_destroy(void) {
    store_t *store = create_test_store();
    CHECK(store != NULL);
    CHECK(store_size(store) == 0);
    store_destroy(store);
}

/* 저장 후 조회, 덮어쓰기, 없는 키, 빈 문자열 값 */
static void test_set_and_get(void) {
    store_t *store = create_test_store();
    bool is_created = false;

    CHECK(store_set(store, "a", "1", STORE_NO_TTL, &is_created) == STORE_OK);
    CHECK(is_created);
    CHECK(has_value(store, "a", "1"));

    CHECK(store_set(store, "a", "2", STORE_NO_TTL, &is_created) == STORE_OK);
    CHECK(!is_created);
    CHECK(has_value(store, "a", "2"));

    CHECK(is_missing(store, "nothing"));

    CHECK(store_set(store, "empty", "", STORE_NO_TTL, NULL) == STORE_OK);
    CHECK(has_value(store, "empty", ""));
    store_destroy(store);
}

/* 있는 키 삭제, 없는 키 삭제, 지운 키 다시 저장 */
static void test_delete(void) {
    store_t *store = create_test_store();
    bool is_deleted = false;
    bool is_created = false;

    store_set(store, "a", "1", STORE_NO_TTL, NULL);
    CHECK(store_delete(store, "a", &is_deleted) == STORE_OK);
    CHECK(is_deleted);
    CHECK(is_missing(store, "a"));

    CHECK(store_delete(store, "a", &is_deleted) == STORE_OK);
    CHECK(!is_deleted);

    CHECK(store_set(store, "a", "again", STORE_NO_TTL, &is_created) == STORE_OK);
    CHECK(is_created);
    CHECK(has_value(store, "a", "again"));
    store_destroy(store);
}

/*
 * 공개 함수만으로는 칸 번호를 알 수 없어서, 키를 많이 넣어 한 칸에 여러 항목이 생기게 한다.
 * 3개 중 1개를 지워서 연결의 앞·중간·끝 삭제가 모두 일어나게 하고, 나머지가 그대로인지 본다.
 */
static void test_collision(void) {
    store_t *store = create_test_store_with_capacity(SMALL_CAPACITY);
    char key[KEY_BUF_BYTES];

    for (int i = 0; i < MANY_KEY_COUNT; i++) {
        make_key(key, i);
        store_set(store, key, key, STORE_NO_TTL, NULL);
    }
    for (int i = 0; i < MANY_KEY_COUNT; i += 3) {
        make_key(key, i);
        store_delete(store, key, NULL);
    }
    for (int i = 0; i < MANY_KEY_COUNT; i++) {
        make_key(key, i);
        bool is_deleted_key = i % 3 == 0;
        CHECK(is_deleted_key ? is_missing(store, key) : has_value(store, key, key));
    }
    store_destroy(store);
}

/* 칸이 늘어난 뒤에도 모든 값이 그대로 조회된다 */
static void test_rehash(void) {
    store_t *store = create_test_store_with_capacity(SMALL_CAPACITY);
    store_stats_t stats;
    char key[KEY_BUF_BYTES];

    for (int i = 0; i < MANY_KEY_COUNT; i++) {
        make_key(key, i);
        store_set(store, key, key, STORE_NO_TTL, NULL);
    }
    store_get_stats(store, &stats);
    CHECK(stats.size == MANY_KEY_COUNT);
    CHECK(stats.capacity > SMALL_CAPACITY);

    for (int i = 0; i < MANY_KEY_COUNT; i++) {
        make_key(key, i);
        CHECK(has_value(store, key, key));
    }
    store_destroy(store);
}

/* 만료 직전 성공, 정각·이후 없음, ttl 값 */
static void test_ttl_expiry(void) {
    store_t *store = create_test_store();
    int64_t ttl_ms = 0;

    store_set(store, "a", "1", 1000, NULL);
    CHECK(store_ttl(store, "a", &ttl_ms) == STORE_OK);
    CHECK(ttl_ms == 1000);

    fake_now_ms = 999;
    CHECK(has_value(store, "a", "1"));
    CHECK(store_ttl(store, "a", &ttl_ms) == STORE_OK);
    CHECK(ttl_ms == 1);

    fake_now_ms = 1000;
    CHECK(is_missing(store, "a"));
    CHECK(store_ttl(store, "a", &ttl_ms) == STORE_NOT_FOUND);
    store_destroy(store);
}

/* ttl 없이 덮어쓰면 만료 해제, TTL 교체, 만료된 키에 저장하면 새 항목 */
static void test_ttl_overwrite(void) {
    store_t *store = create_test_store();
    int64_t ttl_ms = 0;
    bool is_created = false;

    store_set(store, "a", "1", 1000, NULL);
    store_set(store, "a", "2", STORE_NO_TTL, NULL);
    CHECK(store_ttl(store, "a", &ttl_ms) == STORE_OK);
    CHECK(ttl_ms == STORE_TTL_NO_EXPIRY);
    fake_now_ms = 5000;
    CHECK(has_value(store, "a", "2"));

    store_set(store, "b", "1", 1000, NULL);
    store_set(store, "b", "1", 3000, NULL);
    CHECK(store_ttl(store, "b", &ttl_ms) == STORE_OK);
    CHECK(ttl_ms == 3000);

    fake_now_ms = 8000;
    CHECK(store_set(store, "b", "new", STORE_NO_TTL, &is_created) == STORE_OK);
    CHECK(is_created);
    CHECK(has_value(store, "b", "new"));
    store_destroy(store);
}

/* store_expire : 걸기, 없는 키, 만료된 키 */
static void test_expire(void) {
    store_t *store = create_test_store();
    int64_t ttl_ms = 0;
    bool is_applied = false;

    store_set(store, "a", "1", STORE_NO_TTL, NULL);
    CHECK(store_expire(store, "a", 500, &is_applied) == STORE_OK);
    CHECK(is_applied);
    CHECK(store_ttl(store, "a", &ttl_ms) == STORE_OK);
    CHECK(ttl_ms == 500);

    CHECK(store_expire(store, "nothing", 500, &is_applied) == STORE_OK);
    CHECK(!is_applied);

    fake_now_ms = 500;
    CHECK(store_expire(store, "a", 500, &is_applied) == STORE_OK);
    CHECK(!is_applied);
    store_destroy(store);
}

/* 잘못된 TTL로 다시 저장해 거부되면 기존 값과 만료 기한이 그대로다 */
static void test_ttl_kept_on_invalid(void) {
    store_t *store = create_test_store();
    int64_t ttl_ms = 0;

    store_set(store, "a", "1", 1000, NULL);
    CHECK(store_set(store, "a", "2", -1, NULL) == STORE_ERR_INVALID_TTL);
    CHECK(store_set(store, "a", "2", (int64_t)MAX_TTL_MS + 1, NULL) == STORE_ERR_INVALID_TTL);
    CHECK(store_expire(store, "a", 0, NULL) == STORE_ERR_INVALID_TTL);
    CHECK(store_expire(store, "a", -1, NULL) == STORE_ERR_INVALID_TTL);

    CHECK(has_value(store, "a", "1"));
    CHECK(store_ttl(store, "a", &ttl_ms) == STORE_OK);
    CHECK(ttl_ms == 1000);
    store_destroy(store);
}

/* 만료된 키를 정리하기 전에 같은 키를 다시 저장해도 새 값이 유지된다 */
static void test_set_before_cleanup(void) {
    store_t *store = create_test_store();

    store_set(store, "a", "old", 100, NULL);
    fake_now_ms = 100;
    store_set(store, "a", "new", STORE_NO_TTL, NULL);
    CHECK(store_remove_expired(store, MANY_KEY_COUNT) == 0);
    CHECK(has_value(store, "a", "new"));
    store_destroy(store);
}

/* 키 길이 경계, NULL 값, 값 크기 경계 (협약 15.5) */
static void test_invalid_key_and_value(void) {
    store_t *store = create_test_store();
    char *key_256 = create_text(MAX_KEY_BYTES);
    char *key_257 = create_text(MAX_KEY_BYTES + 1);
    char *value_max = create_text(MAX_VALUE_BYTES);
    char *value_over = create_text(MAX_VALUE_BYTES + 1);

    CHECK(store_set(store, NULL, "1", STORE_NO_TTL, NULL) == STORE_ERR_INVALID_KEY);
    CHECK(store_set(store, "", "1", STORE_NO_TTL, NULL) == STORE_ERR_INVALID_KEY);
    CHECK(store_set(store, key_256, "1", STORE_NO_TTL, NULL) == STORE_OK);
    CHECK(store_set(store, key_257, "1", STORE_NO_TTL, NULL) == STORE_ERR_INVALID_KEY);

    CHECK(store_set(store, "a", NULL, STORE_NO_TTL, NULL) == STORE_ERR_INVALID_VALUE);
    CHECK(store_set(store, "a", value_max, STORE_NO_TTL, NULL) == STORE_OK);
    CHECK(store_set(store, "b", value_over, STORE_NO_TTL, NULL) == STORE_ERR_VALUE_TOO_LARGE);
    CHECK(is_missing(store, "b"));

    free(key_256);
    free(key_257);
    free(value_max);
    free(value_over);
    store_destroy(store);
}

/* 접두사가 같은 키만 지우고, 지운 개수에 만료 키는 넣지 않는다 */
static void test_delete_by_prefix(void) {
    store_t *store = create_test_store();
    size_t deleted_count = 0;

    store_set(store, "product:1", "1", STORE_NO_TTL, NULL);
    store_set(store, "product:2", "2", STORE_NO_TTL, NULL);
    store_set(store, "product:3", "3", 100, NULL);
    store_set(store, "user:1", "1", STORE_NO_TTL, NULL);
    fake_now_ms = 100;

    CHECK(store_delete_by_prefix(store, "product:", &deleted_count) == STORE_OK);
    CHECK(deleted_count == 2);
    CHECK(is_missing(store, "product:1"));
    CHECK(has_value(store, "user:1", "1"));

    CHECK(store_delete_by_prefix(store, "", NULL) == STORE_ERR_INVALID_PREFIX);
    CHECK(store_delete_by_prefix(store, NULL, NULL) == STORE_ERR_INVALID_PREFIX);
    store_destroy(store);
}

/* 읽히지 않은 만료 키를 정리하고, max_count 상한을 지킨다 */
static void test_remove_expired(void) {
    store_t *store = create_test_store();
    char key[KEY_BUF_BYTES];

    for (int i = 0; i < 5; i++) {
        make_key(key, i);
        store_set(store, key, "1", 100, NULL);
    }
    store_set(store, "alive", "1", STORE_NO_TTL, NULL);
    fake_now_ms = 100;

    CHECK(store_remove_expired(store, 2) == 2);
    CHECK(store_remove_expired(store, MANY_KEY_COUNT) == 3);
    CHECK(store_remove_expired(store, MANY_KEY_COUNT) == 0);
    CHECK(store_size(store) == 1);
    store_destroy(store);
}

/* 저장·삭제·만료 뒤의 size와 hit·miss·expired 통계 */
static void test_size_and_stats(void) {
    store_t *store = create_test_store();
    store_stats_t stats;

    store_set(store, "a", "1", STORE_NO_TTL, NULL);
    store_set(store, "b", "1", STORE_NO_TTL, NULL);
    store_set(store, "c", "1", 10, NULL);
    CHECK(store_size(store) == 3);

    CHECK(has_value(store, "a", "1"));
    CHECK(is_missing(store, "nothing"));
    store_delete(store, "b", NULL);
    fake_now_ms = 10;

    store_get_stats(store, &stats);
    CHECK(stats.size == 1);
    CHECK(stats.hit_count == 1);
    CHECK(stats.miss_count == 1);
    CHECK(stats.expired_count == 1);
    store_destroy(store);
}

/* 아무것도 없는 상태에서 조회·삭제·통계·접두사 삭제 (협약 15.5) */
static void test_empty_store(void) {
    store_t *store = create_test_store();
    store_stats_t stats;
    bool is_deleted = true;
    size_t deleted_count = 1;

    CHECK(is_missing(store, "a"));
    CHECK(store_delete(store, "a", &is_deleted) == STORE_OK);
    CHECK(!is_deleted);
    CHECK(store_delete_by_prefix(store, "a", &deleted_count) == STORE_OK);
    CHECK(deleted_count == 0);
    store_get_stats(store, &stats);
    CHECK(stats.size == 0);
    store_destroy(store);
}

int main(void) {
    test_create_and_destroy();
    test_set_and_get();
    test_delete();
    test_collision();
    test_rehash();
    test_ttl_expiry();
    test_ttl_overwrite();
    test_expire();
    test_ttl_kept_on_invalid();
    test_set_before_cleanup();
    test_invalid_key_and_value();
    test_delete_by_prefix();
    test_remove_expired();
    test_size_and_stats();
    test_empty_store();

    if (failed_count > 0) {
        printf("test_store : %d개 실패\n", failed_count);
        return 1;
    }
    printf("test_store : 모두 통과\n");
    return 0;
}
