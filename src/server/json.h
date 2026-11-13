#ifndef JSON_H
#define JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * 요청 본문 JSON 읽기와 응답 JSON 쓰기. 외부 라이브러리 없이 직접 구현한다. (협약 2.2, 11.4)
 * store를 모른다. 샘플 API(demo)와 클라이언트도 같이 쓴다.
 *
 * 읽기는 "최상위 객체 하나, 필드 값은 무엇이든"을 받는다. 필드 이름은 이스케이프를 푼 뒤의
 * 바이트로 보관하므로 "ttlMs"와 "ttlMs"는 같은 이름이 된다 (중복 판정, 협약 11.4).
 */

#define JSON_MAX_DEPTH 32    /* 중첩된 배열·객체를 이만큼까지만 따라간다 (스택 보호) */
/* 최상위 필드 수 상한. 중복 확인이 O(필드 수²)라서 큰 본문 하나로 CPU를 붙잡지 못하게 한다 */
#define JSON_MAX_FIELDS 64

typedef enum {
    JSON_OK = 0,
    JSON_ERR_SYNTAX,         /* JSON이 아니다 (빈 본문, 잘못된 UTF-8 포함) → INVALID_JSON */
    JSON_ERR_NOT_OBJECT,     /* JSON이지만 최상위가 객체가 아니다 → INVALID_BODY */
    JSON_ERR_DUPLICATE,      /* 같은 필드가 두 번 나온다 → INVALID_BODY */
    JSON_ERR_TOO_MANY_FIELDS,/* 최상위 필드가 JSON_MAX_FIELDS개를 넘는다 → INVALID_BODY */
    JSON_ERR_NO_MEMORY
} json_status_t;

typedef enum {
    JSON_TYPE_STRING,
    JSON_TYPE_NUMBER,
    JSON_TYPE_OTHER          /* true, false, null, 배열, 객체 */
} json_value_type_t;

typedef struct {
    char *name;              /* 이스케이프를 푼 이름. 끝에 '\0'이 있다 */
    size_t name_len;
    json_value_type_t type;
    char *text;              /* 문자열이면 푼 값, 숫자면 원문. OTHER면 NULL */
    size_t text_len;         /* 문자열 중간에 \u0000이 있을 수 있어서 길이를 따로 둔다 */
} json_field_t;

typedef struct {
    json_field_t *fields;
    size_t field_count;
} json_object_t;

/*
 * text_len 바이트를 최상위 JSON 객체로 읽는다.
 * 반환 : JSON_OK면 *out_object를 채운다. 실패하면 *out_object는 빈 상태
 * 메모리 : 성공하면 호출한 쪽이 json_object_destroy로 해제한다
 * 시간 복잡도 : O(text_len + 필드 수²) (중복 확인)
 */
json_status_t json_parse_object(const char *text, size_t text_len, json_object_t *out_object);

/* 필드와 이름·값 메모리를 모두 해제한다. 해제 후 빈 상태가 된다 */
void json_object_destroy(json_object_t *object);

/* 이름이 name인 필드를 찾는다. 없으면 NULL. 반환한 필드는 object가 계속 소유한다 */
const json_field_t *json_object_find(const json_object_t *object, const char *name);

/* 숫자 필드가 정수 값을 가지면 참이고 *out_value에 넣는다. 1.0은 참, 1.5·1e400은 거짓 */
bool json_field_to_int64(const json_field_t *field, int64_t *out_value);

/* 응답 JSON을 만드는 가변 버퍼. 할당에 실패하면 has_error가 참이 되고 이후 추가는 무시한다 */
typedef struct {
    char *data;
    size_t len;
    size_t capacity;
    bool has_error;
} json_buffer_t;

void json_buffer_append(json_buffer_t *buffer, const char *text, size_t text_len);
void json_buffer_append_text(json_buffer_t *buffer, const char *text);

/* 따옴표로 감싸고 ", \, 제어 문자를 이스케이프해서 붙인다 */
void json_buffer_append_string(json_buffer_t *buffer, const char *text, size_t text_len);
void json_buffer_append_int(json_buffer_t *buffer, int64_t value);
void json_buffer_append_uint(json_buffer_t *buffer, uint64_t value);

/* 버퍼의 메모리를 해제한다 */
void json_buffer_destroy(json_buffer_t *buffer);

#endif
