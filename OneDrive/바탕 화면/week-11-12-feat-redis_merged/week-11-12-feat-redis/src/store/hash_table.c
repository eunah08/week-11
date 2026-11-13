#include "hash_table.h"

#include <stdlib.h>
#include <string.h>

#define DEFAULT_CAPACITY 16
#define GROWTH_FACTOR 2
/* 적재율 기준 3/4. 실수 비교 대신 size * 4 > capacity * 3 으로 계산한다 */
#define MAX_LOAD_FACTOR_NUMERATOR 3
#define MAX_LOAD_FACTOR_DENOMINATOR 4
#define FNV_OFFSET_BASIS 14695981039346656037ULL
#define FNV_PRIME 1099511628211ULL

/* 칸 하나에 매달린 연결 리스트의 노드. 해시값을 같이 둬서 rehash 때 다시 계산하지 않는다 */
typedef struct entry {
    char *key;
    uint64_t hash;
    void *item;
    struct entry *next;
} entry_t;

struct hash_table {
    entry_t **buckets;
    size_t size;
    size_t capacity;    /* 항상 2의 거듭제곱 */
    hash_table_item_destroy_fn destroy_item;
};

static size_t round_up_to_power_of_two(size_t requested) {
    size_t capacity = 1;

    while (capacity < requested && capacity <= SIZE_MAX / GROWTH_FACTOR) {
        capacity *= GROWTH_FACTOR;
    }
    return capacity;
}

/* capacity가 2의 거듭제곱이라 나머지 연산 대신 아래 비트만 남기면 된다 */
static size_t find_bucket_index(uint64_t hash, size_t capacity) {
    return (size_t)(hash & (uint64_t)(capacity - 1));
}

static char *copy_key(const char *key) {
    size_t key_len = strlen(key);
    char *copy = malloc(key_len + 1);

    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, key, key_len + 1);
    return copy;
}

static void destroy_entry(hash_table_t *hash_table, entry_t *entry) {
    if (hash_table->destroy_item != NULL) {
        hash_table->destroy_item(entry->item);
    }
    free(entry->key);
    entry->key = NULL;
    free(entry);
}

hash_table_t *hash_table_create(size_t initial_capacity, hash_table_item_destroy_fn destroy_item) {
    hash_table_t *hash_table = calloc(1, sizeof(*hash_table));

    if (hash_table == NULL) {
        return NULL;
    }
    if (initial_capacity == 0) {
        initial_capacity = DEFAULT_CAPACITY;
    }
    hash_table->capacity = round_up_to_power_of_two(initial_capacity);
    hash_table->buckets = calloc(hash_table->capacity, sizeof(*hash_table->buckets));
    if (hash_table->buckets == NULL) {
        free(hash_table);
        return NULL;
    }
    hash_table->destroy_item = destroy_item;
    return hash_table;
}

void hash_table_destroy(hash_table_t *hash_table) {
    if (hash_table == NULL) {
        return;
    }
    for (size_t i = 0; i < hash_table->capacity; i++) {
        entry_t *entry = hash_table->buckets[i];
        while (entry != NULL) {
            entry_t *next = entry->next;
            destroy_entry(hash_table, entry);
            entry = next;
        }
    }
    free(hash_table->buckets);
    hash_table->buckets = NULL;
    free(hash_table);
}

uint64_t hash_table_hash_key(const char *key) {
    uint64_t hash = FNV_OFFSET_BASIS;

    for (const unsigned char *byte = (const unsigned char *)key; *byte != '\0'; byte++) {
        hash ^= *byte;
        hash *= FNV_PRIME;
    }
    return hash;
}

size_t hash_table_size(const hash_table_t *hash_table) {
    return hash_table->size;
}

size_t hash_table_capacity(const hash_table_t *hash_table) {
    return hash_table->capacity;
}

/* 해시값을 먼저 비교해서, 해시가 다른 노드는 문자열 비교를 하지 않는다 */
static entry_t *find_entry(const hash_table_t *hash_table, const char *key, uint64_t hash) {
    entry_t *entry = hash_table->buckets[find_bucket_index(hash, hash_table->capacity)];

    while (entry != NULL) {
        if (entry->hash == hash && strcmp(entry->key, key) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

void *hash_table_find(const hash_table_t *hash_table, const char *key) {
    entry_t *entry = find_entry(hash_table, key, hash_table_hash_key(key));

    return entry == NULL ? NULL : entry->item;
}

static bool is_over_load_factor(const hash_table_t *hash_table) {
    return hash_table->size * MAX_LOAD_FACTOR_DENOMINATOR >
           hash_table->capacity * MAX_LOAD_FACTOR_NUMERATOR;
}

/*
 * 칸 수가 바뀌면 hash & (capacity - 1)의 결과도 바뀌므로 모든 노드를 새 칸에 다시 건다.
 * 노드는 새로 만들지 않고 포인터만 옮긴다. 새 칸 배열을 못 받으면 아무것도 바꾸지 않는다.
 */
static void grow_buckets(hash_table_t *hash_table) {
    size_t new_capacity = hash_table->capacity * GROWTH_FACTOR;
    entry_t **new_buckets = calloc(new_capacity, sizeof(*new_buckets));

    if (new_buckets == NULL) {
        return;
    }
    for (size_t i = 0; i < hash_table->capacity; i++) {
        entry_t *entry = hash_table->buckets[i];
        while (entry != NULL) {
            entry_t *next = entry->next;
            size_t new_index = find_bucket_index(entry->hash, new_capacity);
            entry->next = new_buckets[new_index];
            new_buckets[new_index] = entry;
            entry = next;
        }
    }
    free(hash_table->buckets);
    hash_table->buckets = new_buckets;
    hash_table->capacity = new_capacity;
}

bool hash_table_insert(hash_table_t *hash_table, const char *key, void *item) {
    entry_t *entry = malloc(sizeof(*entry));

    if (entry == NULL) {
        return false;
    }
    entry->key = copy_key(key);
    if (entry->key == NULL) {
        free(entry);
        return false;
    }
    entry->hash = hash_table_hash_key(key);
    entry->item = item;

    size_t index = find_bucket_index(entry->hash, hash_table->capacity);
    entry->next = hash_table->buckets[index];
    hash_table->buckets[index] = entry;
    hash_table->size++;

    if (is_over_load_factor(hash_table) && hash_table->capacity <= SIZE_MAX / GROWTH_FACTOR) {
        grow_buckets(hash_table);
    }
    return true;
}

/* link는 지울 노드를 가리키는 포인터의 주소다. 맨 앞이든 중간이든 같은 방식으로 떼어 낸다 */
static void unlink_entry(hash_table_t *hash_table, entry_t **link) {
    entry_t *entry = *link;

    *link = entry->next;
    destroy_entry(hash_table, entry);
    hash_table->size--;
}

bool hash_table_remove(hash_table_t *hash_table, const char *key) {
    uint64_t hash = hash_table_hash_key(key);
    entry_t **link = &hash_table->buckets[find_bucket_index(hash, hash_table->capacity)];

    while (*link != NULL) {
        if ((*link)->hash == hash && strcmp((*link)->key, key) == 0) {
            unlink_entry(hash_table, link);
            return true;
        }
        link = &(*link)->next;
    }
    return false;
}

/* 칸 하나에서 조건에 맞는 노드를 최대 remaining_count개 지우고 지운 개수를 돌려준다 */
static size_t remove_matching_in_bucket(hash_table_t *hash_table, size_t bucket_index,
                                        const hash_table_remove_options_t *options,
                                        size_t remaining_count) {
    entry_t **link = &hash_table->buckets[bucket_index];
    size_t removed_count = 0;

    while (*link != NULL && removed_count < remaining_count) {
        if (options->is_match((*link)->key, (*link)->item, options->context)) {
            unlink_entry(hash_table, link);
            removed_count++;
        } else {
            link = &(*link)->next;
        }
    }
    return removed_count;
}

size_t hash_table_remove_matching(hash_table_t *hash_table,
                                  const hash_table_remove_options_t *options,
                                  size_t *out_next_bucket) {
    size_t removed_count = 0;
    size_t bucket_index = find_bucket_index(options->start_bucket, hash_table->capacity);

    for (size_t i = 0; i < hash_table->capacity; i++) {
        if (removed_count >= options->max_count) {
            break;
        }
        removed_count += remove_matching_in_bucket(hash_table, bucket_index, options,
                                                   options->max_count - removed_count);
        if (removed_count < options->max_count) {
            /* 상한에 걸려 멈춘 칸은 남은 노드가 있을 수 있으니 다음에 그 칸부터 다시 본다 */
            bucket_index = find_bucket_index(bucket_index + 1, hash_table->capacity);
        }
    }
    if (out_next_bucket != NULL) {
        *out_next_bucket = bucket_index;
    }
    return removed_count;
}
