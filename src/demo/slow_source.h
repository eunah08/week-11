#ifndef SLOW_SOURCE_H
#define SLOW_SOURCE_H
#include <stdint.h>
#define SLOW_SOURCE_DELAY_MS 300

typedef struct slow_source slow_source_t;
typedef enum { SOURCE_OK, SOURCE_FAILED, SOURCE_NO_MEMORY } source_status_t;
/* O(1), 실패 NULL. 호출자가 destroy. delay_ms는 원본 지연 재현용. */
slow_source_t *slow_source_create(unsigned delay_ms);
/* 모든 원본 수정 항목 해제. 진행 중 요청 종료 후 호출한다. */
void slow_source_destroy(slow_source_t *source);
/* 지연 후 원본 조회. 값 복사본은 호출자 free. 실패 응답은 값 NULL. O(n). */
source_status_t slow_source_get_product(slow_source_t *source, const char *id, char **out_value);
/* O(n), 입력 복사. 할당 실패 시 기존 값 유지. */
source_status_t slow_source_update_product(slow_source_t *source, const char *id,
                                            const char *value);
/* 조회 시도 수, 원본 실패 포함. O(1), 상태 변경 없음. */
uint64_t slow_source_get_origin_call_count(slow_source_t *source);
#endif
