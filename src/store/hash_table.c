#include "hash_table.h"

#include <stdlib.h>
#include <string.h>

#define DEFAULT_CAPACITY 16
#define FNV_OFFSET UINT64_C(14695981039346656037)
#define FNV_PRIME UINT64_C(1099511628211)
#define LOAD_NUMERATOR 3
#define LOAD_DENOMINATOR 4
#define GROWTH_FACTOR 2

struct hash_table {
    size_t size;
    size_t capacity;
    hash_entry_t **buckets;
};

static size_t find_bucket_index(const char *key, size_t capacity) {
    uint64_t hash = FNV_OFFSET;
    for (const unsigned char *cursor = (const unsigned char *)key; *cursor; cursor++) {
        hash = (hash ^ *cursor) * FNV_PRIME;
    }
    return (size_t)(hash % capacity);
}

static void destroy_entry(hash_entry_t *entry) {
    free(entry->key);
    free(entry->value);
    free(entry);
}

hash_table_t *hash_table_create(size_t initial_capacity) {
    hash_table_t *hash_table = calloc(1, sizeof(*hash_table));
    if (hash_table == NULL) {
        return NULL;
    }
    hash_table->capacity = initial_capacity ? initial_capacity : DEFAULT_CAPACITY;
    hash_table->buckets = calloc(hash_table->capacity, sizeof(*hash_table->buckets));
    if (hash_table->buckets == NULL) {
        free(hash_table);
        return NULL;
    }
    return hash_table;
}

void hash_table_destroy(hash_table_t *hash_table) {
    if (hash_table == NULL) {
        return;
    }
    for (size_t i = 0; i < hash_table->capacity; i++) {
        hash_entry_t *entry = hash_table->buckets[i];
        while (entry != NULL) {
            hash_entry_t *next = entry->next;
            destroy_entry(entry);
            entry = next;
        }
    }
    free(hash_table->buckets);
    free(hash_table);
}

size_t hash_table_size(const hash_table_t *hash_table) {
    return hash_table->size;
}

size_t hash_table_capacity(const hash_table_t *hash_table) {
    return hash_table->capacity;
}

hash_entry_t *hash_table_find_entry(hash_table_t *hash_table, const char *key) {
    size_t index = find_bucket_index(key, hash_table->capacity);
    for (hash_entry_t *entry = hash_table->buckets[index]; entry; entry = entry->next) {
        if (strcmp(entry->key, key) == 0) {
            return entry;
        }
    }
    return NULL;
}

static bool hash_table_resize(hash_table_t *hash_table) {
    if (hash_table->capacity > SIZE_MAX / GROWTH_FACTOR / sizeof(hash_entry_t *)) {
        return false;
    }
    size_t capacity = hash_table->capacity * GROWTH_FACTOR;
    hash_entry_t **buckets = calloc(capacity, sizeof(*buckets));
    if (buckets == NULL) {
        return false;
    }
    for (size_t i = 0; i < hash_table->capacity; i++) {
        hash_entry_t *entry = hash_table->buckets[i];
        while (entry != NULL) {
            hash_entry_t *next = entry->next;
            size_t index = find_bucket_index(entry->key, capacity);
            entry->next = buckets[index];
            buckets[index] = entry;
            entry = next;
        }
    }
    free(hash_table->buckets);
    hash_table->buckets = buckets;
    hash_table->capacity = capacity;
    return true;
}

static hash_entry_t *create_entry(const char *key, const char *value, uint64_t expiry) {
    hash_entry_t *entry = calloc(1, sizeof(*entry));
    if (entry == NULL) {
        return NULL;
    }
    entry->key = strdup(key);
    entry->value = strdup(value);
    if (entry->key == NULL || entry->value == NULL) {
        destroy_entry(entry);
        return NULL;
    }
    entry->expires_at_ms = expiry;
    return entry;
}

bool hash_table_put(hash_table_t *hash_table, const char *key, const char *value,
                    uint64_t expires_at_ms) {
    hash_entry_t *entry = hash_table_find_entry(hash_table, key);
    if (entry != NULL) {
        char *copy = strdup(value);
        if (copy == NULL) {
            return false;
        }
        free(entry->value);
        entry->value = copy;
        entry->expires_at_ms = expires_at_ms;
        return true;
    }
    entry = create_entry(key, value, expires_at_ms);
    if (entry == NULL) {
        return false;
    }
    size_t threshold = hash_table->capacity / LOAD_DENOMINATOR * LOAD_NUMERATOR
                       + hash_table->capacity % LOAD_DENOMINATOR * LOAD_NUMERATOR
                         / LOAD_DENOMINATOR;
    if (hash_table->size + 1 > threshold && !hash_table_resize(hash_table)) {
        destroy_entry(entry);
        return false;
    }
    size_t index = find_bucket_index(key, hash_table->capacity);
    entry->next = hash_table->buckets[index];
    hash_table->buckets[index] = entry;
    hash_table->size++;
    return true;
}

bool hash_table_remove_entry(hash_table_t *hash_table, const char *key) {
    size_t index = find_bucket_index(key, hash_table->capacity);
    hash_entry_t **link = &hash_table->buckets[index];
    while (*link != NULL) {
        hash_entry_t *entry = *link;
        if (strcmp(entry->key, key) == 0) {
            *link = entry->next;
            destroy_entry(entry);
            hash_table->size--;
            return true;
        }
        link = &entry->next;
    }
    return false;
}

size_t hash_table_visit(hash_table_t *hash_table, hash_table_predicate_fn predicate,
                         void *context, size_t max_count) {
    size_t removed_count = 0;
    for (size_t i = 0; i < hash_table->capacity && removed_count < max_count; i++) {
        hash_entry_t **link = &hash_table->buckets[i];
        while (*link != NULL && removed_count < max_count) {
            hash_entry_t *entry = *link;
            if (predicate(entry, context)) {
                *link = entry->next;
                destroy_entry(entry);
                hash_table->size--;
                removed_count++;
            } else {
                link = &entry->next;
            }
        }
    }
    return removed_count;
}
