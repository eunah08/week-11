#include "store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t fake_now_ms;
static int failed_count;
static int check_count;
static uint64_t get_fake_now_ms(void) { return fake_now_ms; }

#define CHECK(condition) do { \
    check_count++; \
    if (!(condition)) { \
        fprintf(stderr, "실패 %d: %s\n", __LINE__, #condition); failed_count++; \
    } \
} while (0)

static store_t *create_test_store(size_t capacity) {
    store_options_t options = {capacity, get_fake_now_ms};
    fake_now_ms = 1000;
    store_t *store = store_create(&options);
    CHECK(store != NULL);
    return store;
}

static void check_value(store_t *store, const char *key, const char *expected) {
    char *value = NULL;
    store_status_t status = store_get(store, key, &value);
    CHECK(status == (expected == NULL ? STORE_NOT_FOUND : STORE_OK));
    if (expected != NULL) {
        CHECK(value != NULL && strcmp(value, expected) == 0);
    } else {
        CHECK(value == NULL);
    }
    free(value);
}

static void test_crud_and_ownership(void) {
    store_t *store = create_test_store(0);
    bool changed = false;
    char key[] = "product:42", value[] = "old";
    CHECK(store_set(store, key, value, 0, &changed) == STORE_OK && changed);
    value[0] = 'X'; key[0] = 'X';
    check_value(store, "product:42", "old");
    char *copy = NULL;
    CHECK(store_get(store, "product:42", &copy) == STORE_OK);
    if (copy != NULL) { copy[0] = 'Y'; free(copy); }
    check_value(store, "product:42", "old");
    CHECK(store_set(store, "product:42", "", 0, &changed) == STORE_OK && !changed);
    check_value(store, "product:42", "");
    CHECK(store_exists(store, "product:42", &changed) == STORE_OK && changed);
    CHECK(store_delete(store, "product:42", &changed) == STORE_OK && changed);
    CHECK(store_delete(store, "product:42", &changed) == STORE_OK && !changed);
    check_value(store, "product:42", NULL);
    CHECK(store_set(store, "product:42", "again", 0, NULL) == STORE_OK);
    check_value(store, "product:42", "again");
    store_destroy(store);
}

static void test_collisions_and_resize(void) {
    store_t *store = create_test_store(16);
    /* FNV-1a의 초기 16칸에서 a, q, A는 모두 12번 칸이다. */
    const char *keys[] = {"a", "q", "A"};
    for (size_t i = 0; i < 3; i++) {
        CHECK(store_set(store, keys[i], keys[i], 0, NULL) == STORE_OK);
    }
    CHECK(store_delete(store, "q", NULL) == STORE_OK);
    check_value(store, "a", "a"); check_value(store, "A", "A");
    check_value(store, "q", NULL);
    CHECK(store_delete(store, "A", NULL) == STORE_OK);
    CHECK(store_delete(store, "a", NULL) == STORE_OK);
    store_stats_t before, after;
    store_get_stats(store, &before);
    for (size_t i = 0; i < 500; i++) {
        char key[32]; snprintf(key, sizeof(key), "item:%zu", i);
        CHECK(store_set(store, key, key, 0, NULL) == STORE_OK);
    }
    store_get_stats(store, &after);
    CHECK(after.size == 500 && after.capacity > before.capacity);
    for (size_t i = 0; i < 500; i++) {
        char key[32]; snprintf(key, sizeof(key), "item:%zu", i);
        check_value(store, key, key);
    }
    store_destroy(store);
}

static void test_ttl_boundaries(void) {
    store_t *store = create_test_store(0);
    bool changed = false;
    int64_t ttl = 0;
    CHECK(store_set(store, "a", "old", 10, NULL) == STORE_OK);
    fake_now_ms = 1009;
    CHECK(store_ttl(store, "a", &ttl) == STORE_OK && ttl == 1);
    check_value(store, "a", "old");
    fake_now_ms = 1010;
    check_value(store, "a", NULL);
    CHECK(store_expire(store, "a", 20, &changed) == STORE_OK && !changed);
    CHECK(store_set(store, "a", "new", 20, &changed) == STORE_OK && changed);
    CHECK(store_expire(store, "a", 30, &changed) == STORE_OK && changed);
    CHECK(store_ttl(store, "a", &ttl) == STORE_OK && ttl == 30);
    CHECK(store_set(store, "a", "forever", 0, &changed) == STORE_OK && !changed);
    fake_now_ms = 2000;
    CHECK(store_ttl(store, "a", &ttl) == STORE_OK && ttl == STORE_TTL_NO_EXPIRY);
    check_value(store, "a", "forever");
    CHECK(store_set(store, "a", "short", 1, NULL) == STORE_OK);
    fake_now_ms++;
    CHECK(store_set(store, "a", "renewed", 0, &changed) == STORE_OK && changed);
    CHECK(store_remove_expired(store, 200) == 0);
    check_value(store, "a", "renewed");
    store_destroy(store);
}

static void test_set_replaces_ttl(void) {
    store_t *store = create_test_store(0);
    bool created = true;
    CHECK(store_set(store, "a", "old", 10, NULL) == STORE_OK);
    fake_now_ms = 1005;
    CHECK(store_set(store, "a", "new", 30, &created) == STORE_OK && !created);
    fake_now_ms = 1010;
    check_value(store, "a", "new");
    int64_t ttl = 0;
    CHECK(store_ttl(store, "a", &ttl) == STORE_OK && ttl == 25);
    fake_now_ms = 1035;
    check_value(store, "a", NULL);
    store_destroy(store);
}

static void check_invalid_store_inputs(store_t *store, const char *large, const char *key) {
    CHECK(store_set(store, "saved", "bad", -1, NULL) == STORE_ERR_INVALID_TTL);
    CHECK(store_set(store, "saved", "bad", (int64_t)MAX_TTL_MS + 1, NULL)
          == STORE_ERR_INVALID_TTL);
    CHECK(store_set(store, "saved", NULL, 0, NULL) == STORE_ERR_INVALID_VALUE);
    CHECK(store_set(store, "saved", large, 0, NULL) == STORE_ERR_VALUE_TOO_LARGE);
    CHECK(store_expire(store, "saved", 0, NULL) == STORE_ERR_INVALID_TTL);
    CHECK(store_delete_by_prefix(store, "", NULL) == STORE_ERR_INVALID_PREFIX);
    CHECK(store_delete_by_prefix(store, NULL, NULL) == STORE_ERR_INVALID_PREFIX);
    CHECK(store_set(store, NULL, "v", 0, NULL) == STORE_ERR_INVALID_KEY);
    CHECK(store_set(store, "", "v", 0, NULL) == STORE_ERR_INVALID_KEY);
    CHECK(store_set(store, key, "v", 0, NULL) == STORE_ERR_INVALID_KEY);
    CHECK(store_set(store, "\xff", "v", 0, NULL) == STORE_ERR_INVALID_KEY);
}

static void test_invalid_preserves_value_and_ttl(void) {
    store_t *store = create_test_store(0);
    char key[258]; memset(key, 'a', 257); key[257] = '\0';
    char *large = malloc(MAX_VALUE_BYTES + 2);
    CHECK(large != NULL);
    if (large == NULL) { store_destroy(store); return; }
    memset(large, 'v', MAX_VALUE_BYTES + 1); large[MAX_VALUE_BYTES + 1] = '\0';
    CHECK(store_set(store, "saved", "old", 50, NULL) == STORE_OK);
    check_invalid_store_inputs(store, large, key);
    key[256] = '\0';
    CHECK(store_set(store, key, "v", 0, NULL) == STORE_OK);
    large[MAX_VALUE_BYTES] = '\0';
    CHECK(store_set(store, "large", large, 0, NULL) == STORE_OK);
    check_value(store, "large", large);
    int64_t ttl = 0;
    CHECK(store_ttl(store, "saved", &ttl) == STORE_OK && ttl == 50);
    check_value(store, "saved", "old");
    fake_now_ms = 1050;
    check_value(store, "saved", NULL);
    free(large); store_destroy(store);
}

static void test_expired_public_operations(void) {
    store_t *store = create_test_store(0);
    const char *keys[] = {"get", "delete", "exists", "expire", "ttl", "set"};
    for (size_t i = 0; i < 6; i++) {
        CHECK(store_set(store, keys[i], "v", 1, NULL) == STORE_OK);
    }
    fake_now_ms++;
    bool changed = true;
    check_value(store, "get", NULL);
    CHECK(store_delete(store, "delete", &changed) == STORE_OK && !changed);
    CHECK(store_exists(store, "exists", &changed) == STORE_OK && !changed);
    CHECK(store_expire(store, "expire", 5, &changed) == STORE_OK && !changed);
    CHECK(store_ttl(store, "ttl", NULL) == STORE_NOT_FOUND);
    CHECK(store_set(store, "set", "new", 0, &changed) == STORE_OK && changed);
    store_stats_t stats;
    store_get_stats(store, &stats);
    CHECK(stats.size == 1 && stats.expired_count == 6);
    store_destroy(store);
}

static void test_prefix_sweep_and_stats(void) {
    store_t *store = create_test_store(0);
    CHECK(store_set(store, "product:1", "a", 1, NULL) == STORE_OK);
    CHECK(store_set(store, "product:2", "b", 0, NULL) == STORE_OK);
    CHECK(store_set(store, "other", "c", 0, NULL) == STORE_OK);
    fake_now_ms++;
    size_t deleted_count = 99;
    CHECK(store_delete_by_prefix(store, "product:", &deleted_count) == STORE_OK);
    CHECK(deleted_count == 1 && store_size(store) == 1);
    check_value(store, "other", "c"); check_value(store, "missing", NULL);
    store_stats_t stats;
    store_get_stats(store, &stats);
    CHECK(stats.hit_count == 1 && stats.miss_count == 1 && stats.expired_count == 1);
    for (size_t i = 0; i < 5; i++) {
        char key[8]; snprintf(key, sizeof(key), "e%zu", i);
        CHECK(store_set(store, key, "v", 1, NULL) == STORE_OK);
    }
    fake_now_ms++;
    CHECK(store_remove_expired(store, 0) == 0);
    CHECK(store_remove_expired(store, 2) == 2);
    CHECK(store_remove_expired(store, 2) == 2);
    CHECK(store_remove_expired(store, 2) == 1);
    CHECK(store_size(store) == 1);
    CHECK(store_set(store, "unread", "v", 1, NULL) == STORE_OK);
    fake_now_ms++;
    store_get_stats(store, &stats);
    CHECK(stats.size == 1 && stats.expired_count == 7);
    store_destroy(store);
}

int main(void) {
    test_crud_and_ownership(); test_collisions_and_resize(); test_ttl_boundaries();
    test_invalid_preserves_value_and_ttl(); test_expired_public_operations();
    test_set_replaces_ttl();
    test_prefix_sweep_and_stats();
    printf("test_store: %d checks, %d failures\n", check_count, failed_count);
    return failed_count != 0;
}
