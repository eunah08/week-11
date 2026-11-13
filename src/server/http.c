#include "http.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define HEADER_END "\r\n\r\n"
#define HEADER_END_LEN 4
#define LINE_END "\r\n"
#define LINE_END_LEN 2
#define STATUS_LINE_BYTES 256
#define DRAIN_LIMIT_BYTES (8 * 1024 * 1024)
#define DRAIN_TIMEOUT_MS 1000          /* 닫기 전에 남은 본문을 버리는 전체 시간 */
#define DRAIN_CHUNK_BYTES 4096
#define MS_PER_SECOND 1000
#define US_PER_MS 1000
#define NS_PER_MS 1000000
#define DECIMAL_BASE 10

/*
 * 소켓 하나와 "이 시각까지만 읽는다"는 전체 마감 시각.
 * SO_RCVTIMEO는 recv 한 번에만 걸리므로, 1바이트씩 천천히 보내는 상대가 워커를 오래 붙잡지 못하게
 * 요청 전체에 마감을 둔다. (협약 11.5, 12.1 서버가 멈추지 않음)
 */
typedef struct {
    int fd;
    uint64_t deadline_ms;    /* 단조 시계 기준 */
} socket_reader_t;

/* 헤더를 읽으며 알아낸 본문 정보 */
typedef struct {
    size_t content_len;
    bool is_chunked;
} body_info_t;

/* 버퍼에서 "\r\n\r\n"을 찾는다. 없으면 NULL */
static const char *find_header_end(const char *buffer, size_t buffer_len) {
    for (size_t i = 0; i + HEADER_END_LEN <= buffer_len; i++) {
        if (memcmp(buffer + i, HEADER_END, HEADER_END_LEN) == 0) {
            return buffer + i;
        }
    }
    return NULL;
}

static uint64_t now_monotonic_ms(void) {
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * MS_PER_SECOND + (uint64_t)now.tv_nsec / NS_PER_MS;
}

static socket_reader_t create_reader(int fd, uint64_t timeout_ms) {
    socket_reader_t reader = {fd, now_monotonic_ms() + timeout_ms};

    return reader;
}

/*
 * 마감까지 남은 시간만큼만 기다리며 읽는다. 마감이 지났거나 시간 초과(EAGAIN), 끊김(0)은 모두
 * "더 못 읽음"(0 이하)으로 본다. 신호로 끊긴 recv는 다시 부른다.
 */
static ssize_t receive_some(const socket_reader_t *reader, char *buffer, size_t buffer_len) {
    ssize_t received = 0;

    do {
        uint64_t now_ms = now_monotonic_ms();
        if (now_ms >= reader->deadline_ms) {
            return 0;
        }
        uint64_t remaining_ms = reader->deadline_ms - now_ms;
        struct timeval timeout = {(time_t)(remaining_ms / MS_PER_SECOND),
                                  (suseconds_t)((remaining_ms % MS_PER_SECOND) * US_PER_MS)};
        setsockopt(reader->fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        received = recv(reader->fd, buffer, buffer_len, 0);
    } while (received < 0 && errno == EINTR);
    return received;
}

/* 헤더 끝까지 읽는다. 반환 : 버퍼에 읽은 전체 길이, 실패면 0. *out_header_len은 헤더 끝 위치 */
static size_t read_header(const socket_reader_t *reader, char *buffer, size_t *out_header_len,
                          http_read_status_t *out_status) {
    size_t buffer_len = 0;
    const char *header_end = NULL;

    while (header_end == NULL) {
        if (buffer_len >= MAX_HEADER_BYTES) {
            *out_status = HTTP_READ_BAD_REQUEST;
            return 0;
        }
        ssize_t received = receive_some(reader, buffer + buffer_len,
                                         MAX_HEADER_BYTES - buffer_len);
        if (received <= 0) {
            *out_status = HTTP_READ_CLOSED;
            return 0;
        }
        size_t search_from = buffer_len >= HEADER_END_LEN ? buffer_len - HEADER_END_LEN + 1 : 0;
        buffer_len += (size_t)received;
        header_end = find_header_end(buffer + search_from, buffer_len - search_from);
    }
    *out_header_len = (size_t)(header_end - buffer) + HEADER_END_LEN;
    return buffer_len;
}

/* "METHOD SP target SP HTTP/1.x" 를 읽는다 */
static bool parse_request_line(const char *line, size_t line_len, http_request_t *request) {
    const char *method_end = memchr(line, ' ', line_len);
    const char *target_start = NULL;
    const char *target_end = NULL;

    if (method_end == NULL || method_end == line ||
        (size_t)(method_end - line) >= HTTP_METHOD_BYTES) {
        return false;
    }
    target_start = method_end + 1;
    target_end = memchr(target_start, ' ', line_len - (size_t)(target_start - line));
    if (target_end == NULL || target_end == target_start || *target_start != '/' ||
        strncmp(target_end + 1, "HTTP/1.", strlen("HTTP/1.")) != 0) {
        return false;
    }
    memcpy(request->method, line, (size_t)(method_end - line));
    request->method[method_end - line] = '\0';
    request->path = malloc((size_t)(target_end - target_start) + 1);
    if (request->path == NULL) {
        return false;
    }
    memcpy(request->path, target_start, (size_t)(target_end - target_start));
    request->path[target_end - target_start] = '\0';
    return true;
}

static bool parse_content_len(const char *value, size_t value_len, size_t *out_len) {
    size_t content_len = 0;

    if (value_len == 0) {
        return false;
    }
    for (size_t i = 0; i < value_len; i++) {
        if (value[i] < '0' || value[i] > '9') {
            return false;
        }
        if (content_len > (MAX_BODY_BYTES + 1) / DECIMAL_BASE) {
            content_len = MAX_BODY_BYTES + 1;    /* 너무 큰 값은 "한도 초과"로만 기억한다 */
            continue;
        }
        content_len = content_len * DECIMAL_BASE + (size_t)(value[i] - '0');
    }
    *out_len = content_len;
    return true;
}

/* 헤더 한 줄 "이름: 값"에서 Content-Length와 Transfer-Encoding만 본다 */
static bool parse_header_line(const char *line, size_t line_len, body_info_t *body_info) {
    const char *colon = memchr(line, ':', line_len);
    const char *value = NULL;
    size_t name_len = 0;
    size_t value_len = 0;

    if (colon == NULL) {
        return false;
    }
    name_len = (size_t)(colon - line);
    value = colon + 1;
    value_len = line_len - name_len - 1;
    while (value_len > 0 && (*value == ' ' || *value == '\t')) {
        value++;
        value_len--;
    }
    while (value_len > 0 && (value[value_len - 1] == ' ' || value[value_len - 1] == '\t')) {
        value_len--;
    }
    if (name_len == strlen("Content-Length") && strncasecmp(line, "Content-Length", name_len) == 0) {
        return parse_content_len(value, value_len, &body_info->content_len);
    }
    if (name_len == strlen("Transfer-Encoding") &&
        strncasecmp(line, "Transfer-Encoding", name_len) == 0) {
        body_info->is_chunked = true;
    }
    return true;
}

/*
 * line부터 "\r\n" 앞까지의 길이. end에는 항상 "\r\n"이 있으므로 end를 넘지 않는다.
 * strstr을 쓰지 않는 이유 : 헤더에 '\0'이 섞이면 strstr이 일찍 멈춰 NULL을 돌려준다
 */
static size_t find_line_len(const char *line, const char *end) {
    const char *current = line;

    while (current < end && memcmp(current, LINE_END, LINE_END_LEN) != 0) {
        current++;
    }
    return (size_t)(current - line);
}

/* 요청 줄과 헤더 줄들을 차례로 읽는다. header_len은 마지막 빈 줄까지 포함한 길이 */
static http_read_status_t parse_header(const char *buffer, size_t header_len,
                                       http_request_t *request, body_info_t *body_info) {
    const char *line = buffer;
    const char *end = buffer + header_len - LINE_END_LEN;    /* 마지막 빈 줄 앞 */
    bool is_first_line = true;

    while (line < end) {
        size_t line_len = find_line_len(line, end);
        const char *line_end = line + line_len;
        bool is_ok = is_first_line ? parse_request_line(line, line_len, request)
                                   : parse_header_line(line, line_len, body_info);
        if (!is_ok) {
            /* 요청 줄에서 method는 썼는데 path를 못 받았으면 메모리 부족이다 */
            bool is_no_memory = is_first_line && request->method[0] != '\0' && request->path == NULL;
            return is_no_memory ? HTTP_READ_NO_MEMORY : HTTP_READ_BAD_REQUEST;
        }
        is_first_line = false;
        line = line_end + LINE_END_LEN;
    }
    return is_first_line ? HTTP_READ_BAD_REQUEST : HTTP_READ_OK;
}

/* 헤더 뒤에 이미 받은 부분(already_len)을 이어 붙이고 나머지 본문을 읽는다 */
static http_read_status_t read_body(const socket_reader_t *reader, const char *already, size_t already_len,
                                    size_t content_len, http_request_t *request) {
    size_t body_len = already_len < content_len ? already_len : content_len;

    request->body = malloc(content_len + 1);
    if (request->body == NULL) {
        return HTTP_READ_NO_MEMORY;
    }
    memcpy(request->body, already, body_len);
    while (body_len < content_len) {
        ssize_t received = receive_some(reader, request->body + body_len,
                                         content_len - body_len);
        if (received <= 0) {
            return HTTP_READ_CLOSED;
        }
        body_len += (size_t)received;
    }
    request->body[content_len] = '\0';
    request->body_len = content_len;
    return HTTP_READ_OK;
}

static http_read_status_t check_body_info(const body_info_t *body_info) {
    if (body_info->is_chunked) {
        return HTTP_READ_UNSUPPORTED_BODY;
    }
    if (body_info->content_len > MAX_BODY_BYTES) {
        return HTTP_READ_TOO_LARGE;
    }
    return HTTP_READ_OK;
}

http_read_status_t http_read_request(int fd, http_request_t *out_request) {
    char buffer[MAX_HEADER_BYTES + 1];
    size_t header_len = 0;
    body_info_t body_info = {0, false};
    http_read_status_t status = HTTP_READ_OK;
    size_t buffer_len = 0;
    socket_reader_t reader = create_reader(fd, HTTP_REQUEST_TIMEOUT_MS);

    memset(out_request, 0, sizeof(*out_request));
    buffer_len = read_header(&reader, buffer, &header_len, &status);
    if (buffer_len == 0) {
        return status;
    }
    buffer[buffer_len] = '\0';
    status = parse_header(buffer, header_len, out_request, &body_info);
    if (status == HTTP_READ_OK) {
        status = check_body_info(&body_info);
    }
    if (status == HTTP_READ_OK && body_info.content_len > 0) {
        status = read_body(&reader, buffer + header_len, buffer_len - header_len,
                           body_info.content_len, out_request);
    }
    /* 413, chunked는 응답에 경로가 필요 없지만 path는 남겨 둬도 된다. 그 밖의 실패는 비운다 */
    if (status != HTTP_READ_OK && status != HTTP_READ_TOO_LARGE &&
        status != HTTP_READ_UNSUPPORTED_BODY) {
        http_request_destroy(out_request);
    }
    return status;
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

static bool send_all(int fd, const char *data, size_t data_len) {
    size_t sent_len = 0;

    while (sent_len < data_len) {
        ssize_t sent = send(fd, data + sent_len, data_len - sent_len, MSG_NOSIGNAL);
        if (sent < 0 && errno == EINTR) {
            continue;
        }
        if (sent <= 0) {
            return false;
        }
        sent_len += (size_t)sent;
    }
    return true;
}

/* 상태 줄 + 공통 헤더 + 추가 헤더 + 빈 줄. 반환 : 길이, 버퍼가 모자라면 -1 */
static int format_header(const http_response_t *response, char *header, size_t header_bytes) {
    const char *extra_headers = response->extra_headers != NULL ? response->extra_headers : "";
    int header_len = snprintf(header, header_bytes,
                              "HTTP/1.1 %d %s\r\n"
                              "Content-Type: application/json; charset=utf-8\r\n"
                              "Content-Length: %zu\r\n"
                              "Connection: close\r\n%s\r\n",
                              response->status_code,
                              http_find_status_text(response->status_code), response->body_len,
                              extra_headers);

    return (header_len < 0 || (size_t)header_len >= header_bytes) ? -1 : header_len;
}

/*
 * 헤더와 본문을 한 버퍼로 모아 한 번에 보낸다. 나눠 보내면 상대가 recv 한 번에 헤더만 받고
 * 본문을 놓칠 수 있고, 작은 패킷이 여러 개 생긴다. 모을 메모리가 없으면 나눠서 보낸다.
 */
bool http_write_response(int fd, const http_response_t *response) {
    char header[STATUS_LINE_BYTES];
    int header_len = format_header(response, header, sizeof(header));
    char *whole = NULL;
    bool is_sent = false;

    if (header_len < 0) {
        return false;
    }
    whole = malloc((size_t)header_len + response->body_len);
    if (whole == NULL) {
        return send_all(fd, header, (size_t)header_len) &&
               send_all(fd, response->body, response->body_len);
    }
    memcpy(whole, header, (size_t)header_len);
    if (response->body_len > 0) {
        memcpy(whole + header_len, response->body, response->body_len);
    }
    is_sent = send_all(fd, whole, (size_t)header_len + response->body_len);
    free(whole);
    return is_sent;
}

void http_response_destroy(http_response_t *response) {
    if (response == NULL) {
        return;
    }
    free(response->body);
    response->body = NULL;
    response->body_len = 0;
}

void http_close_connection(int fd) {
    socket_reader_t reader = create_reader(fd, DRAIN_TIMEOUT_MS);
    char discard[DRAIN_CHUNK_BYTES];
    size_t drained_len = 0;

    shutdown(fd, SHUT_WR);
    while (drained_len < DRAIN_LIMIT_BYTES) {
        ssize_t received = receive_some(&reader, discard, sizeof(discard));
        if (received <= 0) {
            break;
        }
        drained_len += (size_t)received;
    }
    close(fd);
}

static int parse_hex_digit(char digit) {
    if (digit >= '0' && digit <= '9') {
        return digit - '0';
    }
    if (digit >= 'a' && digit <= 'f') {
        return digit - 'a' + 10;
    }
    if (digit >= 'A' && digit <= 'F') {
        return digit - 'A' + 10;
    }
    return -1;
}

http_decode_status_t http_decode_percent(const char *text, size_t text_len, char **out_text,
                                         size_t *out_len) {
    char *decoded = malloc(text_len + 1);
    size_t decoded_len = 0;

    if (decoded == NULL) {
        return HTTP_DECODE_NO_MEMORY;
    }
    for (size_t i = 0; i < text_len; i++) {
        if (text[i] != '%') {
            decoded[decoded_len++] = text[i];
            continue;
        }
        int high = (i + 2 < text_len) ? parse_hex_digit(text[i + 1]) : -1;
        int low = (high >= 0) ? parse_hex_digit(text[i + 2]) : -1;
        if (low < 0) {
            free(decoded);
            return HTTP_DECODE_INVALID;
        }
        decoded[decoded_len++] = (char)(high * 16 + low);
        i += 2;
    }
    decoded[decoded_len] = '\0';
    *out_text = decoded;
    *out_len = decoded_len;
    return HTTP_DECODE_OK;
}

const char *http_find_status_text(int status_code) {
    switch (status_code) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 409: return "Conflict";
    case 413: return "Payload Too Large";
    case 500: return "Internal Server Error";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    default: return "Unknown";
    }
}
