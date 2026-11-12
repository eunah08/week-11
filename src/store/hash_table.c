#include "hash_table.h"

#include <stdlib.h>

#define DEFAULT_CAPACITY 16

struct hash_table {
    size_t size;
    size_t capacity;
    /* TODO(이니셜): 칸 배열(buckets)과 항목(entry) 구조를 정한다 */
};

hash_table_t *hash_table_create(size_t initial_capacity) {
    hash_table_t *hash_table = calloc(1, sizeof(*hash_table));
    if (hash_table == NULL) {
        return NULL;
    }
    if (initial_capacity == 0) {
        initial_capacity = DEFAULT_CAPACITY;
    }
    hash_table->capacity = initial_capacity;
    /* TODO(이니셜): 칸 배열을 확보한다. 실패하면 hash_table을 해제하고 NULL */
    return hash_table;
}

void hash_table_destroy(hash_table_t *hash_table) {
    if (hash_table == NULL) {
        return;
    }
    /* TODO(이니셜): 모든 항목과 칸 배열을 해제한다 */
    free(hash_table);
}

size_t hash_table_size(const hash_table_t *hash_table) {
    return hash_table->size;
}

size_t hash_table_capacity(const hash_table_t *hash_table) {
    return hash_table->capacity;
}
