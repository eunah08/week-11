#include "mini_redis_client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "http.h"
#include "json.h"

#define KEYS_PATH_PREFIX "/keys/"
#define REQUEST_HEAD_BYTES 1024
#define SET_BODY_WRAPPER_BYTES 64
#define MAX_RESPONSE_BYTES (8 * 1024 * 1024)
#define INITIAL_RESPONSE_BYTES 4096
#define HEADER_END "\r\n\r\n"
#define HTTP_STATUS_OK 200
#define HTTP_STATUS_NOT_FOUND 404

typedef struct {
    int http_status;
    char *text;          /* 응답 전체 원문. 해제는 destroy_response */
    const char *body;    /* text 안의 본문 시작 위치 */
} client_response_t;

static void destroy_response(client_response_t *response) {
    free(response->text);
    response->text = NULL;
}

static int connect_to_server(const mini_redis_client_t *client) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    http_set_timeout(fd, MINI_REDIS_TIMEOUT_MS);

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)client->port);
    if (inet_pton(AF_INET, client->host, &address.sin_addr) != 1
        || connect(fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* 버퍼가 차면 두 배로 늘린다. 상한을 넘으면 false */
static bool grow_buffer(char **buffer, size_t *buffer_size) {
    if (*buffer_size >= MAX_RESPONSE_BYTES) {
        return false;
    }
    size_t new_size = *buffer_size * 2;
    char *new_buffer = realloc(*buffer, new_size + 1);
    if (new_buffer == NULL) {
        return false;
    }
    *buffer = new_buffer;
    *buffer_size = new_size;
    return true;
}

/* 서버가 연결을 닫을 때까지 모두 읽는다 (서버는 Connection: close). 해제 책임 : 호출한 쪽이 free() */
static char *read_all(int fd) {
    size_t text_len = 0;
    size_t buffer_size = INITIAL_RESPONSE_BYTES;
    char *text = malloc(buffer_size + 1);

    while (text != NULL) {
        if (text_len == buffer_size && !grow_buffer(&text, &buffer_size)) {
            break;
        }
        ssize_t read_len = read(fd, text + text_len, buffer_size - text_len);
        if (read_len == 0) {
            text[text_len] = '\0';
            return text;
        }
        if (read_len < 0) {
            break;
        }
        text_len += (size_t)read_len;
    }
    free(text);
    return NULL;
}

/* "HTTP/1.1 200 OK ... \r\n\r\n본문"을 상태 코드와 본문으로 나눈다 */
static bool parse_response(char *text, client_response_t *out_response) {
    int http_status = 0;
    char *body = strstr(text, HEADER_END);
    if (body == NULL || sscanf(text, "HTTP/1.%*d %d", &http_status) != 1) {
        return false;
    }
    out_response->http_status = http_status;
    out_response->text = text;
    out_response->body = body + strlen(HEADER_END);
    return true;
}

/* 요청 줄, 헤더, 본문을 보낸다. body가 NULL이면 본문 없이 보낸다 */
static bool send_request(int fd, const char *method, const char *path, const char *body) {
    char head[REQUEST_HEAD_BYTES];
    size_t body_len = body != NULL ? strlen(body) : 0;
    int head_len = snprintf(head, sizeof(head),
                            "%s %s HTTP/1.1\r\n"
                            "Host: mini-redis\r\n"
                            "Content-Type: application/json\r\n"
                            "Content-Length: %zu\r\n"
                            "Connection: close\r\n\r\n",
                            method, path, body_len);
    if (head_len < 0 || (size_t)head_len >= sizeof(head)) {
        return false;
    }
    return http_write_all(fd, head, (size_t)head_len)
           && (body == NULL || http_write_all(fd, body, body_len));
}

/* "/keys/{인코딩한 key}" 경로를 만든다. 해제 책임 : 호출한 쪽이 free() */
static char *create_key_path(const char *key) {
    char *encoded_key = http_encode_url(key);
    if (encoded_key == NULL) {
        return NULL;
    }
    char *path = malloc(strlen(KEYS_PATH_PREFIX) + strlen(encoded_key) + 1);
    if (path != NULL) {
        sprintf(path, "%s%s", KEYS_PATH_PREFIX, encoded_key);
    }
    free(encoded_key);
    return path;
}

/*
 * /keys/{key} 에 요청을 보내고 응답을 받는다. body가 NULL이면 본문 없이 보낸다.
 * 해제 책임 : 성공하면 호출한 쪽이 destroy_response
 */
static bool request_key(const mini_redis_client_t *client, const char *method, const char *key,
                        const char *body, client_response_t *out_response) {
    char *path = create_key_path(key);
    if (path == NULL) {
        return false;
    }
    int fd = connect_to_server(client);
    if (fd < 0) {
        free(path);
        return false;
    }

    char *text = NULL;
    if (send_request(fd, method, path, body)) {
        text = read_all(fd);
    }
    close(fd);
    free(path);

    if (text == NULL) {
        return false;
    }
    if (!parse_response(text, out_response)) {
        free(text);
        return false;
    }
    return true;
}

/* {"value": "..."} 본문에서 값 문자열을 꺼낸다. 해제 책임 : 호출한 쪽이 free() */
static char *read_value_field(const char *body) {
    json_object_t object;
    if (json_parse_object(body, strlen(body), &object) != JSON_PARSE_OK) {
        return NULL;
    }
    const json_field_t *value_field = json_find_field(&object, "value");
    char *value = NULL;
    if (value_field != NULL && value_field->type == JSON_TYPE_STRING) {
        value = strdup(value_field->string_value);
    }
    json_object_destroy(&object);
    return value;
}

mini_redis_client_status_t mini_redis_client_get(const mini_redis_client_t *client,
                                                 const char *key, char **out_value) {
    client_response_t response;
    *out_value = NULL;
    if (!request_key(client, "GET", key, NULL, &response)) {
        return MINI_REDIS_CLIENT_ERROR;
    }

    mini_redis_client_status_t status = MINI_REDIS_CLIENT_ERROR;
    if (response.http_status == HTTP_STATUS_NOT_FOUND) {
        status = MINI_REDIS_CLIENT_NOT_FOUND;
    } else if (response.http_status == HTTP_STATUS_OK) {
        *out_value = read_value_field(response.body);
        status = *out_value != NULL ? MINI_REDIS_CLIENT_OK : MINI_REDIS_CLIENT_ERROR;
    }
    destroy_response(&response);
    return status;
}

mini_redis_client_status_t mini_redis_client_set(const mini_redis_client_t *client,
                                                 const char *key, const char *value,
                                                 int64_t ttl_ms) {
    char *escaped_value = json_escape_string(value);
    if (escaped_value == NULL) {
        return MINI_REDIS_CLIENT_ERROR;
    }
    size_t body_size = strlen(escaped_value) + SET_BODY_WRAPPER_BYTES;
    char *body = malloc(body_size);
    if (body == NULL) {
        free(escaped_value);
        return MINI_REDIS_CLIENT_ERROR;
    }
    snprintf(body, body_size, "{\"value\": %s, \"ttlMs\": %lld}", escaped_value,
             (long long)ttl_ms);
    free(escaped_value);

    client_response_t response;
    bool is_sent = request_key(client, "PUT", key, body, &response);
    free(body);
    if (!is_sent) {
        return MINI_REDIS_CLIENT_ERROR;
    }
    bool is_ok = response.http_status == HTTP_STATUS_OK;
    destroy_response(&response);
    return is_ok ? MINI_REDIS_CLIENT_OK : MINI_REDIS_CLIENT_ERROR;
}

mini_redis_client_status_t mini_redis_client_delete(const mini_redis_client_t *client,
                                                    const char *key) {
    client_response_t response;
    if (!request_key(client, "DELETE", key, NULL, &response)) {
        return MINI_REDIS_CLIENT_ERROR;
    }
    bool is_ok = response.http_status == HTTP_STATUS_OK;
    destroy_response(&response);
    return is_ok ? MINI_REDIS_CLIENT_OK : MINI_REDIS_CLIENT_ERROR;
}
