#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * 해싱, 충돌 처리, 항목의 삽입·탐색·삭제, rehash를 맡는다. (협약 6장)
 * 시간·만료·네트워크를 모른다. store.h, server를 include하지 않는다.
 *
 * 해시 함수 : FNV-1a, 충돌 처리 : 체이닝, rehash : 적재율이 넘으면 칸을 두 배로 한 번에 늘림
 * 고른 이유는 docs/implementation_notes.md에 적는다. (협약 8.6)
 */

/*
 * 저장된 항목 하나. (협약 7.2)
 * key·value 메모리는 hash_table이 확보하고 해제한다.
 * expires_at_ms는 hash_table이 해석하지 않고 보관만 한다. 읽고 쓰는 쪽은 store다.
 */
typedef struct hash_table_entry {
    char *key;
    char *value;
    uint64_t expires_at_ms;
    struct hash_table_entry *next;   /* 같은 칸의 다음 항목 (체이닝) */
} hash_table_entry_t;

typedef struct hash_table hash_table_t;

/* 칸 initial_capacity개로 테이블을 만든다. 메모리가 부족하면 NULL. 해제는 hash_table_destroy */
hash_table_t *hash_table_create(size_t initial_capacity);

/* 테이블과 모든 항목의 메모리를 해제한다 */
void hash_table_destroy(hash_table_t *hash_table);

size_t hash_table_size(const hash_table_t *hash_table);
size_t hash_table_capacity(const hash_table_t *hash_table);

/* 키가 같은 항목을 찾는다. 없으면 NULL. 평균 O(1), 최악 O(n) */
hash_table_entry_t *hash_table_find_entry(const hash_table_t *hash_table, const char *key);

/*
 * 키와 값을 복사해 새 항목을 넣는다. 같은 키가 없다는 것은 호출한 쪽이 먼저 확인한다.
 * 새 항목의 expires_at_ms는 0이다.
 * 넣은 뒤 적재율이 기준을 넘으면 칸을 늘린다. 항목은 옮겨질 뿐이라 돌려준 포인터는 그대로 쓸 수 있다.
 * 메모리가 부족하면 아무것도 바꾸지 않고 NULL.
 * 해제 책임 : hash_table_remove_entry 또는 hash_table_destroy가 해제한다.
 * 평균 O(1), rehash가 일어나면 O(n)
 */
hash_table_entry_t *hash_table_insert_entry(hash_table_t *hash_table, const char *key,
                                            const char *value);

/*
 * 항목의 값을 복사본으로 바꾸고 기존 값은 해제한다.
 * 메모리가 부족하면 기존 값을 그대로 두고 false. O(값 길이)
 */
bool hash_table_update_value(hash_table_entry_t *entry, const char *value);

/* 키가 같은 항목을 빼내고 메모리를 해제한다. 지웠으면 true. 평균 O(1), 최악 O(n) */
bool hash_table_remove_entry(hash_table_t *hash_table, const char *key);

/*
 * bucket_index번 칸의 첫 항목. 비어 있거나 범위 밖이면 NULL.
 * 테이블 전체를 훑을 때 0 ~ capacity-1 칸을 차례로 넘겨 쓴다.
 */
hash_table_entry_t *hash_table_get_bucket(const hash_table_t *hash_table, size_t bucket_index);

#endif
