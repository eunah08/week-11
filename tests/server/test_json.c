#include <stdio.h>
#include <string.h>

#include "json.h"

/* 직접 만든 JSON 읽기·쓰기 단위 테스트 (협약 11.4) */

static int failed_count = 0;

#define CHECK(condition)                                                          \
    do {                                                                          \
        if (!(condition)) {                                                       \
            fprintf(stderr, "  실패 %s:%d %s\n", __FILE__, __LINE__, #condition); \
            failed_count++;                                                       \
        }                                                                         \
    } while (0)

static json_status_t parse_text(const char *text, json_object_t *out_object) {
    return json_parse_object(text, strlen(text), out_object);
}

static void test_parse_fields(void) {
    json_object_t object;
    const json_field_t *field = NULL;

    CHECK(parse_text(" {\"value\" : \"a\\\"b\\\\c\\n\\u00e9\\ud83d\\ude00\", \"ttlMs\": 1.0e3,"
                     " \"other\": [1, {\"x\": null}]} ", &object) == JSON_OK);
    CHECK(object.field_count == 3);
    field = json_object_find(&object, "value");
    CHECK(field != NULL && field->type == JSON_TYPE_STRING);
    CHECK(field != NULL && strcmp(field->text, "a\"b\\c\n\xc3\xa9\xf0\x9f\x98\x80") == 0);
    field = json_object_find(&object, "ttlMs");
    CHECK(field != NULL && field->type == JSON_TYPE_NUMBER && strcmp(field->text, "1.0e3") == 0);
    CHECK(json_object_find(&object, "other")->type == JSON_TYPE_OTHER);
    CHECK(json_object_find(&object, "missing") == NULL);
    json_object_destroy(&object);
}

static void test_nul_in_string_keeps_length(void) {
    json_object_t object;
    const json_field_t *field = NULL;

    CHECK(parse_text("{\"value\":\"a\\u0000b\"}", &object) == JSON_OK);
    field = json_object_find(&object, "value");
    CHECK(field != NULL && field->text_len == 3 && field->text[1] == '\0');
    json_object_destroy(&object);
}

static void test_parse_errors(void) {
    const char *syntax_errors[] = {"", "{", "{\"a\":}", "{\"a\":1,}", "{\"a\" 1}", "{'a':1}",
                                   "{\"a\":01}", "{\"a\":1.}", "{\"a\":tru}", "{\"a\":1} x",
                                   "{\"a\":\"\\ud800\"}", "{\"a\":\"\\udc00\"}", "{\"a\":\"\\x\"}",
                                   "{\"a\":\"line\nbreak\"}", "{\"a\":\"\xff\"}"};
    const char *not_objects[] = {"[1]", "1", "\"text\"", "null", " true "};
    json_object_t object;

    for (size_t i = 0; i < sizeof(syntax_errors) / sizeof(syntax_errors[0]); i++) {
        CHECK(parse_text(syntax_errors[i], &object) == JSON_ERR_SYNTAX);
        CHECK(object.fields == NULL);
    }
    for (size_t i = 0; i < sizeof(not_objects) / sizeof(not_objects[0]); i++) {
        CHECK(parse_text(not_objects[i], &object) == JSON_ERR_NOT_OBJECT);
    }
    CHECK(parse_text("{\"ttlMs\":1,\"ttl\\u004ds\":2}", &object) == JSON_ERR_DUPLICATE);
    CHECK(parse_text("{\"a\":1,\"a\":1}", &object) == JSON_ERR_DUPLICATE);
}

/* 필드가 JSON_MAX_FIELDS개면 받고, 하나 더 많으면 거부한다 */
static void test_field_count_limit(void) {
    char text[2048];
    json_object_t object;
    size_t len = 0;

    text[len++] = '{';
    for (int i = 0; i <= JSON_MAX_FIELDS; i++) {
        len += (size_t)sprintf(text + len, "%s\"f%d\":0", i == 0 ? "" : ",", i);
    }
    text[len++] = '}';
    text[len] = '\0';
    CHECK(parse_text(text, &object) == JSON_ERR_TOO_MANY_FIELDS);
    CHECK(object.fields == NULL);
    len = (size_t)(strstr(text, ",\"f64\"") - text);
    text[len++] = '}';
    text[len] = '\0';
    CHECK(parse_text(text, &object) == JSON_OK);
    CHECK(object.field_count == JSON_MAX_FIELDS);
    json_object_destroy(&object);
}

static void test_deep_nesting_is_rejected(void) {
    char text[200];
    json_object_t object;
    size_t len = 0;

    len += (size_t)sprintf(text, "{\"a\":");
    for (int i = 0; i < JSON_MAX_DEPTH + 5; i++) {
        text[len++] = '[';
    }
    text[len] = '\0';
    CHECK(parse_text(text, &object) == JSON_ERR_SYNTAX);
}

static int is_integer_value(const char *number_text, int64_t expected) {
    json_field_t field = {NULL, 0, JSON_TYPE_NUMBER, (char *)number_text, strlen(number_text)};
    int64_t value = 0;

    return json_field_to_int64(&field, &value) && value == expected;
}

static int is_rejected_number(const char *number_text) {
    json_field_t field = {NULL, 0, JSON_TYPE_NUMBER, (char *)number_text, strlen(number_text)};
    int64_t value = 0;

    return !json_field_to_int64(&field, &value);
}

static void test_integer_values(void) {
    CHECK(is_integer_value("1", 1));
    CHECK(is_integer_value("1.0", 1));
    CHECK(is_integer_value("1.000000000000000000000000", 1));
    CHECK(is_integer_value("1e3", 1000));
    CHECK(is_integer_value("10.50e1", 105));
    CHECK(is_integer_value("2147483647", 2147483647));
    CHECK(is_integer_value("-5", -5));
    CHECK(is_integer_value("0", 0));
    CHECK(is_integer_value("-0.0", 0));
    CHECK(is_integer_value("100e-2", 1));
    CHECK(is_rejected_number("1.5"));
    CHECK(is_rejected_number("2147483647.00000001"));
    CHECK(is_rejected_number("1e400"));
    CHECK(is_rejected_number("1e-1"));
    CHECK(is_rejected_number("99999999999999999999"));
}

static void test_buffer_escapes(void) {
    json_buffer_t buffer = {0};

    json_buffer_append_string(&buffer, "a\"b\\c\n\x01\xea\xb0\x80", 10);
    json_buffer_append_int(&buffer, -42);
    json_buffer_append_uint(&buffer, 18446744073709551615ULL);
    CHECK(!buffer.has_error);
    CHECK(strcmp(buffer.data, "\"a\\\"b\\\\c\\n\\u0001\xea\xb0\x80\"-4218446744073709551615") == 0);
    json_buffer_destroy(&buffer);
}

int main(void) {
    test_parse_fields();
    test_nul_in_string_keeps_length();
    test_parse_errors();
    test_field_count_limit();
    test_deep_nesting_is_rejected();
    test_integer_values();
    test_buffer_escapes();

    if (failed_count > 0) {
        printf("test_json : %d개 실패\n", failed_count);
        return 1;
    }
    printf("test_json : 모두 통과\n");
    return 0;
}
