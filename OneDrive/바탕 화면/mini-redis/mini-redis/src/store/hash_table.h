#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stddef.h>

/*
 * 해싱, 충돌 처리, 항목의 삽입·탐색·삭제, rehash를 맡는다. (협약 6장)
 * 시간·만료·네트워크를 모른다. store.h, server를 include하지 않는다.
 *
 * 해시 함수, 충돌 처리 방식, rehash 방식은 자율이다. (협약 8.6)
 * 고른 방식과 이유는 docs/implementation_notes.md에 적는다.
 */

typedef struct hash_table hash_table_t;

/* 칸 initial_capacity개로 테이블을 만든다. 메모리가 부족하면 NULL. 해제는 hash_table_destroy */
hash_table_t *hash_table_create(size_t initial_capacity);

/* 테이블과 모든 항목의 메모리를 해제한다 */
void hash_table_destroy(hash_table_t *hash_table);

size_t hash_table_size(const hash_table_t *hash_table);
size_t hash_table_capacity(const hash_table_t *hash_table);

/* TODO(이니셜): 삽입·탐색·삭제·rehash 함수 시그니처를 정한다 */

#endif
