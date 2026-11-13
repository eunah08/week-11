#include "mini_redis_client.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "json.h"

/* 1MiB 값이 모두 제어 문자면 \u00XX(6바이트)로 바뀌어 약 6MiB가 된다. 여유를 둔다 */
#define MAX_RESPONSE_BYTES (8 * 1024 * 1024)
#define READ_CHUNK_BYTES 8192
#define HEADER_END "\r\n\r\n"
#define HEADER_END_LEN 4
#define STATUS_CODE_OFFSET 9    /* "HTTP/1.1 " 다음 */
#define MS_PER_SECOND 1000
#define US_PER_MS 1000
#define HTTP_OK 200
#define HTTP_NOT_FOUND 404

typedef struct {
    const char *method;
    const char *key;
    const char *body;     /* 없으면 NULL */
    size_t body_len;
} client_request_t;

typedef struct {
    int status_code;
    json_buffer_t raw;    /* 응답 원문 전체 */
    size_t body_offset;   /* raw.data 안에서 본문이 시작하는 위치 */
} client_response_t;

static void set_timeouts(int fd, int timeout_ms) {
    struct timeval timeout = {timeout_ms / MS_PER_SECOND, (timeout_ms % MS_PER_SECOND) * US_PER_MS};

    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

/* 연결에 실패하면 -1. SO_SNDTIMEO가 connect에도 걸려서 응답 없는 서버를 오래 기다리지 않는다 */
static int connect_to_server(const mini_redis_client_t *client) {
    struct sockaddr_in address;
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0) {
        return -1;
    }
    set_timeouts(fd, client->timeout_ms);
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)client->port);
    if (inet_pton(AF_INET, client->host, &address.sin_addr) != 1 ||
        connect(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static bool is_unreserved(unsigned char current) {
    return (current >= 'a' && current <= 'z') || (current >= 'A' && current <= 'Z') ||
           (current >= '0' && current <= '9') || current == '-' || current == '_' ||
           current == '.' || current == '~';
}

/* "/keys/" + 퍼센트 인코딩한 키 */
static void append_key_path(json_buffer_t *buffer, const char *key) {
    char encoded[4];

    json_buffer_append_text(buffer, "/keys/");
    for (const unsigned char *current = (const unsigned char *)key; *current != '\0'; current++) {
        if (is_unreserved(*current)) {
            json_buffer_append(buffer, (const char *)current, 1);
        } else {
            snprintf(encoded, sizeof(encoded), "%%%02X", *current);
            json_buffer_append(buffer, encoded, 3);
        }
    }
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

static bool write_request(int fd, const client_request_t *request) {
    json_buffer_t header = {0};
    char length_line[64];
    bool is_sent = false;

    json_buffer_append_text(&header, request->method);
    json_buffer_append_text(&header, " ");
    append_key_path(&header, request->key);
    json_buffer_append_text(&header, " HTTP/1.1\r\nHost: mini-redis\r\nConnection: close\r\n");
    snprintf(length_line, sizeof(length_line), "Content-Length: %zu\r\n\r\n", request->body_len);
    json_buffer_append_text(&header, length_line);
    is_sent = !header.has_error && send_all(fd, header.data, header.len) &&
              (request->body_len == 0 || send_all(fd, request->body, request->body_len));
    json_buffer_destroy(&header);
    return is_sent;
}

/* 상대가 연결을 닫을 때까지 읽는다 (서버는 Connection: close) */
static bool read_all(int fd, json_buffer_t *out_raw) {
    char chunk[READ_CHUNK_BYTES];

    while (out_raw->len <= MAX_RESPONSE_BYTES) {
        ssize_t received = recv(fd, chunk, sizeof(chunk), 0);
        if (received < 0 && errno == EINTR) {
            continue;
        }
        if (received < 0) {
            return false;    /* 시간 초과 포함 */
        }
        if (received == 0) {
            return !out_raw->has_error;
        }
        json_buffer_append(out_raw, chunk, (size_t)received);
    }
    return false;
}

static bool parse_response(client_response_t *response) {
    const char *data = response->raw.data;
    const char *header_end = NULL;

    if (data == NULL || response->raw.len < STATUS_CODE_OFFSET + 3 ||
        strncmp(data, "HTTP/1.", strlen("HTTP/1.")) != 0) {
        return false;
    }
    response->status_code = atoi(data + STATUS_CODE_OFFSET);
    header_end = strstr(data, HEADER_END);
    if (header_end == NULL) {
        return false;
    }
    response->body_offset = (size_t)(header_end - data) + HEADER_END_LEN;
    return true;
}

/* 요청을 보내고 응답을 받는다. 성공하면 response->raw를 호출한 쪽이 json_buffer_destroy */
static bool send_request(const mini_redis_client_t *client, const client_request_t *request,
                         client_response_t *out_response) {
    int fd = connect_to_server(client);
    bool is_ok = false;

    memset(out_response, 0, sizeof(*out_response));
    if (fd < 0) {
        return false;
    }
    is_ok = write_request(fd, request) && read_all(fd, &out_response->raw) &&
            parse_response(out_response);
    close(fd);
    if (!is_ok) {
        json_buffer_destroy(&out_response->raw);
    }
    return is_ok;
}

/* 응답 본문 {"value": "..."}에서 값을 꺼낸다. *out_value는 호출한 쪽이 free() */
static bool read_value_field(const client_response_t *response, char **out_value) {
    json_object_t object;
    const json_field_t *field = NULL;
    const char *body = response->raw.data + response->body_offset;
    bool is_ok = false;

    if (json_parse_object(body, response->raw.len - response->body_offset, &object) != JSON_OK) {
        return false;
    }
    field = json_object_find(&object, "value");
    if (field != NULL && field->type == JSON_TYPE_STRING) {
        json_field_t *owned_field = &object.fields[field - object.fields];
        *out_value = owned_field->text;
        owned_field->text = NULL;    /* 값의 소유권을 넘겼으므로 destroy에서 해제하지 않게 */
        is_ok = true;
    }
    json_object_destroy(&object);
    return is_ok;
}

mini_redis_client_status_t mini_redis_client_get(const mini_redis_client_t *client,
                                                 const char *key, char **out_value) {
    client_request_t request = {"GET", key, NULL, 0};
    client_response_t response;
    mini_redis_client_status_t status = MINI_REDIS_CLIENT_ERROR;

    *out_value = NULL;
    if (!send_request(client, &request, &response)) {
        return MINI_REDIS_CLIENT_ERROR;
    }
    if (response.status_code == HTTP_NOT_FOUND) {
        status = MINI_REDIS_CLIENT_NOT_FOUND;
    } else if (response.status_code == HTTP_OK && read_value_field(&response, out_value)) {
        status = MINI_REDIS_CLIENT_OK;
    }
    json_buffer_destroy(&response.raw);
    return status;
}

mini_redis_client_status_t mini_redis_client_set(const mini_redis_client_t *client,
                                                 const char *key, const char *value,
                                                 int64_t ttl_ms) {
    json_buffer_t body = {0};
    client_response_t response;
    bool is_ok = false;

    json_buffer_append_text(&body, "{\"value\":");
    json_buffer_append_string(&body, value, strlen(value));
    if (ttl_ms > 0) {
        json_buffer_append_text(&body, ",\"ttlMs\":");
        json_buffer_append_int(&body, ttl_ms);
    }
    json_buffer_append_text(&body, "}");
    if (!body.has_error) {
        client_request_t request = {"PUT", key, body.data, body.len};
        is_ok = send_request(client, &request, &response);
    }
    json_buffer_destroy(&body);
    if (!is_ok) {
        return MINI_REDIS_CLIENT_ERROR;
    }
    is_ok = response.status_code == HTTP_OK;
    json_buffer_destroy(&response.raw);
    return is_ok ? MINI_REDIS_CLIENT_OK : MINI_REDIS_CLIENT_ERROR;
}

mini_redis_client_status_t mini_redis_client_delete(const mini_redis_client_t *client,
                                                    const char *key) {
    client_request_t request = {"DELETE", key, NULL, 0};
    client_response_t response;
    bool is_ok = false;

    if (!send_request(client, &request, &response)) {
        return MINI_REDIS_CLIENT_ERROR;
    }
    is_ok = response.status_code == HTTP_OK;
    json_buffer_destroy(&response.raw);
    return is_ok ? MINI_REDIS_CLIENT_OK : MINI_REDIS_CLIENT_ERROR;
}
