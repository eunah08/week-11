#ifndef JSON_H
#define JSON_H

#include <stdbool.h>
#include <stddef.h>

/*
 * 요청 본문 JSON 해석과 응답용 문자열 이스케이프를 맡는다. (협약 11.4)
 * 최상위 객체의 필드만 꺼내 보관하고, 안쪽 배열·객체는 문법만 확인한다.
 * 외부 JSON 라이브러리를 쓰지 않는다. 중복 필드를 이스케이프를 푼 이름으로 판정하기 위해서다.
 */

typedef enum {
    JSON_TYPE_STRING,
    JSON_TYPE_NUMBER,
    JSON_TYPE_OTHER          /* true, false, null, 배열, 객체 */
} json_type_t;

typedef struct {
    char *name;              /* 이스케이프를 푼 필드 이름 */
    size_t name_len;
    json_type_t type;
    char *string_value;      /* JSON_TYPE_STRING일 때만. 이스케이프를 푼 값 */
    size_t string_len;       /* \u0000이 들어 있으면 strlen 결과와 다르다 */
    double number_value;     /* JSON_TYPE_NUMBER일 때만 */
} json_field_t;

typedef struct {
    json_field_t *fields;
    size_t field_count;
    size_t field_capacity;
} json_object_t;

typedef enum {
    JSON_PARSE_OK = 0,
    JSON_PARSE_INVALID_JSON, /* JSON 문법 오류 */
    JSON_PARSE_NOT_OBJECT,   /* 문법은 맞지만 최상위가 객체가 아님 */
    JSON_PARSE_NO_MEMORY
} json_parse_status_t;

/*
 * text_len바이트를 최상위 JSON 객체로 해석한다. text는 NUL로 끝나지 않아도 된다.
 * 실패하면 *out_object를 비워 둔다.
 * 해제 책임 : 성공하면 호출한 쪽이 json_object_destroy로 해제한다. O(본문 길이)
 */
json_parse_status_t json_parse_object(const char *text, size_t text_len,
                                      json_object_t *out_object);

/* 객체 안의 모든 필드 메모리를 해제한다. 비어 있는 객체를 넘겨도 된다 */
void json_object_destroy(json_object_t *object);

/* 이름이 같은 첫 필드. 없으면 NULL */
const json_field_t *json_find_field(const json_object_t *object, const char *name);

/* 필드 이름이 name과 같은지. 이름 안의 \u0000까지 비교한다 */
bool json_is_field_named(const json_field_t *field, const char *name);

/* 이름이 같은 필드가 두 번 이상 있는지. O(필드 수²) */
bool json_has_duplicate_field(const json_object_t *object);

/*
 * 따옴표로 감싸고 이스케이프한 JSON 문자열을 만든다. 예 : a"b → "a\"b"
 * 해제 책임 : 호출한 쪽이 free(). 메모리가 부족하면 NULL. O(길이)
 */
char *json_escape_string(const char *text);

#endif
