#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash_table.h"

/*
 * 내 해시 테이블 구현 전용 테스트. (store.h 공통 테스트는 test_store.c)
 * 같은 칸에 들어가는 키를 직접 찾아 체이닝의 앞·중간·끝을 검증한다. (협약 15.1 충돌, 15.5)
 */

#define TEST_CAPACITY 16
#define COLLISION_KEY_COUNT 3
#define KEY_BUF_BYTES 32
#define MANY_KEY_COUNT 1000

static int failed_count = 0;
static int destroyed_count = 0;

#define CHECK(condition)                                                          \
    do {                                                                          \
        if (!(condition)) {                                                       \
            fprintf(stderr, "  실패 %s:%d %s\n", __FILE__, __LINE__, #condition); \
            failed_count++;                                                       \
        }                                                                         \
    } while (0)

static void count_destroyed_item(void *item) {
    destroyed_count++;
    free(item);
}

static char *copy_text(const char *text) {
    char *copy = malloc(strlen(text) + 1);

    if (copy != NULL) {
        strcpy(copy, text);
    }
    return copy;
}

static int is_found(hash_table_t *hash_table, const char *key) {
    char *item = hash_table_find(hash_table, key);

    return item != NULL && strcmp(item, key) == 0;
}

/* capacity 16 기준으로 "c:0"과 같은 칸에 들어가는 키를 COLLISION_KEY_COUNT개 찾는다 */
static void find_colliding_keys(char keys[][KEY_BUF_BYTES]) {
    size_t target_bucket = 0;
    int found_count = 0;

    for (int i = 0; found_count < COLLISION_KEY_COUNT; i++) {
        char key[KEY_BUF_BYTES];
        snprintf(key, sizeof(key), "c:%d", i);
        size_t bucket = (size_t)(hash_table_hash_key(key) & (TEST_CAPACITY - 1));
        if (found_count == 0) {
            target_bucket = bucket;
        }
        if (bucket == target_bucket) {
            strcpy(keys[found_count], key);
            found_count++;
        }
    }
}

static hash_table_t *create_colliding_table(char keys[][KEY_BUF_BYTES]) {
    hash_table_t *hash_table = hash_table_create(TEST_CAPACITY, count_destroyed_item);

    for (int i = 0; i < COLLISION_KEY_COUNT; i++) {
        hash_table_insert(hash_table, keys[i], copy_text(keys[i]));
    }
    return hash_table;
}

/* 같은 칸의 연결 리스트에서 removed_index번째 키를 지워도 나머지는 그대로 찾아야 한다 */
static void check_remove_in_chain(char keys[][KEY_BUF_BYTES], int removed_index) {
    hash_table_t *hash_table = create_colliding_table(keys);

    CHECK(hash_table_capacity(hash_table) == TEST_CAPACITY);
    CHECK(hash_table_remove(hash_table, keys[removed_index]));
    CHECK(!hash_table_remove(hash_table, keys[removed_index]));
    for (int i = 0; i < COLLISION_KEY_COUNT; i++) {
        CHECK(is_found(hash_table, keys[i]) == (i != removed_index));
    }
    CHECK(hash_table_size(hash_table) == COLLISION_KEY_COUNT - 1);
    hash_table_destroy(hash_table);
}

static void test_collision_chain(void) {
    char keys[COLLISION_KEY_COUNT][KEY_BUF_BYTES];

    find_colliding_keys(keys);
    for (int i = 0; i < COLLISION_KEY_COUNT; i++) {
        check_remove_in_chain(keys, i);    /* 앞, 중간, 끝 */
    }
}

static void test_capacity_rounding(void) {
    hash_table_t *hash_table = hash_table_create(10, count_destroyed_item);

    CHECK(hash_table_capacity(hash_table) == 16);
    hash_table_destroy(hash_table);
    hash_table = hash_table_create(0, count_destroyed_item);
    CHECK(hash_table_capacity(hash_table) == 16);
    hash_table_destroy(hash_table);
}

/* 적재율 3/4 경계 : 16칸에 12개까지는 그대로, 13개째에 32칸 */
static void test_rehash_boundary(void) {
    hash_table_t *hash_table = hash_table_create(TEST_CAPACITY, count_destroyed_item);
    char key[KEY_BUF_BYTES];

    for (int i = 0; i < 12; i++) {
        snprintf(key, sizeof(key), "r:%d", i);
        hash_table_insert(hash_table, key, copy_text(key));
    }
    CHECK(hash_table_capacity(hash_table) == 16);
    hash_table_insert(hash_table, "r:12", copy_text("r:12"));
    CHECK(hash_table_capacity(hash_table) == 32);
    for (int i = 0; i <= 12; i++) {
        snprintf(key, sizeof(key), "r:%d", i);
        CHECK(is_found(hash_table, key));
    }
    CHECK(hash_table_size(hash_table) == 13);
    hash_table_destroy(hash_table);
}

static bool is_even_key(const char *key, void *item, void *context) {
    (void)item;
    (void)context;
    return atoi(key + 2) % 2 == 0;
}

static void test_remove_matching(void) {
    hash_table_t *hash_table = hash_table_create(TEST_CAPACITY, count_destroyed_item);
    hash_table_remove_options_t options = {is_even_key, NULL, SIZE_MAX, 0};
    char key[KEY_BUF_BYTES];
    size_t next_bucket = 0;

    for (int i = 0; i < MANY_KEY_COUNT; i++) {
        snprintf(key, sizeof(key), "m:%d", i);
        hash_table_insert(hash_table, key, copy_text(key));
    }
    options.max_count = 100;
    CHECK(hash_table_remove_matching(hash_table, &options, &next_bucket) == 100);
    options.max_count = SIZE_MAX;
    options.start_bucket = next_bucket;
    CHECK(hash_table_remove_matching(hash_table, &options, NULL) == MANY_KEY_COUNT / 2 - 100);
    CHECK(hash_table_size(hash_table) == MANY_KEY_COUNT / 2);
    CHECK(is_found(hash_table, "m:1"));
    CHECK(!is_found(hash_table, "m:2"));
    hash_table_destroy(hash_table);
}

static void test_destroy_frees_every_item(void) {
    hash_table_t *hash_table = hash_table_create(TEST_CAPACITY, count_destroyed_item);
    char key[KEY_BUF_BYTES];

    destroyed_count = 0;
    for (int i = 0; i < 50; i++) {
        snprintf(key, sizeof(key), "d:%d", i);
        hash_table_insert(hash_table, key, copy_text(key));
    }
    hash_table_remove(hash_table, "d:0");
    hash_table_destroy(hash_table);
    CHECK(destroyed_count == 50);
    hash_table_destroy(NULL);
}

int main(void) {
    test_collision_chain();
    test_capacity_rounding();
    test_rehash_boundary();
    test_remove_matching();
    test_destroy_frees_every_item();

    if (failed_count > 0) {
        printf("test_hash_table : %d개 실패\n", failed_count);
        return 1;
    }
    printf("test_hash_table : 모두 통과\n");
    return 0;
}
