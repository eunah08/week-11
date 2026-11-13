#include "hash_table.h"

#include <stdlib.h>
#include <string.h>

#define DEFAULT_CAPACITY 16
#define MAX_LOAD_FACTOR 0.75
#define GROWTH_FACTOR 2
#define FNV_OFFSET_BASIS 14695981039346656037ULL
#define FNV_PRIME 1099511628211ULL

struct hash_table {
    hash_table_entry_t **buckets;   /* 칸 배열. 칸마다 항목 연결 리스트의 첫 항목을 가리킨다 */
    size_t size;
    size_t capacity;
};

/* FNV-1a : 글자 하나씩 섞고 소수를 곱해서, 비슷한 키도 서로 다른 숫자가 되게 흩뜨린다 */
static uint64_t hash_key(const char *key) {
    uint64_t hash = FNV_OFFSET_BASIS;

    for (size_t i = 0; key[i] != '\0'; i++) {
        hash ^= (unsigned char)key[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

static size_t find_bucket_index(const char *key, size_t capacity) {
    return (size_t)(hash_key(key) % capacity);
}

static void destroy_entry(hash_table_entry_t *entry) {
    free(entry->key);
    free(entry->value);
    free(entry);
}

/* 키와 값을 복사한 새 항목. 메모리가 부족하면 확보한 것을 모두 풀고 NULL */
static hash_table_entry_t *create_entry(const char *key, const char *value) {
    hash_table_entry_t *entry = calloc(1, sizeof(*entry));
    if (entry == NULL) {
        return NULL;
    }

    entry->key = strdup(key);
    entry->value = strdup(value);
    if (entry->key == NULL || entry->value == NULL) {
        destroy_entry(entry);
        return NULL;
    }
    return entry;
}

static bool is_over_load_factor(const hash_table_t *hash_table) {
    double load_factor = (double)hash_table->size / (double)hash_table->capacity;
    return load_factor > MAX_LOAD_FACTOR;
}

/*
 * 칸 수가 바뀌면 칸 번호(해시 % 칸 수)도 바뀌므로 모든 항목을 새 칸으로 옮긴다.
 * 메모리가 부족하면 기존 칸을 그대로 쓴다. 저장은 이미 끝났으므로 실패로 보지 않는다.
 */
static void grow_buckets(hash_table_t *hash_table) {
    size_t new_capacity = hash_table->capacity * GROWTH_FACTOR;
    hash_table_entry_t **new_buckets = calloc(new_capacity, sizeof(*new_buckets));
    if (new_buckets == NULL) {
        return;
    }

    for (size_t i = 0; i < hash_table->capacity; i++) {
        hash_table_entry_t *entry = hash_table->buckets[i];
        while (entry != NULL) {
            hash_table_entry_t *next_entry = entry->next;
            size_t new_index = find_bucket_index(entry->key, new_capacity);
            entry->next = new_buckets[new_index];
            new_buckets[new_index] = entry;
            entry = next_entry;
        }
    }
    free(hash_table->buckets);
    hash_table->buckets = new_buckets;
    hash_table->capacity = new_capacity;
}

hash_table_t *hash_table_create(size_t initial_capacity) {
    hash_table_t *hash_table = calloc(1, sizeof(*hash_table));
    if (hash_table == NULL) {
        return NULL;
    }
    if (initial_capacity == 0) {
        initial_capacity = DEFAULT_CAPACITY;
    }

    hash_table->buckets = calloc(initial_capacity, sizeof(*hash_table->buckets));
    if (hash_table->buckets == NULL) {
        free(hash_table);
        return NULL;
    }
    hash_table->capacity = initial_capacity;
    return hash_table;
}

void hash_table_destroy(hash_table_t *hash_table) {
    if (hash_table == NULL) {
        return;
    }

    for (size_t i = 0; i < hash_table->capacity; i++) {
        hash_table_entry_t *entry = hash_table->buckets[i];
        while (entry != NULL) {
            hash_table_entry_t *next_entry = entry->next;
            destroy_entry(entry);
            entry = next_entry;
        }
    }
    free(hash_table->buckets);
    hash_table->buckets = NULL;
    free(hash_table);
}

size_t hash_table_size(const hash_table_t *hash_table) {
    return hash_table->size;
}

size_t hash_table_capacity(const hash_table_t *hash_table) {
    return hash_table->capacity;
}

hash_table_entry_t *hash_table_find_entry(const hash_table_t *hash_table, const char *key) {
    size_t bucket_index = find_bucket_index(key, hash_table->capacity);
    hash_table_entry_t *entry = hash_table->buckets[bucket_index];

    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

hash_table_entry_t *hash_table_insert_entry(hash_table_t *hash_table, const char *key,
                                            const char *value) {
    hash_table_entry_t *entry = create_entry(key, value);
    if (entry == NULL) {
        return NULL;
    }

    /* 칸의 맨 앞에 붙인다. 끝까지 따라가지 않아도 되므로 O(1)이다 */
    size_t bucket_index = find_bucket_index(key, hash_table->capacity);
    entry->next = hash_table->buckets[bucket_index];
    hash_table->buckets[bucket_index] = entry;
    hash_table->size++;

    if (is_over_load_factor(hash_table)) {
        grow_buckets(hash_table);
    }
    return entry;
}

bool hash_table_update_value(hash_table_entry_t *entry, const char *value) {
    char *new_value = strdup(value);
    if (new_value == NULL) {
        return false;
    }

    free(entry->value);
    entry->value = new_value;
    return true;
}

bool hash_table_remove_entry(hash_table_t *hash_table, const char *key) {
    size_t bucket_index = find_bucket_index(key, hash_table->capacity);
    hash_table_entry_t *previous_entry = NULL;
    hash_table_entry_t *entry = hash_table->buckets[bucket_index];

    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            /* 앞 항목과 뒤 항목을 이어 붙여서 연결에서 빼낸다 */
            if (previous_entry == NULL) {
                hash_table->buckets[bucket_index] = entry->next;
            } else {
                previous_entry->next = entry->next;
            }
            destroy_entry(entry);
            hash_table->size--;
            return true;
        }
        previous_entry = entry;
        entry = entry->next;
    }
    return false;
}

hash_table_entry_t *hash_table_get_bucket(const hash_table_t *hash_table, size_t bucket_index) {
    if (bucket_index >= hash_table->capacity) {
        return NULL;
    }
    return hash_table->buckets[bucket_index];
}
