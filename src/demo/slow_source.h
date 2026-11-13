#ifndef SLOW_SOURCE_H
#define SLOW_SOURCE_H

#include <stdbool.h>
#include <stdint.h>

/* 느린 원본(DB나 외부 API)을 흉내낸다. (협약 13.1) 여러 스레드에서 불러도 된다 */

#define SLOW_SOURCE_DELAY_MS 300

/*
 * SLOW_SOURCE_DELAY_MS만큼 기다린 뒤 상품 JSON을 돌려준다. 예 : {"id": "42", "name": "상품 42"}
 * 부를 때마다 원본 조회 횟수를 1 올린다.
 * 해제 책임 : 호출한 쪽이 free(). 메모리가 부족하면 NULL (원본 실패)
 */
char *slow_source_get_product(const char *product_id);

/* 원본의 상품 이름을 바꾼다. 메모리가 부족하면 false */
bool slow_source_update_product(const char *product_id, const char *name);

/* 원본을 부른 횟수 */
uint64_t slow_source_get_origin_call_count(void);

#endif
