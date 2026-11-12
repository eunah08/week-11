#include "handlers.h"
#include "router.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool should_fail;
void *test_malloc(size_t size) {
    if (should_fail) { should_fail = false; return NULL; }
    return malloc(size);
}
void *test_calloc(size_t count, size_t size) { return calloc(count, size); }
char *test_strdup(const char *value) { return strdup(value); }
void test_free(void *pointer) { free(pointer); }

int main(void) {
    server_context_t server = {store_create(NULL), PTHREAD_MUTEX_INITIALIZER};
    assert(server.store != NULL);
    http_request_t request = {"GET", "/keys/product:42", NULL, 0};
    http_response_t response = {0};
    should_fail = true;
    router_dispatch(&request, &response, &server);
    assert(response.status == 500);
    assert(response.body != NULL && strstr(response.body, "INTERNAL_ERROR") != NULL);
    free(response.body);
    store_destroy(server.store); pthread_mutex_destroy(&server.mutex);
    puts("test_http_allocation: key decoding OOM maps to 500 OK");
    return 0;
}
