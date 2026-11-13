#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * 해싱, 충돌 처리, 항목의 삽입·탐색·삭제, rehash를 맡는다. (협약 6장)
 * 시간·만료·네트워크를 모른다. store.h, server를 include하지 않는다.
 *
 * 고른 방식 (협약 8.6, 자세한 이유는 docs/implementation_notes.md)
 * - 해시 함수 : FNV-1a 64비트
 * - 칸 번호 : 칸 수를 2의 거듭제곱으로 두고 hash & (capacity - 1)
 * - 충돌 처리 : 체이닝 (칸마다 단일 연결 리스트)
 * - rehash : 적재율이 3/4를 넘으면 칸을 2배로 늘려 한 번에 다시 배치. 줄이지는 않는다
 *
 * 테이블은 키 문자열과 "항목(item)" 포인터를 짝지어 보관한다. 항목의 내용(값, 만료 시각)은
 * 호출한 쪽(store)이 정하고, 테이블은 항목을 뺄 때 생성 시 받은 destroy_item으로 해제만 한다.
 */

typedef struct hash_table hash_table_t;

/* 항목 하나를 해제하는 함수. 항목이 테이블에서 빠질 때 테이블이 부른다 */
typedef void (*hash_table_item_destroy_fn)(void *item);

/* hash_table_remove_matching에서 항목을 지울지 정하는 함수. 참이면 지운다 */
typedef bool (*hash_table_match_fn)(const char *key, void *item, void *context);

typedef struct {
    hash_table_match_fn is_match;
    void *context;          /* is_match에 그대로 넘긴다 */
    size_t max_count;       /* 이만큼 지우면 멈춘다. 상한이 없으면 SIZE_MAX */
    size_t start_bucket;    /* 이 칸부터 한 바퀴 돈다. capacity 이상이면 나머지로 바꾼다 */
} hash_table_remove_options_t;

/*
 * 칸을 initial_capacity개 이상(2의 거듭제곱으로 올림)으로 테이블을 만든다. 0이면 기본값 16.
 * 반환 : 새 테이블. 메모리가 부족하면 NULL
 * 메모리 : 호출한 쪽이 hash_table_destroy로 해제한다
 * 시간 복잡도 : O(capacity)
 */
hash_table_t *hash_table_create(size_t initial_capacity, hash_table_item_destroy_fn destroy_item);

/* 모든 항목(destroy_item 호출)과 키, 칸 배열, 테이블을 해제한다. NULL이면 아무것도 안 한다. O(n) */
void hash_table_destroy(hash_table_t *hash_table);

/* 키를 FNV-1a 64비트로 해시한다. 테스트에서 같은 칸에 들어가는 키를 찾을 때도 쓴다. O(키 길이) */
uint64_t hash_table_hash_key(const char *key);

/* 저장된 항목 수. O(1) */
size_t hash_table_size(const hash_table_t *hash_table);

/* 칸 개수. O(1) */
size_t hash_table_capacity(const hash_table_t *hash_table);

/*
 * 키에 연결된 항목을 찾는다. 상태를 바꾸지 않는다.
 * 반환 : 항목 포인터 (테이블이 계속 소유한다), 없으면 NULL
 * 시간 복잡도 : 평균 O(1), 최악(모든 키가 한 칸) O(n)
 */
void *hash_table_find(const hash_table_t *hash_table, const char *key);

/*
 * 아직 없는 키를 새로 넣는다. 키는 복사하고, item의 소유권은 성공했을 때만 테이블로 넘어간다.
 * 이미 있는 키인지는 호출한 쪽이 hash_table_find로 먼저 확인한다 (같은 키를 두 번 넣지 않는다).
 * 넣은 뒤 적재율이 3/4를 넘으면 칸을 2배로 늘린다. 늘릴 메모리가 없으면 그대로 둔다(실패 아님).
 * 반환 : 성공하면 참. 메모리가 부족하면 거짓이고 테이블은 바뀌지 않는다 (item은 호출한 쪽이 해제)
 * 시간 복잡도 : 평균 O(1), rehash가 일어나면 O(n)
 */
bool hash_table_insert(hash_table_t *hash_table, const char *key, void *item);

/*
 * 키를 지우고 항목을 destroy_item으로 해제한다.
 * 반환 : 지웠으면 참, 없었으면 거짓
 * 시간 복잡도 : 평균 O(1), 최악 O(n)
 */
bool hash_table_remove(hash_table_t *hash_table, const char *key);

/*
 * start_bucket부터 칸을 한 바퀴 돌며 is_match가 참인 항목을 최대 max_count개 지운다.
 * 반환 : 지운 개수
 * out_next_bucket : 다음에 이어서 볼 칸 번호 (NULL이면 받지 않는다)
 * 시간 복잡도 : O(capacity + n)
 */
size_t hash_table_remove_matching(hash_table_t *hash_table,
                                  const hash_table_remove_options_t *options,
                                  size_t *out_next_bucket);

#endif
