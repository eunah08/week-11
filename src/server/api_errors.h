#ifndef API_ERRORS_H
#define API_ERRORS_H

#include "store.h"

/* store_status_t와 에러 코드·상태 코드의 대응을 한 곳에 모은다. (협약 11.3) */

typedef struct {
    int http_status;
    const char *code;    /* 예 : "INVALID_TTL" */
} api_error_t;

api_error_t api_errors_from_store_status(store_status_t status);

#endif
