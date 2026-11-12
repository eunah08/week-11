#include "json.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define UTF8_MAX UINT32_C(0x10ffff)
#define SURROGATE_FIRST UINT32_C(0xd800)
#define SURROGATE_LAST UINT32_C(0xdfff)

typedef struct {
    const char *cursor;
    const char *end;
    int error;
} json_parser_t;

static void skip_space(json_parser_t *parser) {
    while (parser->cursor < parser->end && strchr(" \n\r\t", *parser->cursor) != NULL
           && *parser->cursor != '\0') {
        parser->cursor++;
    }
}

static bool consume(json_parser_t *parser, char expected) {
    skip_space(parser);
    if (parser->cursor == parser->end || *parser->cursor != expected) {
        return false;
    }
    parser->cursor++;
    return true;
}

static int hex_value(unsigned char value) {
    if (value >= '0' && value <= '9') { return value - '0'; }
    if (value >= 'a' && value <= 'f') { return value - 'a' + 10; }
    if (value >= 'A' && value <= 'F') { return value - 'A' + 10; }
    return -1;
}

static bool parse_hex(json_parser_t *parser, uint32_t *out_code) {
    *out_code = 0;
    if (parser->end - parser->cursor < 4) { return false; }
    for (size_t i = 0; i < 4; i++) {
        int digit = hex_value((unsigned char)*parser->cursor++);
        if (digit < 0) { return false; }
        *out_code = *out_code * 16 + (uint32_t)digit;
    }
    return true;
}

static size_t encode_utf8(uint32_t code, char *output) {
    if (code < 0x80) { output[0] = (char)code; return 1; }
    if (code < 0x800) {
        output[0] = (char)(0xc0 | code >> 6);
        output[1] = (char)(0x80 | (code & 0x3f)); return 2;
    }
    if (code < 0x10000) {
        output[0] = (char)(0xe0 | code >> 12);
        output[1] = (char)(0x80 | ((code >> 6) & 0x3f));
        output[2] = (char)(0x80 | (code & 0x3f)); return 3;
    }
    output[0] = (char)(0xf0 | code >> 18);
    output[1] = (char)(0x80 | ((code >> 12) & 0x3f));
    output[2] = (char)(0x80 | ((code >> 6) & 0x3f));
    output[3] = (char)(0x80 | (code & 0x3f)); return 4;
}

bool json_is_utf8(const char *input, size_t len) {
    size_t index = 0;
    while (index < len) {
        unsigned char first = (unsigned char)input[index++];
        if (first < 0x80) { continue; }
        size_t count = first >= 0xf0 ? 3 : first >= 0xe0 ? 2 : 1;
        uint32_t code = first & (count == 3 ? 7 : count == 2 ? 15 : 31);
        uint32_t minimum = count == 3 ? 0x10000 : count == 2 ? 0x800 : 0x80;
        if (first < 0xc2 || first > 0xf4 || len - index < count) { return false; }
        for (size_t i = 0; i < count; i++) {
            unsigned char next = (unsigned char)input[index++];
            if ((next & 0xc0) != 0x80) { return false; }
            code = (code << 6) | (next & 0x3f);
        }
        if (code < minimum || code > UTF8_MAX
            || (code >= SURROGATE_FIRST && code <= SURROGATE_LAST)) { return false; }
    }
    return true;
}

static bool parse_escape(json_parser_t *parser, char *output, size_t *out_len) {
    if (parser->cursor == parser->end) { return false; }
    char escaped = *parser->cursor++;
    const char *codes = "\"\\/bfnrt";
    const char *values = "\"\\/\b\f\n\r\t";
    const char *found = escaped ? strchr(codes, escaped) : NULL;
    if (found != NULL) { output[(*out_len)++] = values[found - codes]; return true; }
    uint32_t code, low;
    if (escaped != 'u' || !parse_hex(parser, &code)) { return false; }
    if (code >= 0xd800 && code <= 0xdbff) {
        if (parser->end - parser->cursor < 2 || parser->cursor[0] != '\\'
            || parser->cursor[1] != 'u') { return false; }
        parser->cursor += 2;
        if (!parse_hex(parser, &low) || low < 0xdc00 || low > 0xdfff) { return false; }
        code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
    } else if (code >= 0xdc00 && code <= 0xdfff) { return false; }
    *out_len += encode_utf8(code, output + *out_len);
    return true;
}

static char *parse_string(json_parser_t *parser, size_t *out_len) {
    if (!consume(parser, '"')) { return NULL; }
    char *output = malloc((size_t)(parser->end - parser->cursor) + 1);
    if (output == NULL) { parser->error = -2; return NULL; }
    *out_len = 0;
    while (parser->cursor < parser->end) {
        unsigned char value = (unsigned char)*parser->cursor++;
        if (value == '"') {
            output[*out_len] = '\0';
            if (json_is_utf8(output, *out_len)) { return output; }
            break;
        }
        if (value < 0x20) { break; }
        if (value == '\\') {
            if (!parse_escape(parser, output, out_len)) { break; }
        } else { output[(*out_len)++] = (char)value; }
    }
    free(output);
    return NULL;
}

static bool is_digit(const json_parser_t *parser) {
    return parser->cursor < parser->end && *parser->cursor >= '0' && *parser->cursor <= '9';
}

static bool is_integer_number(const char *start, const char *end) {
    size_t fraction_count = 0, trailing_zero_count = 0;
    bool has_fraction = false, has_nonzero = false;
    const char *cursor = start;
    for (; cursor < end && *cursor != 'e' && *cursor != 'E'; cursor++) {
        if (*cursor == '.') { has_fraction = true; continue; }
        if (*cursor == '-') { continue; }
        if (has_fraction) { fraction_count++; }
        if (*cursor == '0') { trailing_zero_count++; }
        else { has_nonzero = true; trailing_zero_count = 0; }
    }
    long exponent = 0, sign = 1;
    if (cursor < end) {
        cursor++;
        if (*cursor == '-' || *cursor == '+') { sign = *cursor++ == '-' ? -1 : 1; }
        for (; cursor < end; cursor++) {
            if (exponent < 1000000) { exponent = exponent * 10 + (*cursor - '0'); }
        }
    }
    return !has_nonzero || (long)fraction_count - (long)trailing_zero_count <= exponent * sign;
}

static bool parse_number(json_parser_t *parser, json_field_t *field) {
    const char *start = parser->cursor;
    if (parser->cursor < parser->end && *parser->cursor == '-') { parser->cursor++; }
    if (!is_digit(parser)) { return false; }
    if (*parser->cursor++ != '0') {
        while (is_digit(parser)) { parser->cursor++; }
    }
    if (parser->cursor < parser->end && *parser->cursor == '.') {
        parser->cursor++;
        if (!is_digit(parser)) { return false; }
        while (is_digit(parser)) { parser->cursor++; }
    }
    if (parser->cursor < parser->end && strchr("eE", *parser->cursor) && *parser->cursor) {
        parser->cursor++;
        if (parser->cursor < parser->end && strchr("+-", *parser->cursor)
            && *parser->cursor) { parser->cursor++; }
        if (!is_digit(parser)) { return false; }
        while (is_digit(parser)) { parser->cursor++; }
    }
    size_t len = (size_t)(parser->cursor - start);
    char *copy = malloc(len + 1);
    if (copy == NULL) { parser->error = -2; return false; }
    memcpy(copy, start, len); copy[len] = '\0';
    field->number = strtod(copy, NULL);
    field->is_integer = is_integer_number(start, parser->cursor);
    free(copy);
    return true;
}

static bool parse_value(json_parser_t *parser, json_field_t *field, size_t depth);

static bool parse_container(json_parser_t *parser, size_t depth, bool is_object) {
    parser->cursor++;
    if (consume(parser, is_object ? '}' : ']')) { return true; }
    do {
        if (is_object) {
            size_t len;
            char *name = parse_string(parser, &len);
            if (name == NULL) { return false; }
            free(name);
            if (!consume(parser, ':')) { return false; }
        }
        json_field_t field = {0};
        bool valid = parse_value(parser, &field, depth + 1);
        free(field.string);
        if (!valid) { return false; }
        if (consume(parser, is_object ? '}' : ']')) { return true; }
    } while (consume(parser, ','));
    return false;
}

static bool parse_value(json_parser_t *parser, json_field_t *field, size_t depth) {
    skip_space(parser);
    if (depth > JSON_MAX_DEPTH || parser->cursor == parser->end) { return false; }
    char value = *parser->cursor;
    field->kind = JSON_OTHER;
    if (value == '"') {
        field->kind = JSON_STRING;
        field->string = parse_string(parser, &field->string_len);
        return field->string != NULL;
    }
    if (value == '{' || value == '[') { return parse_container(parser, depth, value == '{'); }
    if (value == '-' || is_digit(parser)) {
        field->kind = JSON_NUMBER;
        return parse_number(parser, field);
    }
    const char *literal = value == 't' ? "true" : value == 'f' ? "false" : "null";
    size_t len = strlen(literal);
    if ((size_t)(parser->end - parser->cursor) < len
        || memcmp(parser->cursor, literal, len) != 0) { return false; }
    parser->cursor += len;
    return true;
}

static bool parse_fields(json_parser_t *parser, json_object_t *object) {
    if (consume(parser, '}')) { return true; }
    do {
        json_field_t field = {0};
        field.name = parse_string(parser, &field.name_len);
        if (field.name == NULL) { return false; }
        bool valid = consume(parser, ':') && parse_value(parser, &field, 1);
        for (size_t i = 0; i < object->count; i++) {
            if (field.name_len == object->fields[i].name_len
                && memcmp(field.name, object->fields[i].name, field.name_len) == 0) {
                object->has_duplicate = true;
            }
        }
        if (object->count < JSON_MAX_FIELDS) { object->fields[object->count++] = field; }
        else { object->has_overflow = true; free(field.name); free(field.string); }
        if (!valid) { return false; }
        if (consume(parser, '}')) { return true; }
    } while (consume(parser, ','));
    return false;
}

int json_parse_object(const char *input, size_t len, json_object_t *out_object) {
    memset(out_object, 0, sizeof(*out_object));
    if (input == NULL) { return -1; }
    json_parser_t parser = {input, input + len, -1};
    skip_space(&parser);
    bool valid;
    if (consume(&parser, '{')) {
        out_object->is_object = true;
        valid = parse_fields(&parser, out_object);
    } else {
        json_field_t field = {0};
        valid = parse_value(&parser, &field, 0);
        free(field.string);
    }
    skip_space(&parser);
    if (!valid || parser.cursor != parser.end) {
        json_object_destroy(out_object);
        return parser.error;
    }
    return 0;
}

void json_object_destroy(json_object_t *object) {
    for (size_t i = 0; i < object->count; i++) {
        free(object->fields[i].name);
        free(object->fields[i].string);
    }
    memset(object, 0, sizeof(*object));
}

const json_field_t *json_find_field(const json_object_t *object, const char *name) {
    for (size_t i = 0; i < object->count; i++) {
        if (object->fields[i].name_len == strlen(name)
            && strcmp(object->fields[i].name, name) == 0) { return &object->fields[i]; }
    }
    return NULL;
}

char *json_quote(const char *input) {
    size_t len = strlen(input);
    if (len > (SIZE_MAX - 3) / 6) { return NULL; }
    char *output = malloc(len * 6 + 3);
    if (output == NULL) { return NULL; }
    char *cursor = output;
    *cursor++ = '"';
    const char *hex = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        unsigned char value = (unsigned char)input[i];
        if (value < 0x20) {
            memcpy(cursor, "\\u00", 4); cursor += 4;
            *cursor++ = hex[value >> 4]; *cursor++ = hex[value & 15];
        } else {
            if (value == '"' || value == '\\') { *cursor++ = '\\'; }
            *cursor++ = (char)value;
        }
    }
    *cursor++ = '"'; *cursor = '\0';
    return output;
}
