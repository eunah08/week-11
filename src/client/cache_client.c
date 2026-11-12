#include "cache_client.h"
#include "http.h"
#include "json.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *create_path(const char *id) {
    size_t len = strlen(id);
    if (len > 248) { return NULL; }
    char *path = malloc(sizeof("/keys/product%3A") + len * 3);
    if (path == NULL) { return NULL; }
    strcpy(path, "/keys/product%3A");
    char *cursor = path + strlen(path);
    const char *hex = "0123456789ABCDEF";
    for (size_t i = 0; i < len; i++) {
        unsigned char value = (unsigned char)id[i];
        *cursor++ = '%'; *cursor++ = hex[value >> 4]; *cursor++ = hex[value & 15];
    }
    *cursor = '\0';
    return path;
}

static int request_cache(const cache_client_t *client, const char *method, const char *id,
                          const char *body, http_response_t *response) {
    char *path = create_path(id);
    if (path == NULL) { return -1; }
    http_client_request_t request = {client->port, method, path, body};
    int status = http_request(&request, response);
    free(path);
    return status;
}

char *cache_client_get(const cache_client_t *client, const char *id) {
    http_response_t response = {0};
    char *value = NULL;
    if (request_cache(client, "GET", id, NULL, &response) == 0 && response.status == 200) {
        json_object_t object;
        if (json_parse_object(response.body, strlen(response.body), &object) == 0) {
            const json_field_t *field = json_find_field(&object, "value");
            if (field != NULL && field->kind == JSON_STRING
                && strlen(field->string) == field->string_len) { value = strdup(field->string); }
            json_object_destroy(&object);
        }
    }
    free(response.body);
    return value;
}

int cache_client_set(const cache_client_t *client, const char *id, const char *value) {
    char *quoted = json_quote(value);
    if (quoted == NULL) { return -1; }
    size_t len = strlen(quoted) + 80;
    char *body = malloc(len);
    if (body == NULL) { free(quoted); return -1; }
    snprintf(body, len, "{\"value\":%s,\"ttlMs\":%" PRId64 "}", quoted, client->ttl_ms);
    http_response_t response = {0};
    int status = request_cache(client, "PUT", id, body, &response);
    free(body); free(quoted);
    bool is_success = status == 0 && response.status == 200;
    free(response.body);
    return is_success ? 0 : -1;
}

int cache_client_delete(const cache_client_t *client, const char *id) {
    http_response_t response = {0};
    int status = request_cache(client, "DELETE", id, NULL, &response);
    bool is_success = status == 0 && response.status == 200;
    free(response.body);
    return is_success ? 0 : -1;
}
