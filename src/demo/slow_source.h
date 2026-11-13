#ifndef SLOW_SOURCE_H
#define SLOW_SOURCE_H

#include <stdint.h>

/*
 * 느린 원본(DB나 외부 API)을 흉내낸다. (협약 13.1)
 * 상품 id 1~SLOW_SOURCE_MAX_PRODUCT_ID는 모두 있다고 보고, 처음 값은 id로 만든다.
 * PUT으로 고친 상품만 따로 기억한다. 여러 워커 스레드에서 동시에 불러도 된다.
 */

#define SLOW_SOURCE_DELAY_MS 300
#define SLOW_SOURCE_MIN_PRODUCT_ID 1
#define SLOW_SOURCE_MAX_PRODUCT_ID 1000000

typedef enum {
    SLOW_SOURCE_OK = 0,
    SLOW_SOURCE_NOT_FOUND,     /* id가 범위 밖. 이 실패 응답은 캐시하지 않는다 */
    SLOW_SOURCE_NO_MEMORY
} slow_source_status_t;

/*
 * SLOW_SOURCE_DELAY_MS만큼 기다린 뒤 상품 JSON을 돌려준다. 부를 때마다 원본 조회 횟수를 1 올린다.
 * 예 : {"id":42,"name":"product-42","price":4200,"version":1}
 * 메모리 : 성공하면 *out_product_json은 호출한 쪽이 free()
 */
slow_source_status_t slow_source_get_product(int64_t id, char **out_product_json);

/*
 * 상품의 이름과 가격을 바꾸고 version을 1 올린다. 원본 쓰기라서 같은 시간만큼 기다린다.
 * 원본 조회 횟수는 올리지 않는다 (조회가 아니라 쓰기다).
 * 메모리 : 성공하면 *out_product_json(바뀐 상품)은 호출한 쪽이 free()
 */
slow_source_status_t slow_source_update_product(int64_t id, const char *name, int64_t price,
                                                char **out_product_json);

/* 원본을 부른 횟수 (slow_source_get_product 호출 수) */
uint64_t slow_source_get_origin_call_count(void);

/* 고친 상품 목록의 메모리를 해제한다. 프로그램 종료 때 부른다 */
void slow_source_destroy(void);

#endif
