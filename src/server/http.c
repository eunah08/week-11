#include "http.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define HEADER_END "\r\n\r\n"
#define LINE_END "\r\n"
#define LINE_END_LEN 2
#define HTTP_VERSION_PREFIX "HTTP/1."
#define CONTENT_LENGTH_HEADER "Content-Length:"
#define TRANSFER_ENCODING_HEADER "Transfer-Encoding:"
#define RESPONSE_HEAD_BYTES 512
#define DECIMAL_BASE 10
#define HEX_BASE 16
#define HEX_LETTER_BASE 10
#define PERCENT_ENCODED_BYTES 3     /* %XX */
#define MS_PER_SECOND 1000
#define US_PER_MS 1000

typedef struct {
    char buffer[MAX_HEADER_BYTES + 1];
    size_t used;          /* 읽은 바이트 수. 헤더 뒤에 같이 읽힌 본문 앞부분도 들어 있다 */
    size_t header_len;    /* 빈 줄(\r\n\r\n)까지의 길이 */
} header_block_t;

/* 신호로 끊기면(EINTR) 다시 읽는다. 0은 끊김, 음수는 오류(시간 초과 포함) */
static ssize_t read_some(int fd, char *buffer, size_t buffer_len) {
    ssize_t read_len = 0;
    do {
        read_len = read(fd, buffer, buffer_len);
    } while (read_len < 0 && errno == EINTR);
    return read_len;
}

static http_read_status_t read_header_block(int fd, header_block_t *block) {
    block->used = 0;
    while (block->used < MAX_HEADER_BYTES) {
        ssize_t read_len = read_some(fd, block->buffer + block->used,
                                     MAX_HEADER_BYTES - block->used);
        if (read_len <= 0) {
            return HTTP_READ_CLOSED;
        }
        block->used += (size_t)read_len;
        block->buffer[block->used] = '\0';

        char *header_end = strstr(block->buffer, HEADER_END);
        if (header_end != NULL) {
            block->header_len = (size_t)(header_end - block->buffer) + strlen(HEADER_END);
            return HTTP_READ_OK;
        }
    }
    return HTTP_READ_BAD_REQUEST;
}

/* "METHOD PATH HTTP/1.x" 줄을 나눈다 */
static http_read_status_t parse_request_line(const char *headers, http_request_t *out_request) {
    const char *line_end = strstr(headers, LINE_END);
    const char *method_end = strchr(headers, ' ');
    if (method_end == NULL || method_end > line_end) {
        return HTTP_READ_BAD_REQUEST;
    }
    size_t method_len = (size_t)(method_end - headers);
    if (method_len == 0 || method_len >= sizeof(out_request->method)) {
        return HTTP_READ_BAD_REQUEST;
    }

    const char *path_start = method_end + 1;
    const char *path_end = strchr(path_start, ' ');
    if (path_end == NULL || path_end > line_end || path_end == path_start) {
        return HTTP_READ_BAD_REQUEST;
    }
    if (strncmp(path_end + 1, HTTP_VERSION_PREFIX, strlen(HTTP_VERSION_PREFIX)) != 0) {
        return HTTP_READ_BAD_REQUEST;
    }

    memcpy(out_request->method, headers, method_len);
    out_request->method[method_len] = '\0';
    out_request->path = strndup(path_start, (size_t)(path_end - path_start));
    if (out_request->path == NULL) {
        return HTTP_READ_NO_MEMORY;
    }
    return HTTP_READ_OK;
}

/* "Content-Length:" 뒤의 숫자를 읽는다. 상한을 넘는 순간 멈춰서 숫자가 넘치지 않게 한다 */
static http_read_status_t parse_content_length(const char *text, size_t *out_content_length) {
    size_t content_length = 0;
    size_t digit_count = 0;

    while (*text == ' ') {
        text++;
    }
    while (*text >= '0' && *text <= '9') {
        content_length = content_length * DECIMAL_BASE + (size_t)(*text - '0');
        if (content_length > MAX_BODY_BYTES) {
            return HTTP_READ_TOO_LARGE;
        }
        digit_count++;
        text++;
    }
    while (*text == ' ') {
        text++;
    }
    if (digit_count == 0 || strncmp(text, LINE_END, LINE_END_LEN) != 0) {
        return HTTP_READ_BAD_REQUEST;
    }
    *out_content_length = content_length;
    return HTTP_READ_OK;
}

/* 헤더 줄들에서 본문 길이를 찾는다. chunked 전송은 지원하지 않는다 (협약 11.5) */
static http_read_status_t parse_body_headers(const char *headers, size_t *out_content_length) {
    const char *line = strstr(headers, LINE_END) + LINE_END_LEN;
    size_t content_length_header_len = strlen(CONTENT_LENGTH_HEADER);

    *out_content_length = 0;
    while (strncmp(line, LINE_END, LINE_END_LEN) != 0) {
        if (strncasecmp(line, TRANSFER_ENCODING_HEADER, strlen(TRANSFER_ENCODING_HEADER)) == 0) {
            return HTTP_READ_BAD_REQUEST;
        }
        if (strncasecmp(line, CONTENT_LENGTH_HEADER, content_length_header_len) == 0) {
            http_read_status_t status =
                parse_content_length(line + content_length_header_len, out_content_length);
            if (status != HTTP_READ_OK) {
                return status;
            }
        }
        line = strstr(line, LINE_END) + LINE_END_LEN;
    }
    return HTTP_READ_OK;
}

/* 헤더와 같이 읽힌 본문 앞부분을 옮기고, 나머지를 content_length까지 읽는다 */
static http_read_status_t read_body(int fd, const header_block_t *block, size_t content_length,
                                    http_request_t *out_request) {
    if (content_length == 0) {
        return HTTP_READ_OK;
    }
    char *body = malloc(content_length + 1);
    if (body == NULL) {
        return HTTP_READ_NO_MEMORY;
    }

    size_t body_len = block->used - block->header_len;
    if (body_len > content_length) {
        body_len = content_length;
    }
    memcpy(body, block->buffer + block->header_len, body_len);
    while (body_len < content_length) {
        ssize_t read_len = read_some(fd, body + body_len, content_length - body_len);
        if (read_len <= 0) {
            free(body);
            return HTTP_READ_CLOSED;
        }
        body_len += (size_t)read_len;
    }
    body[body_len] = '\0';
    out_request->body = body;
    out_request->body_len = body_len;
    return HTTP_READ_OK;
}

http_read_status_t http_read_request(int fd, http_request_t *out_request) {
    header_block_t block;
    size_t content_length = 0;

    memset(out_request, 0, sizeof(*out_request));
    http_read_status_t status = read_header_block(fd, &block);
    if (status == HTTP_READ_OK) {
        status = parse_request_line(block.buffer, out_request);
    }
    if (status == HTTP_READ_OK) {
        status = parse_body_headers(block.buffer, &content_length);
    }
    if (status == HTTP_READ_OK) {
        status = read_body(fd, &block, content_length, out_request);
    }
    if (status != HTTP_READ_OK) {
        http_request_destroy(out_request);
    }
    return status;
}

static const char *get_reason_phrase(int http_status) {
    switch (http_status) {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 413:
        return "Payload Too Large";
    }
    return "Internal Server Error";
}

bool http_write_all(int fd, const char *data, size_t data_len) {
    size_t written_len = 0;
    while (written_len < data_len) {
        ssize_t write_len = write(fd, data + written_len, data_len - written_len);
        if (write_len < 0 && errno == EINTR) {
            continue;
        }
        if (write_len <= 0) {
            return false;
        }
        written_len += (size_t)write_len;
    }
    return true;
}

bool http_write_response(int fd, const http_response_t *response) {
    char head[RESPONSE_HEAD_BYTES];
    const char *body = response->body != NULL ? response->body : "";
    const char *extra_headers = response->extra_headers != NULL ? response->extra_headers : "";
    size_t body_len = strlen(body);

    int head_len = snprintf(head, sizeof(head),
                            "HTTP/1.1 %d %s\r\n"
                            "Content-Type: application/json; charset=utf-8\r\n"
                            "Content-Length: %zu\r\n"
                            "Connection: close\r\n"
                            "%s\r\n",
                            response->http_status, get_reason_phrase(response->http_status),
                            body_len, extra_headers);
    if (head_len < 0 || (size_t)head_len >= sizeof(head)) {
        return false;
    }
    return http_write_all(fd, head, (size_t)head_len) && http_write_all(fd, body, body_len);
}

void http_set_timeout(int fd, int timeout_ms) {
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / MS_PER_SECOND;
    timeout.tv_usec = (timeout_ms % MS_PER_SECOND) * US_PER_MS;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
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

/* "%XX" 하나를 바이트 값으로 바꾼다. 잘못된 모양이면 -1 */
static int decode_percent(const char *text, size_t remaining_len) {
    if (remaining_len < PERCENT_ENCODED_BYTES) {
        return -1;
    }
    int high = hex_digit_value(text[1]);
    int low = hex_digit_value(text[2]);
    if (high < 0 || low < 0) {
        return -1;
    }
    return high * HEX_BASE + low;
}

http_decode_status_t http_decode_url(const char *text, size_t text_len, char **out_decoded) {
    size_t decoded_len = 0;
    char *decoded = malloc(text_len + 1);
    *out_decoded = NULL;
    if (decoded == NULL) {
        return HTTP_DECODE_NO_MEMORY;
    }

    for (size_t i = 0; i < text_len; i++) {
        if (text[i] != '%') {
            decoded[decoded_len++] = text[i];
            continue;
        }
        int byte_value = decode_percent(text + i, text_len - i);
        if (byte_value <= 0) {   /* 0은 %00(NUL)이라 받지 않는다 (협약 7.1) */
            free(decoded);
            return HTTP_DECODE_INVALID;
        }
        decoded[decoded_len++] = (char)byte_value;
        i += PERCENT_ENCODED_BYTES - 1;
    }
    decoded[decoded_len] = '\0';
    *out_decoded = decoded;
    return HTTP_DECODE_OK;
}

static bool is_unreserved_char(char character) {
    bool is_lower = character >= 'a' && character <= 'z';
    bool is_upper = character >= 'A' && character <= 'Z';
    bool is_number = character >= '0' && character <= '9';
    return is_lower || is_upper || is_number || strchr("-._~", character) != NULL;
}

char *http_encode_url(const char *text) {
    size_t text_len = strlen(text);
    char *encoded = malloc(text_len * PERCENT_ENCODED_BYTES + 1);
    if (encoded == NULL) {
        return NULL;
    }

    size_t encoded_len = 0;
    for (size_t i = 0; i < text_len; i++) {
        if (is_unreserved_char(text[i])) {
            encoded[encoded_len++] = text[i];
        } else {
            sprintf(encoded + encoded_len, "%%%02X", (unsigned char)text[i]);
            encoded_len += PERCENT_ENCODED_BYTES;
        }
    }
    encoded[encoded_len] = '\0';
    return encoded;
}

void http_request_destroy(http_request_t *request) {
    if (request == NULL) {
        return;
    }
    free(request->path);
    request->path = NULL;
    free(request->body);
    request->body = NULL;
    request->body_len = 0;
}

void http_response_destroy(http_response_t *response) {
    if (response == NULL) {
        return;
    }
    free(response->body);
    response->body = NULL;
}
