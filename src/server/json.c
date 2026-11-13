#include "json.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_JSON_DEPTH 64            /* 안쪽 배열·객체 깊이 상한. 재귀로 스택이 넘치지 않게 */
#define INITIAL_FIELD_CAPACITY 4
#define HEX_DIGIT_COUNT 4
#define HEX_BASE 16
#define HEX_LETTER_BASE 10
#define UTF8_MAX_BYTES 4
#define FIRST_PRINTABLE_CHAR 0x20    /* 이보다 작은 제어 문자는 이스케이프해야 한다 */
#define HIGH_SURROGATE_MIN 0xD800
#define HIGH_SURROGATE_MAX 0xDBFF
#define LOW_SURROGATE_MIN 0xDC00
#define LOW_SURROGATE_MAX 0xDFFF
#define SURROGATE_OFFSET 0x10000
#define SURROGATE_SHIFT 10
#define MAX_ESCAPED_BYTES_PER_CHAR 6 /* \u001f */
#define QUOTE_AND_NUL_BYTES 3

typedef struct {
    const char *text;
    size_t len;
    size_t pos;
    int depth;
    bool is_out_of_memory;   /* 문법 오류와 메모리 부족을 구분하려고 둔다 */
} json_parser_t;

static bool skip_value(json_parser_t *parser);

/* ---------- 글자 단위 도우미 ---------- */

static bool has_more(const json_parser_t *parser) {
    return parser->pos < parser->len;
}

static char peek_char(const json_parser_t *parser) {
    if (!has_more(parser)) {
        return '\0';
    }
    return parser->text[parser->pos];
}

/* 지금 글자가 expected면 넘기고 true */
static bool consume_char(json_parser_t *parser, char expected) {
    if (!has_more(parser) || parser->text[parser->pos] != expected) {
        return false;
    }
    parser->pos++;
    return true;
}

static bool is_digit(char character) {
    return character >= '0' && character <= '9';
}

static void skip_whitespace(json_parser_t *parser) {
    while (has_more(parser)) {
        char current = parser->text[parser->pos];
        if (current != ' ' && current != '\t' && current != '\n' && current != '\r') {
            return;
        }
        parser->pos++;
    }
}

static int hex_digit_value(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + HEX_LETTER_BASE;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + HEX_LETTER_BASE;
    }
    return -1;
}

/* ---------- 문자열 ---------- */

static bool read_hex4(json_parser_t *parser, uint32_t *out_code) {
    uint32_t code = 0;
    if (parser->len - parser->pos < HEX_DIGIT_COUNT) {
        return false;
    }
    for (int i = 0; i < HEX_DIGIT_COUNT; i++) {
        int digit = hex_digit_value(parser->text[parser->pos + i]);
        if (digit < 0) {
            return false;
        }
        code = code * HEX_BASE + (uint32_t)digit;
    }
    parser->pos += HEX_DIGIT_COUNT;
    *out_code = code;
    return true;
}

static bool is_high_surrogate(uint32_t code) {
    return code >= HIGH_SURROGATE_MIN && code <= HIGH_SURROGATE_MAX;
}

static bool is_low_surrogate(uint32_t code) {
    return code >= LOW_SURROGATE_MIN && code <= LOW_SURROGATE_MAX;
}

/*
 * \u 뒤의 16진수 4자리를 읽는다. 이모지처럼 큰 글자는 😀처럼 두 개가 한 쌍이라
 * 앞쪽(high)을 만나면 뒤쪽(low)까지 읽어 한 글자로 합친다.
 */
static bool read_unicode_escape(json_parser_t *parser, uint32_t *out_code) {
    uint32_t high = 0;
    uint32_t low = 0;
    if (!read_hex4(parser, &high) || is_low_surrogate(high)) {
        return false;
    }
    if (!is_high_surrogate(high)) {
        *out_code = high;
        return true;
    }
    if (!consume_char(parser, '\\') || !consume_char(parser, 'u')) {
        return false;
    }
    if (!read_hex4(parser, &low) || !is_low_surrogate(low)) {
        return false;
    }
    *out_code = SURROGATE_OFFSET + ((high - HIGH_SURROGATE_MIN) << SURROGATE_SHIFT)
                + (low - LOW_SURROGATE_MIN);
    return true;
}

/* 글자 번호를 UTF-8 바이트로 바꿔 out_text에 쓰고, 쓴 바이트 수를 돌려준다 (UTF-8 규격의 비트 배치) */
static size_t encode_utf8(uint32_t code, char *out_text) {
    if (code < 0x80) {
        out_text[0] = (char)code;
        return 1;
    }
    if (code < 0x800) {
        out_text[0] = (char)(0xC0 | (code >> 6));
        out_text[1] = (char)(0x80 | (code & 0x3F));
        return 2;
    }
    if (code < 0x10000) {
        out_text[0] = (char)(0xE0 | (code >> 12));
        out_text[1] = (char)(0x80 | ((code >> 6) & 0x3F));
        out_text[2] = (char)(0x80 | (code & 0x3F));
        return 3;
    }
    out_text[0] = (char)(0xF0 | (code >> 18));
    out_text[1] = (char)(0x80 | ((code >> 12) & 0x3F));
    out_text[2] = (char)(0x80 | ((code >> 6) & 0x3F));
    out_text[3] = (char)(0x80 | (code & 0x3F));
    return 4;
}

static bool read_escaped_char(char escape, char *out_char) {
    switch (escape) {
    case '"':
    case '\\':
    case '/':
        *out_char = escape;
        return true;
    case 'b':
        *out_char = '\b';
        return true;
    case 'f':
        *out_char = '\f';
        return true;
    case 'n':
        *out_char = '\n';
        return true;
    case 'r':
        *out_char = '\r';
        return true;
    case 't':
        *out_char = '\t';
        return true;
    }
    return false;
}

/*
 * 백슬래시 다음 글자부터 이스케이프 하나를 풀어 out_text[*out_len] 뒤에 붙인다.
 * out_text가 NULL이면 문법만 확인한다.
 */
static bool decode_escape(json_parser_t *parser, char *out_text, size_t *out_len) {
    char utf8_buf[UTF8_MAX_BYTES];
    size_t byte_count = 1;
    uint32_t code = 0;
    char escape = peek_char(parser);

    parser->pos++;
    if (escape == 'u') {
        if (!read_unicode_escape(parser, &code)) {
            return false;
        }
        byte_count = encode_utf8(code, utf8_buf);
    } else if (!read_escaped_char(escape, &utf8_buf[0])) {
        return false;
    }

    if (out_text != NULL) {
        memcpy(out_text + *out_len, utf8_buf, byte_count);
    }
    *out_len += byte_count;
    return true;
}

/* 여는 따옴표 다음부터 닫는 따옴표의 위치를 찾는다. 이스케이프된 \"는 건너뛴다 */
static bool find_string_end(const json_parser_t *parser, size_t *out_end) {
    size_t index = parser->pos;
    while (index < parser->len) {
        if (parser->text[index] == '"') {
            *out_end = index;
            return true;
        }
        index += (parser->text[index] == '\\') ? 2 : 1;
    }
    return false;
}

/* 닫는 따옴표(end) 앞까지 이스케이프를 풀며 out_text에 쓴다. out_text가 NULL이면 확인만 */
static bool decode_string_body(json_parser_t *parser, size_t end, char *out_text,
                               size_t *out_len) {
    while (parser->pos < end) {
        unsigned char current = (unsigned char)parser->text[parser->pos];
        if (current < FIRST_PRINTABLE_CHAR) {
            return false;
        }
        if (current == '\\') {
            parser->pos++;
            if (!decode_escape(parser, out_text, out_len)) {
                return false;
            }
            continue;
        }
        if (out_text != NULL) {
            out_text[*out_len] = (char)current;
        }
        (*out_len)++;
        parser->pos++;
    }
    return parser->pos == end;
}

/*
 * 따옴표 문자열 하나를 읽는다. out_text가 NULL이면 문법만 확인하고 버린다.
 * 푼 문자열은 원문보다 길어지지 않으므로 원문 길이만큼만 확보한다.
 * 해제 책임 : *out_text는 호출한 쪽이 free()
 */
static bool parse_string(json_parser_t *parser, char **out_text, size_t *out_len) {
    size_t end = 0;
    size_t text_len = 0;
    char *text = NULL;
    if (!consume_char(parser, '"') || !find_string_end(parser, &end)) {
        return false;
    }

    if (out_text != NULL) {
        text = malloc(end - parser->pos + 1);
        if (text == NULL) {
            parser->is_out_of_memory = true;
            return false;
        }
    }
    if (!decode_string_body(parser, end, text, &text_len)) {
        free(text);
        return false;
    }

    parser->pos = end + 1;
    if (out_text != NULL) {
        text[text_len] = '\0';
        *out_text = text;
        *out_len = text_len;
    }
    return true;
}

/* ---------- 숫자와 그 밖의 값 ---------- */

static void skip_digits(json_parser_t *parser) {
    while (is_digit(peek_char(parser))) {
        parser->pos++;
    }
}

/* 문법을 확인한 숫자 원문을 double로 바꾼다. out_number가 NULL이면 바꾸지 않는다 */
static bool convert_number(json_parser_t *parser, size_t start, double *out_number) {
    if (out_number == NULL) {
        return true;
    }
    size_t number_len = parser->pos - start;
    char *number_text = malloc(number_len + 1);
    if (number_text == NULL) {
        parser->is_out_of_memory = true;
        return false;
    }
    memcpy(number_text, parser->text + start, number_len);
    number_text[number_len] = '\0';
    *out_number = strtod(number_text, NULL);
    free(number_text);
    return true;
}

/* JSON 숫자 문법 : -?(0|[1-9][0-9]*)(.[0-9]+)?([eE][+-]?[0-9]+)? */
static bool parse_number(json_parser_t *parser, double *out_number) {
    size_t start = parser->pos;

    consume_char(parser, '-');
    if (!is_digit(peek_char(parser))) {
        return false;
    }
    if (!consume_char(parser, '0')) {
        skip_digits(parser);
    }
    if (consume_char(parser, '.')) {
        if (!is_digit(peek_char(parser))) {
            return false;
        }
        skip_digits(parser);
    }
    if (consume_char(parser, 'e') || consume_char(parser, 'E')) {
        if (!consume_char(parser, '+')) {
            consume_char(parser, '-');
        }
        if (!is_digit(peek_char(parser))) {
            return false;
        }
        skip_digits(parser);
    }
    return convert_number(parser, start, out_number);
}

static bool parse_literal(json_parser_t *parser, const char *literal) {
    size_t literal_len = strlen(literal);
    if (parser->len - parser->pos < literal_len) {
        return false;
    }
    if (strncmp(parser->text + parser->pos, literal, literal_len) != 0) {
        return false;
    }
    parser->pos += literal_len;
    return true;
}

static bool skip_array(json_parser_t *parser) {
    consume_char(parser, '[');
    skip_whitespace(parser);
    if (consume_char(parser, ']')) {
        return true;
    }
    while (true) {
        skip_whitespace(parser);
        if (!skip_value(parser)) {
            return false;
        }
        skip_whitespace(parser);
        if (consume_char(parser, ']')) {
            return true;
        }
        if (!consume_char(parser, ',')) {
            return false;
        }
    }
}

static bool skip_object(json_parser_t *parser) {
    consume_char(parser, '{');
    skip_whitespace(parser);
    if (consume_char(parser, '}')) {
        return true;
    }
    while (true) {
        skip_whitespace(parser);
        if (!parse_string(parser, NULL, NULL)) {
            return false;
        }
        skip_whitespace(parser);
        if (!consume_char(parser, ':')) {
            return false;
        }
        skip_whitespace(parser);
        if (!skip_value(parser)) {
            return false;
        }
        skip_whitespace(parser);
        if (consume_char(parser, '}')) {
            return true;
        }
        if (!consume_char(parser, ',')) {
            return false;
        }
    }
}

static bool skip_container(json_parser_t *parser) {
    if (parser->depth >= MAX_JSON_DEPTH) {
        return false;
    }
    parser->depth++;
    bool is_valid = (peek_char(parser) == '[') ? skip_array(parser) : skip_object(parser);
    parser->depth--;
    return is_valid;
}

/* 값 하나의 문법만 확인하고 넘긴다 */
static bool skip_value(json_parser_t *parser) {
    char current = peek_char(parser);
    if (current == '[' || current == '{') {
        return skip_container(parser);
    }
    if (current == '"') {
        return parse_string(parser, NULL, NULL);
    }
    if (current == '-' || is_digit(current)) {
        return parse_number(parser, NULL);
    }
    return parse_literal(parser, "true") || parse_literal(parser, "false")
           || parse_literal(parser, "null");
}

/* ---------- 최상위 객체 ---------- */

static void destroy_field(json_field_t *field) {
    free(field->name);
    field->name = NULL;
    free(field->string_value);
    field->string_value = NULL;
}

static bool append_field(json_parser_t *parser, json_object_t *object, const json_field_t *field) {
    if (object->field_count == object->field_capacity) {
        size_t new_capacity = object->field_capacity == 0 ? INITIAL_FIELD_CAPACITY
                                                          : object->field_capacity * 2;
        json_field_t *new_fields = realloc(object->fields, new_capacity * sizeof(*new_fields));
        if (new_fields == NULL) {
            parser->is_out_of_memory = true;
            return false;
        }
        object->fields = new_fields;
        object->field_capacity = new_capacity;
    }
    object->fields[object->field_count] = *field;
    object->field_count++;
    return true;
}

static bool parse_field_value(json_parser_t *parser, json_field_t *field) {
    char current = peek_char(parser);
    if (current == '"') {
        field->type = JSON_TYPE_STRING;
        return parse_string(parser, &field->string_value, &field->string_len);
    }
    if (current == '-' || is_digit(current)) {
        field->type = JSON_TYPE_NUMBER;
        return parse_number(parser, &field->number_value);
    }
    field->type = JSON_TYPE_OTHER;
    return skip_value(parser);
}

/* "이름": 값 하나를 읽어 object에 붙인다 */
static bool parse_field(json_parser_t *parser, json_object_t *object) {
    json_field_t field = {0};

    skip_whitespace(parser);
    if (!parse_string(parser, &field.name, &field.name_len)) {
        return false;
    }
    skip_whitespace(parser);
    if (!consume_char(parser, ':')) {
        destroy_field(&field);
        return false;
    }
    skip_whitespace(parser);
    if (!parse_field_value(parser, &field) || !append_field(parser, object, &field)) {
        destroy_field(&field);
        return false;
    }
    return true;
}

static bool parse_top_object(json_parser_t *parser, json_object_t *out_object) {
    consume_char(parser, '{');
    skip_whitespace(parser);
    if (consume_char(parser, '}')) {
        return true;
    }
    while (true) {
        if (!parse_field(parser, out_object)) {
            return false;
        }
        skip_whitespace(parser);
        if (consume_char(parser, '}')) {
            return true;
        }
        if (!consume_char(parser, ',')) {
            return false;
        }
    }
}

/* 최상위가 객체가 아닐 때, 문법은 맞는 값인지 확인해 둘 중 어느 오류인지 정한다 */
static json_parse_status_t check_non_object(json_parser_t *parser) {
    bool is_valid = skip_value(parser);
    skip_whitespace(parser);
    if (is_valid && !has_more(parser)) {
        return JSON_PARSE_NOT_OBJECT;
    }
    return JSON_PARSE_INVALID_JSON;
}

json_parse_status_t json_parse_object(const char *text, size_t text_len,
                                      json_object_t *out_object) {
    json_parser_t parser = {text, text_len, 0, 0, false};

    memset(out_object, 0, sizeof(*out_object));
    skip_whitespace(&parser);
    if (peek_char(&parser) != '{') {
        return check_non_object(&parser);
    }

    bool is_valid = parse_top_object(&parser, out_object);
    skip_whitespace(&parser);
    if (is_valid && !has_more(&parser)) {
        return JSON_PARSE_OK;
    }
    json_object_destroy(out_object);
    return parser.is_out_of_memory ? JSON_PARSE_NO_MEMORY : JSON_PARSE_INVALID_JSON;
}

void json_object_destroy(json_object_t *object) {
    if (object == NULL) {
        return;
    }
    for (size_t i = 0; i < object->field_count; i++) {
        destroy_field(&object->fields[i]);
    }
    free(object->fields);
    object->fields = NULL;
    object->field_count = 0;
    object->field_capacity = 0;
}

bool json_is_field_named(const json_field_t *field, const char *name) {
    size_t name_len = strlen(name);
    return field->name_len == name_len && memcmp(field->name, name, name_len) == 0;
}

const json_field_t *json_find_field(const json_object_t *object, const char *name) {
    for (size_t i = 0; i < object->field_count; i++) {
        if (json_is_field_named(&object->fields[i], name)) {
            return &object->fields[i];
        }
    }
    return NULL;
}

static bool is_same_name(const json_field_t *first, const json_field_t *second) {
    return first->name_len == second->name_len
           && memcmp(first->name, second->name, first->name_len) == 0;
}

bool json_has_duplicate_field(const json_object_t *object) {
    for (size_t i = 0; i < object->field_count; i++) {
        for (size_t j = i + 1; j < object->field_count; j++) {
            if (is_same_name(&object->fields[i], &object->fields[j])) {
                return true;
            }
        }
    }
    return false;
}

/* ---------- 이스케이프 ---------- */

/* 글자 하나를 JSON 문자열 안에 쓸 수 있는 모양으로 out_text에 쓰고, 쓴 바이트 수를 돌려준다 */
static size_t escape_char(char character, char *out_text) {
    switch (character) {
    case '"':
        return (size_t)sprintf(out_text, "\\\"");
    case '\\':
        return (size_t)sprintf(out_text, "\\\\");
    case '\n':
        return (size_t)sprintf(out_text, "\\n");
    case '\r':
        return (size_t)sprintf(out_text, "\\r");
    case '\t':
        return (size_t)sprintf(out_text, "\\t");
    }
    if ((unsigned char)character < FIRST_PRINTABLE_CHAR) {
        return (size_t)sprintf(out_text, "\\u%04x", (unsigned char)character);
    }
    out_text[0] = character;
    return 1;
}

char *json_escape_string(const char *text) {
    size_t text_len = strlen(text);
    char *escaped = malloc(text_len * MAX_ESCAPED_BYTES_PER_CHAR + QUOTE_AND_NUL_BYTES);
    if (escaped == NULL) {
        return NULL;
    }

    size_t escaped_len = 0;
    escaped[escaped_len++] = '"';
    for (size_t i = 0; i < text_len; i++) {
        escaped_len += escape_char(text[i], escaped + escaped_len);
    }
    escaped[escaped_len++] = '"';
    escaped[escaped_len] = '\0';
    return escaped;
}
