#include "http.h"
#include "json.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>

#define HTTP_MAX_WORKERS 64
#define HTTP_BACKLOG 128
#define HTTP_POLL_MS 100

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t idle;
    size_t active_count;
    http_handler_fn handler;
    void *context;
} http_server_t;
typedef struct { int fd; http_server_t *server; } http_worker_t;
typedef struct { int fd; uint64_t deadline_ms; } http_stream_t;
static volatile sig_atomic_t is_stopping;

static uint64_t monotonic_ms(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000;
}

static ssize_t receive_until(http_stream_t *stream, char *output, size_t len) {
    uint64_t now_ms = monotonic_ms();
    if (now_ms >= stream->deadline_ms) { return -1; }
    struct pollfd connection = {stream->fd, POLLIN, 0};
    int ready = poll(&connection, 1, (int)(stream->deadline_ms - now_ms));
    if (ready < 0 && errno == EINTR) { return receive_until(stream, output, len); }
    if (ready <= 0) { return -1; }
    return recv(stream->fd, output, len, 0);
}

static void stop_server(int signal_number) {
    (void)signal_number;
    is_stopping = 1;
}

static void set_timeout(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags >= 0) { fcntl(fd, F_SETFL, flags & ~O_NONBLOCK); }
    struct timeval timeout = {HTTP_TIMEOUT_MS / 1000, (HTTP_TIMEOUT_MS % 1000) * 1000};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

static int send_all(int fd, const char *data, size_t len) {
    while (len > 0) {
        ssize_t sent = send(fd, data, len, 0);
        if (sent < 0 && errno == EINTR) { continue; }
        if (sent <= 0) { return -1; }
        data += sent; len -= (size_t)sent;
    }
    return 0;
}

static int read_headers(http_stream_t *stream, char *headers, size_t *out_len,
                          size_t *out_header_len) {
    *out_len = 0;
    while (*out_len < HTTP_MAX_HEADERS) {
        ssize_t received = receive_until(stream, headers + *out_len, HTTP_MAX_HEADERS - *out_len);
        if (received < 0 && errno == EINTR) { continue; }
        if (received <= 0) { return 400; }
        *out_len += (size_t)received;
        headers[*out_len] = '\0';
        char *end = strstr(headers, "\r\n\r\n");
        if (end != NULL) {
            *out_header_len = (size_t)(end - headers) + 4;
            return 0;
        }
        if (memchr(headers, '\0', *out_len) != NULL) { return 400; }
    }
    return 400;
}

static int parse_start(char *headers, http_request_t *request) {
    char *end = strstr(headers, "\r\n");
    if (end == NULL) { return 400; }
    *end = '\0';
    char *path = strchr(headers, ' ');
    if (path == NULL || path == headers || (size_t)(path - headers) >= sizeof(request->method)) {
        return 400;
    }
    memcpy(request->method, headers, (size_t)(path - headers));
    path++;
    char *version = strchr(path, ' ');
    if (version == NULL || path[0] != '/' || strcmp(version + 1, "HTTP/1.1") != 0) {
        return 400;
    }
    *version = '\0';
    request->path = strdup(path);
    return request->path != NULL ? 0 : 500;
}

static int parse_length(char *cursor, size_t *out_length) {
    bool has_length = false;
    *out_length = 0;
    while (*cursor != '\r') {
        char *end = strstr(cursor, "\r\n");
        char *colon = strchr(cursor, ':');
        if (end == NULL || colon == NULL || colon > end) { return 400; }
        *end = '\0'; *colon++ = '\0';
        while (*colon == ' ' || *colon == '\t') { colon++; }
        if (strcasecmp(cursor, "Transfer-Encoding") == 0) { return 400; }
        if (strcasecmp(cursor, "Content-Length") == 0) {
            if (has_length || *colon < '0' || *colon > '9') { return 400; }
            has_length = true;
            for (; *colon >= '0' && *colon <= '9'; colon++) {
                if (*out_length > MAX_BODY_BYTES) { return 413; }
                *out_length = *out_length * 10 + (size_t)(*colon - '0');
            }
            while (*colon == ' ' || *colon == '\t') { colon++; }
            if (*colon != '\0') { return 400; }
        }
        cursor = end + 2;
    }
    return *out_length > MAX_BODY_BYTES ? 413 : 0;
}

static int read_body(http_stream_t *stream, http_request_t *request,
                       const char *initial, size_t len) {
    if (len > request->body_len) { return 400; }
    request->body = malloc(request->body_len + 1);
    if (request->body == NULL) { return 500; }
    memcpy(request->body, initial, len);
    while (len < request->body_len) {
        ssize_t received = receive_until(stream, request->body + len, request->body_len - len);
        if (received < 0 && errno == EINTR) { continue; }
        if (received <= 0) { return 400; }
        len += (size_t)received;
    }
    request->body[request->body_len] = '\0';
    return 0;
}

int http_read(int fd, http_request_t *out_request) {
    char headers[HTTP_MAX_HEADERS + 1];
    size_t len, header_len;
    memset(out_request, 0, sizeof(*out_request));
    http_stream_t stream = {fd, monotonic_ms() + HTTP_TIMEOUT_MS};
    int status = read_headers(&stream, headers, &len, &header_len);
    if (status != 0) { return status; }
    char *fields = strstr(headers, "\r\n") + 2;
    status = parse_start(headers, out_request);
    if (status != 0) { return status; }
    status = parse_length(fields, &out_request->body_len);
    if (status != 0) { return status; }
    return read_body(&stream, out_request, headers + header_len, len - header_len);
}

void http_request_destroy(http_request_t *request) {
    if (request != NULL) {
        free(request->path); request->path = NULL;
        free(request->body); request->body = NULL;
    }
}

void http_response_json(http_response_t *response, int status, const char *body) {
    response->status = status;
    response->body = strdup(body);
}

void http_response_error(http_response_t *response, int status, const char *code,
                          const char *function) {
    char body[512];
    snprintf(body, sizeof(body), "{\"error\":{\"code\":\"%s\","
             "\"message\":\"[%s] 요청을 처리할 수 없습니다\"}}", code, function);
    if (status == 500) { fprintf(stderr, "[%s] INTERNAL_ERROR\n", function); }
    http_response_json(response, status, body);
}

static const char *status_reason(int status) {
    switch (status) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 413: return "Payload Too Large";
    case 503: return "Service Unavailable";
    default: return "Internal Server Error";
    }
}

int http_write(int fd, const http_response_t *response) {
    const char *body = response->body;
    int status = response->status;
    if (body == NULL) {
        status = 500;
        body = "{\"error\":{\"code\":\"INTERNAL_ERROR\","
               "\"message\":\"[http_write] 메모리가 부족합니다\"}}";
        fprintf(stderr, "[http_write] 메모리가 부족합니다\n");
    }
    char headers[1024];
    int len = snprintf(headers, sizeof(headers), "HTTP/1.1 %d %s\r\n"
                       "Content-Type: application/json; charset=utf-8\r\n"
                       "Content-Length: %zu\r\nConnection: close\r\n%s\r\n",
                       status, status_reason(status), strlen(body),
                       response->extra_headers ? response->extra_headers : "");
    if (len < 0 || (size_t)len >= sizeof(headers)) { return -1; }
    if (send_all(fd, headers, (size_t)len) != 0) { return -1; }
    return send_all(fd, body, strlen(body));
}

static void *serve_worker(void *context) {
    http_worker_t *worker = context;
    http_server_t *server = worker->server;
    http_request_t request;
    http_response_t response = {0};
    set_timeout(worker->fd);
    int status = http_read(worker->fd, &request);
    if (status == 0) { server->handler(&request, &response, server->context); }
    else {
        const char *code = status == 413 ? "PAYLOAD_TOO_LARGE"
                           : status == 500 ? "INTERNAL_ERROR" : "INVALID_BODY";
        http_response_error(&response, status, code, "http_read");
    }
    http_write(worker->fd, &response);
    http_request_destroy(&request);
    free(response.body);
    close(worker->fd);
    free(worker);
    pthread_mutex_lock(&server->mutex);
    server->active_count--;
    pthread_cond_broadcast(&server->idle);
    pthread_mutex_unlock(&server->mutex);
    return NULL;
}

static int start_worker(int fd, http_server_t *server) {
    http_worker_t *worker = malloc(sizeof(*worker));
    if (worker == NULL) { return -1; }
    *worker = (http_worker_t){fd, server};
    pthread_mutex_lock(&server->mutex);
    server->active_count++;
    pthread_mutex_unlock(&server->mutex);
    pthread_t thread;
    if (pthread_create(&thread, NULL, serve_worker, worker) != 0) {
        pthread_mutex_lock(&server->mutex);
        server->active_count--;
        pthread_mutex_unlock(&server->mutex);
        free(worker); return -1;
    }
    pthread_detach(thread);
    return 0;
}

static int create_listener(int port) {
    if (port < 1 || port > 65535) { return -1; }
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { return -1; }
    int enabled = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET; address.sin_port = htons((uint16_t)port);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if (bind(fd, (struct sockaddr *)&address, sizeof(address)) != 0
        || listen(fd, HTTP_BACKLOG) != 0 || fcntl(fd, F_SETFL, O_NONBLOCK) != 0) {
        close(fd); return -1;
    }
    return fd;
}

static void accept_requests(int fd, http_server_t *server) {
    struct pollfd listener = {fd, POLLIN, 0};
    while (!is_stopping) {
        if (poll(&listener, 1, HTTP_POLL_MS) <= 0) { continue; }
        pthread_mutex_lock(&server->mutex);
        bool has_room = server->active_count < HTTP_MAX_WORKERS;
        pthread_mutex_unlock(&server->mutex);
        if (!has_room) { continue; }
        int client_fd = accept(fd, NULL, NULL);
        if (client_fd >= 0 && start_worker(client_fd, server) != 0) {
            http_response_t response = {0};
            http_response_error(&response, 500, "INTERNAL_ERROR", "http_serve");
            set_timeout(client_fd); http_write(client_fd, &response);
            free(response.body); close(client_fd);
        }
    }
}

int http_serve(int port, http_handler_fn handler, void *context) {
    is_stopping = 0;
    struct sigaction action = {0};
    action.sa_handler = stop_server; sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL); sigaction(SIGTERM, &action, NULL);
    signal(SIGPIPE, SIG_IGN);
    int fd = create_listener(port);
    if (fd < 0) { perror("[http_serve] listen"); return -1; }
    http_server_t server = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER,
                            0, handler, context};
    accept_requests(fd, &server);
    close(fd);
    pthread_mutex_lock(&server.mutex);
    while (server.active_count != 0) { pthread_cond_wait(&server.idle, &server.mutex); }
    pthread_mutex_unlock(&server.mutex);
    pthread_cond_destroy(&server.idle); pthread_mutex_destroy(&server.mutex);
    return 0;
}

static int connect_local(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { return -1; }
    set_timeout(fd);
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET; address.sin_port = htons((uint16_t)port);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        close(fd); return -1;
    }
    return fd;
}

static int read_response(int fd, http_response_t *response) {
    char headers[HTTP_MAX_HEADERS + 1];
    size_t len, header_len;
    http_stream_t stream = {fd, monotonic_ms() + HTTP_TIMEOUT_MS};
    if (read_headers(&stream, headers, &len, &header_len) != 0) { return -1; }
    if (sscanf(headers, "HTTP/1.1 %d", &response->status) != 1) { return -1; }
    char *first_end = strstr(headers, "\r\n");
    if (first_end == NULL) { return -1; }
    *first_end = '\0';
    http_request_t request = {0};
    int status = parse_length(first_end + 2, &request.body_len);
    if (status == 0) {
        status = read_body(&stream, &request, headers + header_len, len - header_len);
    }
    if (status != 0) { free(request.body); return -1; }
    response->body = request.body;
    return 0;
}

int http_request(const http_client_request_t *request, http_response_t *out_response) {
    memset(out_response, 0, sizeof(*out_response));
    int fd = connect_local(request->port);
    if (fd < 0) { return -1; }
    char headers[HTTP_MAX_HEADERS];
    size_t len = request->body ? strlen(request->body) : 0;
    int header_len = snprintf(headers, sizeof(headers), "%s %s HTTP/1.1\r\nHost: localhost\r\n"
                              "Content-Length: %zu\r\nConnection: close\r\n\r\n",
                              request->method, request->path, len);
    int status = -1;
    if (header_len > 0 && (size_t)header_len < sizeof(headers)
        && send_all(fd, headers, (size_t)header_len) == 0
        && (len == 0 || send_all(fd, request->body, len) == 0)) {
        status = read_response(fd, out_response);
    }
    close(fd);
    return status;
}

char *http_decode_key(const char *input, size_t len) {
    errno = EINVAL;
    char *output = malloc(len + 1);
    if (output == NULL) { errno = ENOMEM; return NULL; }
    size_t length = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char value = (unsigned char)input[i];
        if (value == '%') {
            if (len - i < 3) { free(output); return NULL; }
            char digits[3] = {input[i + 1], input[i + 2], '\0'}, *end;
            if (!strchr("0123456789abcdefABCDEF", digits[0])
                || !strchr("0123456789abcdefABCDEF", digits[1])) { free(output); return NULL; }
            value = (unsigned char)strtoul(digits, &end, 16); i += 2;
        }
        if (value == 0) { free(output); return NULL; }
        output[length++] = (char)value;
    }
    output[length] = '\0';
    if (!json_is_utf8(output, length)) { free(output); return NULL; }
    return output;
}
