#include "api_errors.h"

api_error_t api_errors_from_store_status(store_status_t status) {
    switch (status) {
    case STORE_OK:
        return (api_error_t){200, NULL};
    case STORE_NOT_FOUND:
        return (api_error_t){404, "KEY_NOT_FOUND"};
    case STORE_ERR_INVALID_KEY:
        return (api_error_t){400, "INVALID_KEY"};
    case STORE_ERR_INVALID_VALUE:
        return (api_error_t){400, "INVALID_VALUE"};
    case STORE_ERR_VALUE_TOO_LARGE:
        return (api_error_t){413, "VALUE_TOO_LARGE"};
    case STORE_ERR_INVALID_TTL:
        return (api_error_t){400, "INVALID_TTL"};
    case STORE_ERR_INVALID_PREFIX:
        return (api_error_t){400, "INVALID_PREFIX"};
    case STORE_ERR_NO_MEMORY:
        return (api_error_t){500, "INTERNAL_ERROR"};
    }
    return (api_error_t){500, "INTERNAL_ERROR"};
}
