#include "json.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_BUFFER_BYTES 64
#define INITIAL_FIELD_CAPACITY 4
#define HEX_DIGIT_COUNT 4
#define INTEGER_TEXT_BYTES 32
#define HIGH_SURROGATE_MIN 0xD800
#define LOW_SURROGATE_MIN 0xDC00
#define LOW_SURROGATE_MAX 0xDFFF
#define FIRST_CONTROL_CHAR 0x20
#define MAX_INT64_DIGITS 19         /* INT64_MAX = 9223372036854775807 (19자리) */
#define MAX_EXPONENT_ABS 100000L    /* 지수가 이보다 크면 어차피 범위 밖이라 더 세지 않는다 */

typedef struct {
    const char *text;
    size_t len;
    size_t pos;
    int depth;
} json_parser_t;

static bool skip_value(json_parser_t *parser);

/* ---------- UTF-8 검사 (JSON 원문은 UTF-8이어야 한다, RFC 8259) ---------- */

static size_t find_utf8_sequence_len(unsigned char first, unsigned char *out_min,
                                     unsigned char *out_max) {
    *out_min = 0x80;
    *out_max = 0xBF;
    if (first < 0x80) {
        return 1;
    }
    if (first >= 0xC2 && first <= 0xDF) {
        return 2;
    }
    if (first >= 0xE0 && first <= 0xEF) {
        *out_min = (first == 0xE0) ? 0xA0 : 0x80;
        *out_max = (first == 0xED) ? 0x9F : 0xBF;
        return 3;
    }
    if (first >= 0xF0 && first <= 0xF4) {
        *out_min = (first == 0xF0) ? 0x90 : 0x80;
        *out_max = (first == 0xF4) ? 0x8F : 0xBF;
        return 4;
    }
    return 0;
}

static bool is_valid_utf8(const unsigned char *text, size_t text_len) {
    size_t index = 0;

    while (index < text_len) {
        unsigned char second_min = 0;
        unsigned char second_max = 0;
        size_t sequence_len = find_utf8_sequence_len(text[index], &second_min, &second_max);
        if (sequence_len == 0 || index + sequence_len > text_len) {
            return false;
        }
        for (size_t i = 1; i < sequence_len; i++) {
            unsigned char min = (i == 1) ? second_min : 0x80;
            unsigned char max = (i == 1) ? second_max : 0xBF;
            if (text[index + i] < min || text[index + i] > max) {
                return false;
            }
        }
        index += sequence_len;
    }
    return true;
}

/* ---------- 읽기 ---------- */

static void skip_whitespace(json_parser_t *parser) {
    while (parser->pos < parser->len) {
        char current = parser->text[parser->pos];
        if (current != ' ' && current != '\t' && current != '\n' && current != '\r') {
            return;
        }
        parser->pos++;
    }
}

static bool consume_char(json_parser_t *parser, char expected) {
    if (parser->pos < parser->len && parser->text[parser->pos] == expected) {
        parser->pos++;
        return true;
    }
    return false;
}

static bool read_hex4(json_parser_t *parser, uint32_t *out_code_unit) {
    uint32_t code_unit = 0;

    if (parser->pos + HEX_DIGIT_COUNT > parser->len) {
        return false;
    }
    for (int i = 0; i < HEX_DIGIT_COUNT; i++) {
        char digit = parser->text[parser->pos++];
        code_unit <<= 4;
        if (digit >= '0' && digit <= '9') {
            code_unit |= (uint32_t)(digit - '0');
        } else if (digit >= 'a' && digit <= 'f') {
            code_unit |= (uint32_t)(digit - 'a' + 10);
        } else if (digit >= 'A' && digit <= 'F') {
            code_unit |= (uint32_t)(digit - 'A' + 10);
        } else {
            return false;
        }
    }
    *out_code_unit = code_unit;
    return true;
}

/* 코드 포인트 하나를 UTF-8로 out에 쓰고 쓴 바이트 수를 돌려준다 */
static size_t write_utf8(uint32_t code_point, char *out) {
    if (code_point < 0x80) {
        out[0] = (char)code_point;
        return 1;
    }
    if (code_point < 0x800) {
        out[0] = (char)(0xC0 | (code_point >> 6));
        out[1] = (char)(0x80 | (code_point & 0x3F));
        return 2;
    }
    if (code_point < 0x10000) {
        out[0] = (char)(0xE0 | (code_point >> 12));
        out[1] = (char)(0x80 | ((code_point >> 6) & 0x3F));
        out[2] = (char)(0x80 | (code_point & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (code_point >> 18));
    out[1] = (char)(0x80 | ((code_point >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((code_point >> 6) & 0x3F));
    out[3] = (char)(0x80 | (code_point & 0x3F));
    return 4;
}

/* \u 다음 4자리(서로게이트 쌍이면 \uXXXX 하나 더)를 읽어 UTF-8로 쓴다. 짝 없는 서로게이트는 거부 */
static bool decode_unicode_escape(json_parser_t *parser, char *out, size_t *out_written) {
    uint32_t code_point = 0;
    uint32_t low = 0;

    if (!read_hex4(parser, &code_point)) {
        return false;
    }
    if (code_point >= LOW_SURROGATE_MIN && code_point <= LOW_SURROGATE_MAX) {
        return false;
    }
    if (code_point >= HIGH_SURROGATE_MIN && code_point < LOW_SURROGATE_MIN) {
        if (!consume_char(parser, '\\') || !consume_char(parser, 'u') ||
            !read_hex4(parser, &low) || low < LOW_SURROGATE_MIN || low > LOW_SURROGATE_MAX) {
            return false;
        }
        code_point = 0x10000 + ((code_point - HIGH_SURROGATE_MIN) << 10) +
                     (low - LOW_SURROGATE_MIN);
    }
    *out_written = write_utf8(code_point, out);
    return true;
}

static bool decode_escape(json_parser_t *parser, char *out, size_t *out_written) {
    char escaped = 0;

    if (parser->pos >= parser->len) {
        return false;
    }
    escaped = parser->text[parser->pos++];
    *out_written = 1;
    switch (escaped) {
    case '"': *out = '"'; return true;
    case '\\': *out = '\\'; return true;
    case '/': *out = '/'; return true;
    case 'b': *out = '\b'; return true;
    case 'f': *out = '\f'; return true;
    case 'n': *out = '\n'; return true;
    case 'r': *out = '\r'; return true;
    case 't': *out = '\t'; return true;
    case 'u': return decode_unicode_escape(parser, out, out_written);
    default: return false;
    }
}

/* 여는 따옴표 다음부터 닫는 따옴표 앞까지의 원문 길이. 닫는 따옴표가 없어도 끝까지 센다 */
static size_t measure_raw_string(const json_parser_t *parser) {
    size_t index = parser->pos;

    while (index < parser->len && parser->text[index] != '"') {
        index += (parser->text[index] == '\\') ? 2 : 1;
    }
    return (index > parser->len ? parser->len : index) - parser->pos;
}

/*
 * 따옴표로 시작하는 문자열을 읽어 이스케이프를 푼 복사본을 만든다.
 * 푼 결과는 원문보다 길어지지 않으므로 원문 길이만큼만 확보한다.
 * 반환 : 성공 JSON_OK. *out_text는 호출한 쪽이 free()
 */
static json_status_t parse_string(json_parser_t *parser, char **out_text, size_t *out_len) {
    char *decoded = NULL;
    size_t decoded_len = 0;

    if (!consume_char(parser, '"')) {
        return JSON_ERR_SYNTAX;
    }
    decoded = malloc(measure_raw_string(parser) + 1);
    if (decoded == NULL) {
        return JSON_ERR_NO_MEMORY;
    }
    while (parser->pos < parser->len && parser->text[parser->pos] != '"') {
        unsigned char current = (unsigned char)parser->text[parser->pos++];
        size_t written = 1;
        if (current < FIRST_CONTROL_CHAR ||
            (current == '\\' && !decode_escape(parser, decoded + decoded_len, &written))) {
            free(decoded);
            return JSON_ERR_SYNTAX;
        }
        if (current != '\\') {
            decoded[decoded_len] = (char)current;
        }
        decoded_len += written;
    }
    if (!consume_char(parser, '"')) {
        free(decoded);
        return JSON_ERR_SYNTAX;
    }
    decoded[decoded_len] = '\0';
    *out_text = decoded;
    *out_len = decoded_len;
    return JSON_OK;
}

static bool is_digit_at(const json_parser_t *parser) {
    return parser->pos < parser->len && parser->text[parser->pos] >= '0' &&
           parser->text[parser->pos] <= '9';
}

static bool skip_digits(json_parser_t *parser) {
    if (!is_digit_at(parser)) {
        return false;
    }
    while (is_digit_at(parser)) {
        parser->pos++;
    }
    return true;
}

/* JSON 숫자 문법 : -? (0 | [1-9][0-9]*) (.[0-9]+)? ([eE][+-]?[0-9]+)? */
static bool skip_number(json_parser_t *parser) {
    consume_char(parser, '-');
    if (consume_char(parser, '0')) {
        if (is_digit_at(parser)) {
            return false;    /* 01 같은 앞자리 0은 허용하지 않는다 */
        }
    } else if (!skip_digits(parser)) {
        return false;
    }
    if (consume_char(parser, '.') && !skip_digits(parser)) {
        return false;
    }
    if (consume_char(parser, 'e') || consume_char(parser, 'E')) {
        if (!consume_char(parser, '+')) {
            consume_char(parser, '-');
        }
        return skip_digits(parser);
    }
    return true;
}

static bool skip_literal(json_parser_t *parser, const char *literal) {
    size_t literal_len = strlen(literal);

    if (parser->len - parser->pos < literal_len ||
        memcmp(parser->text + parser->pos, literal, literal_len) != 0) {
        return false;
    }
    parser->pos += literal_len;
    return true;
}

static bool skip_string(json_parser_t *parser) {
    char *text = NULL;
    size_t text_len = 0;
    bool is_ok = parse_string(parser, &text, &text_len) == JSON_OK;

    free(text);
    return is_ok;
}

/* 배열과 객체 모두 "여는 괄호, 항목(쉼표로 구분), 닫는 괄호" 모양이라 하나로 처리한다 */
static bool skip_container(json_parser_t *parser, char close, bool is_object) {
    bool is_ok = true;

    if (++parser->depth > JSON_MAX_DEPTH) {
        return false;
    }
    skip_whitespace(parser);
    if (consume_char(parser, close)) {
        parser->depth--;
        return true;
    }
    do {
        skip_whitespace(parser);
        if (is_object) {
            is_ok = skip_string(parser);
            skip_whitespace(parser);
            is_ok = is_ok && consume_char(parser, ':');
        }
        is_ok = is_ok && skip_value(parser);
        skip_whitespace(parser);
    } while (is_ok && consume_char(parser, ','));
    parser->depth--;
    return is_ok && consume_char(parser, close);
}

static bool skip_value(json_parser_t *parser) {
    skip_whitespace(parser);
    if (parser->pos >= parser->len) {
        return false;
    }
    switch (parser->text[parser->pos]) {
    case '"': return skip_string(parser);
    case '{': parser->pos++; return skip_container(parser, '}', true);
    case '[': parser->pos++; return skip_container(parser, ']', false);
    case 't': return skip_literal(parser, "true");
    case 'f': return skip_literal(parser, "false");
    case 'n': return skip_literal(parser, "null");
    default: return skip_number(parser);
    }
}

static json_status_t append_field(json_object_t *object, size_t *capacity,
                                  const json_field_t *field) {
    if (object->field_count >= JSON_MAX_FIELDS) {
        return JSON_ERR_TOO_MANY_FIELDS;
    }
    if (object->field_count == *capacity) {
        size_t new_capacity = (*capacity == 0) ? INITIAL_FIELD_CAPACITY : *capacity * 2;
        json_field_t *new_fields = realloc(object->fields, new_capacity * sizeof(*new_fields));
        if (new_fields == NULL) {
            return JSON_ERR_NO_MEMORY;
        }
        object->fields = new_fields;
        *capacity = new_capacity;
    }
    object->fields[object->field_count++] = *field;
    return JSON_OK;
}

/* 필드 값 하나를 읽는다. 문자열은 푼 값, 숫자는 원문을 복사하고, 나머지는 건너뛰기만 한다 */
static json_status_t parse_field_value(json_parser_t *parser, json_field_t *field) {
    char first = (parser->pos < parser->len) ? parser->text[parser->pos] : '\0';
    size_t start = parser->pos;

    if (first == '"') {
        field->type = JSON_TYPE_STRING;
        return parse_string(parser, &field->text, &field->text_len);
    }
    if (!skip_value(parser)) {
        return JSON_ERR_SYNTAX;
    }
    field->type = (first == '-' || (first >= '0' && first <= '9')) ? JSON_TYPE_NUMBER
                                                                     : JSON_TYPE_OTHER;
    if (field->type == JSON_TYPE_NUMBER) {
        field->text_len = parser->pos - start;
        field->text = malloc(field->text_len + 1);
        if (field->text == NULL) {
            return JSON_ERR_NO_MEMORY;
        }
        memcpy(field->text, parser->text + start, field->text_len);
        field->text[field->text_len] = '\0';
    }
    return JSON_OK;
}

static void destroy_field(json_field_t *field) {
    free(field->name);
    field->name = NULL;
    free(field->text);
    field->text = NULL;
}

/* "이름": 값 하나를 읽어 object에 붙인다 */
static json_status_t parse_member(json_parser_t *parser, json_object_t *object,
                                  size_t *capacity) {
    json_field_t field = {0};
    json_status_t status = JSON_OK;

    skip_whitespace(parser);
    status = parse_string(parser, &field.name, &field.name_len);
    if (status == JSON_OK) {
        skip_whitespace(parser);
        status = consume_char(parser, ':') ? JSON_OK : JSON_ERR_SYNTAX;
    }
    if (status == JSON_OK) {
        skip_whitespace(parser);
        status = parse_field_value(parser, &field);
    }
    if (status == JSON_OK) {
        status = append_field(object, capacity, &field);
    }
    if (status != JSON_OK) {
        destroy_field(&field);
    }
    return status;
}

static json_status_t parse_members(json_parser_t *parser, json_object_t *object) {
    size_t capacity = 0;
    json_status_t status = JSON_OK;

    skip_whitespace(parser);
    if (consume_char(parser, '}')) {
        return JSON_OK;
    }
    do {
        status = parse_member(parser, object, &capacity);
        skip_whitespace(parser);
    } while (status == JSON_OK && consume_char(parser, ','));
    if (status == JSON_OK && !consume_char(parser, '}')) {
        status = JSON_ERR_SYNTAX;
    }
    return status;
}

static bool has_duplicate_field(const json_object_t *object) {
    for (size_t i = 0; i < object->field_count; i++) {
        for (size_t j = i + 1; j < object->field_count; j++) {
            const json_field_t *left = &object->fields[i];
            const json_field_t *right = &object->fields[j];
            if (left->name_len == right->name_len &&
                memcmp(left->name, right->name, left->name_len) == 0) {
                return true;
            }
        }
    }
    return false;
}

/* 최상위가 객체가 아닐 때 : 올바른 JSON 값이면 NOT_OBJECT, 아니면 SYNTAX */
static json_status_t classify_non_object(json_parser_t *parser) {
    if (!skip_value(parser)) {
        return JSON_ERR_SYNTAX;
    }
    skip_whitespace(parser);
    return parser->pos == parser->len ? JSON_ERR_NOT_OBJECT : JSON_ERR_SYNTAX;
}

json_status_t json_parse_object(const char *text, size_t text_len, json_object_t *out_object) {
    json_parser_t parser = {text, text_len, 0, 0};
    json_status_t status = JSON_OK;

    out_object->fields = NULL;
    out_object->field_count = 0;
    if (text == NULL || !is_valid_utf8((const unsigned char *)text, text_len)) {
        return JSON_ERR_SYNTAX;
    }
    skip_whitespace(&parser);
    if (!consume_char(&parser, '{')) {
        return classify_non_object(&parser);
    }
    status = parse_members(&parser, out_object);
    skip_whitespace(&parser);
    if (status == JSON_OK && parser.pos != parser.len) {
        status = JSON_ERR_SYNTAX;
    }
    if (status == JSON_OK && has_duplicate_field(out_object)) {
        status = JSON_ERR_DUPLICATE;
    }
    if (status != JSON_OK) {
        json_object_destroy(out_object);
    }
    return status;
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
}

const json_field_t *json_object_find(const json_object_t *object, const char *name) {
    size_t name_len = strlen(name);

    for (size_t i = 0; i < object->field_count; i++) {
        const json_field_t *field = &object->fields[i];
        if (field->name_len == name_len && memcmp(field->name, name, name_len) == 0) {
            return field;
        }
    }
    return NULL;
}

/*
 * 숫자 원문을 "유효 숫자들 × 10^scale"로 본다. double로 바꾸지 않는 이유 :
 * 2147483647.00000001 같은 값이 반올림되어 정수처럼 보이면 안 되기 때문이다.
 */
typedef struct {
    char digits[MAX_INT64_DIGITS];    /* 앞뒤 0을 뗀 유효 숫자 */
    size_t digit_count;
    size_t pending_zero_count;        /* 아직 뒤에 숫자가 올지 몰라 미뤄 둔 0의 개수 */
    long scale;                       /* 10의 지수 */
    bool is_negative;
    bool is_too_long;                 /* 유효 숫자가 int64에 들어갈 수 없을 만큼 많다 */
} decimal_t;

static void append_digit(decimal_t *decimal, char digit) {
    if (decimal->digit_count == MAX_INT64_DIGITS) {
        decimal->is_too_long = true;
        return;
    }
    decimal->digits[decimal->digit_count++] = digit;
}

/*
 * 숫자 하나를 유효 숫자 목록에 붙인다. 앞의 0은 값에 영향이 없어 버리고,
 * 중간·끝의 0은 미뤄 뒀다가 0이 아닌 숫자가 오면 붙인다. 끝까지 남은 0은 scale로 옮긴다.
 * 그래서 1.0000000000000000000000 처럼 0이 많이 붙어도 유효 숫자는 "1" 하나다.
 */
static void push_digit(decimal_t *decimal, char digit) {
    if (digit == '0') {
        decimal->pending_zero_count += (decimal->digit_count > 0);
        return;
    }
    for (; decimal->pending_zero_count > 0; decimal->pending_zero_count--) {
        append_digit(decimal, '0');
    }
    append_digit(decimal, digit);
}

static long read_exponent(const char *text) {
    long exponent = 0;
    bool is_negative = (*text == '-');

    if (*text == '-' || *text == '+') {
        text++;
    }
    for (; *text >= '0' && *text <= '9'; text++) {
        if (exponent < MAX_EXPONENT_ABS) {
            exponent = exponent * 10 + (*text - '0');
        }
    }
    return is_negative ? -exponent : exponent;
}

/* 이미 문법 검사를 마친 숫자 원문을 decimal_t로 나눈다 */
static void split_decimal(const char *text, decimal_t *out_decimal) {
    bool is_fraction = false;

    memset(out_decimal, 0, sizeof(*out_decimal));
    out_decimal->is_negative = (*text == '-');
    for (; *text != '\0' && *text != 'e' && *text != 'E'; text++) {
        if (*text == '.') {
            is_fraction = true;
        } else if (*text != '-') {
            push_digit(out_decimal, *text);
            out_decimal->scale -= is_fraction;    /* 소수점 아래 한 자리마다 10으로 나눈다 */
        }
    }
    if (*text == 'e' || *text == 'E') {
        out_decimal->scale += read_exponent(text + 1);
    }
    out_decimal->scale += (long)out_decimal->pending_zero_count;    /* 끝의 0 */
}

/* 숫자 필드가 정수 값을 가지면 참이다. 1.0, 1e3은 참이고 1.5, 1.00000001, int64 밖은 거짓 */
bool json_field_to_int64(const json_field_t *field, int64_t *out_value) {
    decimal_t decimal;
    uint64_t magnitude = 0;

    if (field == NULL || field->type != JSON_TYPE_NUMBER) {
        return false;
    }
    split_decimal(field->text, &decimal);
    if (decimal.digit_count == 0) {
        *out_value = 0;
        return true;
    }
    if (decimal.is_too_long || decimal.scale < 0 ||
        decimal.scale + (long)decimal.digit_count > MAX_INT64_DIGITS) {
        return false;
    }
    for (size_t i = 0; i < decimal.digit_count; i++) {
        magnitude = magnitude * 10 + (uint64_t)(decimal.digits[i] - '0');
    }
    for (long i = 0; i < decimal.scale; i++) {
        magnitude *= 10;
    }
    if (magnitude > (uint64_t)INT64_MAX) {
        return false;
    }
    *out_value = decimal.is_negative ? -(int64_t)magnitude : (int64_t)magnitude;
    return true;
}

/* ---------- 쓰기 ---------- */

static bool reserve_buffer(json_buffer_t *buffer, size_t extra_len) {
    size_t needed = buffer->len + extra_len + 1;
    size_t new_capacity = buffer->capacity == 0 ? INITIAL_BUFFER_BYTES : buffer->capacity;
    char *new_data = NULL;

    if (buffer->has_error) {
        return false;
    }
    if (needed <= buffer->capacity) {
        return true;
    }
    while (new_capacity < needed) {
        new_capacity *= 2;
    }
    new_data = realloc(buffer->data, new_capacity);
    if (new_data == NULL) {
        buffer->has_error = true;
        return false;
    }
    buffer->data = new_data;
    buffer->capacity = new_capacity;
    return true;
}

void json_buffer_append(json_buffer_t *buffer, const char *text, size_t text_len) {
    if (!reserve_buffer(buffer, text_len)) {
        return;
    }
    memcpy(buffer->data + buffer->len, text, text_len);
    buffer->len += text_len;
    buffer->data[buffer->len] = '\0';
}

void json_buffer_append_text(json_buffer_t *buffer, const char *text) {
    json_buffer_append(buffer, text, strlen(text));
}

static void append_escaped_char(json_buffer_t *buffer, unsigned char current) {
    char escaped[8];

    switch (current) {
    case '"': json_buffer_append(buffer, "\\\"", 2); return;
    case '\\': json_buffer_append(buffer, "\\\\", 2); return;
    case '\n': json_buffer_append(buffer, "\\n", 2); return;
    case '\r': json_buffer_append(buffer, "\\r", 2); return;
    case '\t': json_buffer_append(buffer, "\\t", 2); return;
    default:
        snprintf(escaped, sizeof(escaped), "\\u%04x", current);
        json_buffer_append(buffer, escaped, strlen(escaped));
    }
}

/* 이스케이프가 필요 없는 구간은 한 번에 복사한다 (1MiB 값도 빠르게) */
void json_buffer_append_string(json_buffer_t *buffer, const char *text, size_t text_len) {
    size_t plain_start = 0;

    json_buffer_append(buffer, "\"", 1);
    for (size_t i = 0; i < text_len; i++) {
        unsigned char current = (unsigned char)text[i];
        if (current >= FIRST_CONTROL_CHAR && current != '"' && current != '\\') {
            continue;
        }
        json_buffer_append(buffer, text + plain_start, i - plain_start);
        append_escaped_char(buffer, current);
        plain_start = i + 1;
    }
    json_buffer_append(buffer, text + plain_start, text_len - plain_start);
    json_buffer_append(buffer, "\"", 1);
}

void json_buffer_append_int(json_buffer_t *buffer, int64_t value) {
    char text[INTEGER_TEXT_BYTES];

    snprintf(text, sizeof(text), "%" PRId64, value);
    json_buffer_append_text(buffer, text);
}

void json_buffer_append_uint(json_buffer_t *buffer, uint64_t value) {
    char text[INTEGER_TEXT_BYTES];

    snprintf(text, sizeof(text), "%" PRIu64, value);
    json_buffer_append_text(buffer, text);
}

void json_buffer_destroy(json_buffer_t *buffer) {
    if (buffer == NULL) {
        return;
    }
    free(buffer->data);
    buffer->data = NULL;
    buffer->len = 0;
    buffer->capacity = 0;
    buffer->has_error = false;
}
