#ifndef JSON_H
#define JSON_H

#include <stdbool.h>
#include <stddef.h>

#define JSON_MAX_FIELDS 16
#define JSON_MAX_DEPTH 32

typedef enum { JSON_STRING, JSON_NUMBER, JSON_OTHER } json_kind_t;
typedef struct {
    char *name;
    size_t name_len;
    char *string;
    size_t string_len;
    double number;
    bool is_integer;
    json_kind_t kind;
} json_field_t;
typedef struct {
    json_field_t fields[JSON_MAX_FIELDS];
    size_t count;
    bool is_object;
    bool has_duplicate;
    bool has_overflow;
} json_object_t;

/* O(len). 0 성공, -1 문법 오류, -2 메모리 부족. 결과는 destroy로 해제한다. */
int json_parse_object(const char *input, size_t len, json_object_t *out_object);
void json_object_destroy(json_object_t *object);
const json_field_t *json_find_field(const json_object_t *object, const char *name);
/* UTF-8 검증. NUL 포함 여부는 호출자가 길이로 판정한다. */
bool json_is_utf8(const char *input, size_t len);
/* JSON 문자열 할당. 실패 NULL, 호출자가 free, O(strlen). */
char *json_quote(const char *input);

#endif
