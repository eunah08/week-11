#include "store.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static long fail_after = -1;
static long live_count;
static uint64_t fake_now_ms(void) { return 100; }
static bool has_budget(void) {
    if (fail_after == 0) { return false; }
    if (fail_after > 0) { fail_after--; }
    return true;
}
void *test_malloc(size_t size) {
    if (!has_budget()) { return NULL; }
    void *pointer = malloc(size);
    if (pointer != NULL) { live_count++; }
    return pointer;
}
void *test_calloc(size_t count, size_t size) {
    if (!has_budget()) { return NULL; }
    void *pointer = calloc(count, size);
    if (pointer != NULL) { live_count++; }
    return pointer;
}
char *test_strdup(const char *value) {
    char *copy = test_malloc(strlen(value) + 1);
    if (copy != NULL) { strcpy(copy, value); }
    return copy;
}
void test_free(void *pointer) {
    if (pointer != NULL) { live_count--; }
    free(pointer);
}

static void check_saved(store_t *store) {
    char *value = NULL;
    assert(store_get(store, "a", &value) == STORE_OK && strcmp(value, "old") == 0);
    test_free(value);
    int64_t ttl;
    assert(store_ttl(store, "a", &ttl) == STORE_OK && ttl == 50);
}

static void test_operation_failures(long failure_index) {
    store_options_t options = {4, fake_now_ms};
    fail_after = -1;
    store_t *store = store_create(&options);
    assert(store != NULL);
    assert(store_set(store, "a", "old", 50, NULL) == STORE_OK);
    assert(store_set(store, "b", "b", 0, NULL) == STORE_OK);
    assert(store_set(store, "c", "c", 0, NULL) == STORE_OK);
    fail_after = failure_index;
    bool created = false;
    assert(store_set(store, "d", "d", 0, &created) == STORE_ERR_NO_MEMORY);
    assert(!created);
    fail_after = -1;
    store_stats_t stats; store_get_stats(store, &stats);
    assert(stats.size == 3 && stats.capacity == 4);
    check_saved(store);
    fail_after = 0;
    assert(store_set(store, "a", "bad", 0, NULL) == STORE_ERR_NO_MEMORY);
    char *value = (char *)"sentinel";
    assert(store_get(store, "a", &value) == STORE_ERR_NO_MEMORY && value == NULL);
    fail_after = -1;
    check_saved(store);
    store_destroy(store);
    assert(live_count == 0);
}

int main(void) {
    store_options_t options = {4, fake_now_ms};
    for (long i = 0; i < 3; i++) {
        fail_after = i;
        assert(store_create(&options) == NULL);
        assert(live_count == 0);
    }
    for (long i = 0; i < 4; i++) { test_operation_failures(i); }
    puts("test_allocation: create/insert/resize/overwrite/get failure, no leaks OK");
    return 0;
}
