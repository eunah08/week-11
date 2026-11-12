#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 시간의 의미는 store만 해석한다. 문자열과 노드는 테이블이 소유한다. */
typedef struct hash_entry {
    char *key;
    char *value;
    uint64_t expires_at_ms;
    struct hash_entry *next;
} hash_entry_t;
typedef struct hash_table hash_table_t;
typedef bool (*hash_table_predicate_fn)(const hash_entry_t *entry, void *context);

/* O(capacity). 실패 NULL. 호출자는 destroy로 전체 메모리를 해제한다. */
hash_table_t *hash_table_create(size_t initial_capacity);
/* O(capacity+n). 모든 키·값·노드와 테이블을 해제한다. NULL 허용. */
void hash_table_destroy(hash_table_t *hash_table);
size_t hash_table_size(const hash_table_t *hash_table);
size_t hash_table_capacity(const hash_table_t *hash_table);
/* 평균 O(1), 최악 O(n). 빌린 포인터이며 다음 변경까지만 유효하다. */
hash_entry_t *hash_table_find_entry(hash_table_t *hash_table, const char *key);
/* 입력을 복사한다. 평균 분할상환 O(1), 확장 O(n). 실패 시 기존 상태 보존. */
bool hash_table_put(hash_table_t *hash_table, const char *key, const char *value,
                    uint64_t expires_at_ms);
/* 평균 O(1), 최악 O(n). 해당 키의 메모리를 해제한다. */
bool hash_table_remove_entry(hash_table_t *hash_table, const char *key);
/* O(capacity+n). predicate가 참인 노드를 최대 max_count개 해제한다. */
size_t hash_table_visit(hash_table_t *hash_table, hash_table_predicate_fn predicate,
                         void *context, size_t max_count);

#endif
