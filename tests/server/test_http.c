#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "http.h"

/* HTTP 요청 읽기와 퍼센트 디코딩 단위 테스트 (협약 11.1, 11.5). 소켓 대신 socketpair를 쓴다 */

static int failed_count = 0;

#define CHECK(condition)                                                          \
    do {                                                                          \
        if (!(condition)) {                                                       \
            fprintf(stderr, "  실패 %s:%d %s\n", __FILE__, __LINE__, #condition); \
            failed_count++;                                                       \
        }                                                                         \
    } while (0)

/* raw를 한쪽 끝에 쓰고 닫은 뒤, 다른 쪽에서 요청을 읽는다 */
static http_read_status_t read_raw(const char *raw, size_t raw_len, http_request_t *out_request) {
    int fds[2];
    http_read_status_t status = HTTP_READ_CLOSED;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        return HTTP_READ_CLOSED;
    }
    if (write(fds[1], raw, raw_len) == (ssize_t)raw_len) {
        shutdown(fds[1], SHUT_WR);
        status = http_read_request(fds[0], out_request);
    }
    close(fds[0]);
    close(fds[1]);
    return status;
}

static http_read_status_t read_text(const char *raw, http_request_t *out_request) {
    return read_raw(raw, strlen(raw), out_request);
}

static void test_read_request_with_body(void) {
    http_request_t request;
    const char *raw = "PUT /keys/a%20b?x=1 HTTP/1.1\r\nHost: test\r\ncontent-length:  5 \r\n\r\nhello";

    CHECK(read_text(raw, &request) == HTTP_READ_OK);
    CHECK(strcmp(request.method, "PUT") == 0);
    CHECK(strcmp(request.path, "/keys/a%20b?x=1") == 0);
    CHECK(request.body_len == 5 && strcmp(request.body, "hello") == 0);
    http_request_destroy(&request);
}

static void test_read_request_without_body(void) {
    http_request_t request;

    CHECK(read_text("GET /stats HTTP/1.0\r\n\r\n", &request) == HTTP_READ_OK);
    CHECK(request.body == NULL && request.body_len == 0);
    http_request_destroy(&request);
}

static void test_read_errors(void) {
    http_request_t request;
    char huge_header[MAX_HEADER_BYTES + 100];

    CHECK(read_text("GET /a HTTP/1.1\r\nContent-Length: 10\r\n\r\nshort", &request) ==
          HTTP_READ_CLOSED);
    CHECK(read_text("GET /a HTTP/1.1\r\n", &request) == HTTP_READ_CLOSED);
    CHECK(read_text("HELLO\r\n\r\n", &request) == HTTP_READ_BAD_REQUEST);
    CHECK(read_text("GET noslash HTTP/1.1\r\n\r\n", &request) == HTTP_READ_BAD_REQUEST);
    CHECK(read_text("GET /a HTTP/1.1\r\nbroken header\r\n\r\n", &request) == HTTP_READ_BAD_REQUEST);
    CHECK(read_text("GET /a HTTP/1.1\r\nContent-Length: x1\r\n\r\n", &request) ==
          HTTP_READ_BAD_REQUEST);
    CHECK(read_text("PUT /a HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n", &request) ==
          HTTP_READ_UNSUPPORTED_BODY);
    http_request_destroy(&request);
    CHECK(read_text("PUT /a HTTP/1.1\r\nContent-Length: 99999999999999999999\r\n\r\n", &request) ==
          HTTP_READ_TOO_LARGE);
    http_request_destroy(&request);
    CHECK(read_text("PUT /a HTTP/1.1\r\nContent-Length: 2097153\r\n\r\n", &request) ==
          HTTP_READ_TOO_LARGE);
    http_request_destroy(&request);
    memset(huge_header, 'a', sizeof(huge_header));
    memcpy(huge_header, "GET /a HTTP/1.1\r\nX: ", 20);
    CHECK(read_raw(huge_header, sizeof(huge_header), &request) == HTTP_READ_BAD_REQUEST);
}

static void test_header_with_nul_byte(void) {
    http_request_t request;
    const char raw[] = "GET /a HTTP/1.1\r\nX: a\0b\r\n\r\n";

    CHECK(read_raw(raw, sizeof(raw) - 1, &request) == HTTP_READ_OK);
    http_request_destroy(&request);
}

static int decodes_to(const char *encoded, const char *expected, size_t expected_len) {
    char *decoded = NULL;
    size_t decoded_len = 0;
    int is_same = 0;

    if (http_decode_percent(encoded, strlen(encoded), &decoded, &decoded_len) == HTTP_DECODE_OK) {
        is_same = decoded_len == expected_len && memcmp(decoded, expected, expected_len) == 0;
        free(decoded);
    }
    return is_same;
}

static int is_bad_encoding(const char *encoded) {
    char *decoded = NULL;
    size_t decoded_len = 0;

    return http_decode_percent(encoded, strlen(encoded), &decoded, &decoded_len) ==
           HTTP_DECODE_INVALID;
}

static void test_percent_decoding(void) {
    CHECK(decodes_to("product%3A42", "product:42", 10));
    CHECK(decodes_to("%ED%95%9C", "\xed\x95\x9c", 3));
    CHECK(decodes_to("a%2Fb%3fc", "a/b?c", 5));
    CHECK(decodes_to("a+b", "a+b", 3));
    CHECK(decodes_to("a%00b", "a\0b", 3));
    CHECK(is_bad_encoding("%zz"));
    CHECK(is_bad_encoding("abc%"));
    CHECK(is_bad_encoding("abc%4"));
}

int main(void) {
    test_read_request_with_body();
    test_read_request_without_body();
    test_read_errors();
    test_header_with_nul_byte();
    test_percent_decoding();

    if (failed_count > 0) {
        printf("test_http : %d개 실패\n", failed_count);
        return 1;
    }
    printf("test_http : 모두 통과\n");
    return 0;
}
