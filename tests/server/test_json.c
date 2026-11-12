#include "json.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_valid_json(void) {
    const char *valid[] = {
        "{}", "{\"value\":\"한글😀\\\"\\n\"}", "{\"a\":1.0,\"b\":-2e+2}",
        "{\"value\":\"\\ud83d\\ude00\"}", "{\"x\":[null,true,false,{\"a\":0}]}"
    };
    for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) {
        json_object_t object;
        assert(json_parse_object(valid[i], strlen(valid[i]), &object) == 0);
        assert(object.is_object); json_object_destroy(&object);
    }
}

static void test_invalid_json(void) {
    const char *invalid[] = {
        "", "{", "{\"a\":01}", "{\"a\":+1}", "{\"a\":1.}", "{\"a\":.1}",
        "{\"a\":1e}", "{\"a\":NaN}", "{\"a\":Infinity}", "{\"a\":true,}",
        "{\"a\":\"\\ud800\"}", "{\"a\":\"\\udfff\"}", "{\"a\":\"\\x20\"}",
        "{\"a\":\"\n\"}", "{}{}", "{\"a\":[1,]}", "{\"a\":\"\xc0\x80\"}"
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        json_object_t object;
        assert(json_parse_object(invalid[i], strlen(invalid[i]), &object) == -1);
    }
}

static void test_escaping(void) {
    json_object_t object;
    const char *duplicate = "{\"ttlMs\":1,\"ttl\\u004ds\":2}";
    assert(json_parse_object(duplicate, strlen(duplicate), &object) == 0);
    assert(object.has_duplicate); json_object_destroy(&object);
    const char *with_nul = "{\"value\":\"a\\u0000b\"}";
    assert(json_parse_object(with_nul, strlen(with_nul), &object) == 0);
    assert(object.fields[0].string_len == 3); json_object_destroy(&object);
    char *quoted = json_quote("a\n\t\"\\😀");
    assert(quoted != NULL);
    assert(strcmp(quoted, "\"a\\u000a\\u0009\\\"\\\\😀\"") == 0);
    free(quoted);
}

int main(void) {
    test_valid_json(); test_invalid_json(); test_escaping();
    puts("test_json: syntax, UTF-8, escapes, duplicates OK");
    return 0;
}
