#ifndef STORE_H
#define STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STORE_NO_TTL 0                  /* store_set에서 "만료 없음" */
#define STORE_TTL_NO_EXPIRY (-1)        /* store_ttl 결과: 만료 없음 */
#define MAX_KEY_BYTES 256
#define MAX_VALUE_BYTES 1048576
#define MAX_TTL_MS 2147483647

typedef enum {
    STORE_OK = 0,
    STORE_NOT_FOUND,             /* 없거나 만료됨. 에러가 아니다 */
    STORE_ERR_INVALID_KEY,       /* NULL, 빈 키, 256바이트 초과 */
    STORE_ERR_INVALID_VALUE,     /* NULL 값 */
    STORE_ERR_VALUE_TOO_LARGE,   /* 1MiB 초과 */
    STORE_ERR_INVALID_TTL,       /* 1~MAX_TTL_MS 밖 */
    STORE_ERR_INVALID_PREFIX,    /* NULL 또는 빈 접두사 */
    STORE_ERR_NO_MEMORY
} store_status_t;

typedef uint64_t (*store_clock_fn)(void);   /* 단조 시계, 밀리초 */

typedef struct {
    size_t initial_capacity;     /* 0이면 구현 기본값 */
    store_clock_fn now_ms;       /* NULL이면 clock.c의 기본 단조 시계 */
} store_options_t;

typedef struct {
    size_t size;
    size_t capacity;
    uint64_t hit_count;
    uint64_t miss_count;
    uint64_t expired_count;
} store_stats_t;

typedef struct store store_t;   /* 내부 구조는 각자 store.c에서 정의 */

store_t *store_create(const store_options_t *options);
void store_destroy(store_t *store);

store_status_t store_set(store_t *store, const char *key, const char *value,
                         int64_t ttl_ms, bool *out_is_created);
store_status_t store_get(store_t *store, const char *key, char **out_value);
store_status_t store_delete(store_t *store, const char *key, bool *out_is_deleted);
store_status_t store_exists(store_t *store, const char *key, bool *out_exists);
store_status_t store_expire(store_t *store, const char *key, int64_t ttl_ms,
                            bool *out_is_applied);
store_status_t store_ttl(store_t *store, const char *key, int64_t *out_ttl_ms);
size_t store_size(store_t *store);
store_status_t store_delete_by_prefix(store_t *store, const char *prefix,
                                      size_t *out_deleted_count);
size_t store_remove_expired(store_t *store, size_t max_count);
void store_get_stats(store_t *store, store_stats_t *out_stats);

#endif
