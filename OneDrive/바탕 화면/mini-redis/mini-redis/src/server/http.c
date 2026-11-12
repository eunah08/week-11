#include "http.h"

#include <stdlib.h>

void http_request_destroy(http_request_t *request) {
    if (request == NULL) {
        return;
    }
    free(request->path);
    request->path = NULL;
    free(request->body);
    request->body = NULL;
}
