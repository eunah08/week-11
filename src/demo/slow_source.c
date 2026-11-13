#include "slow_source.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "json.h"

#define DEFAULT_PRICE_PER_ID 100
#define FIRST_VERSION 1
#define NAME_BUF_BYTES 64
#define MS_PER_SECOND 1000
#define NS_PER_MS 1000000L

/* PUT으로 고친 상품. 고치지 않은 상품은 id로 계산한 기본값을 쓴다 */
typedef struct product {
    int64_t id;
    char *name;
    int64_t price;
    int64_t version;
    struct product *next;
} product_t;

static pthread_mutex_t source_mutex = PTHREAD_MUTEX_INITIALIZER;
static product_t *updated_products = NULL;
static uint64_t origin_call_count = 0;

static void wait_like_slow_source(void) {
    struct timespec delay = {SLOW_SOURCE_DELAY_MS / MS_PER_SECOND,
                             (SLOW_SOURCE_DELAY_MS % MS_PER_SECOND) * NS_PER_MS};

    while (nanosleep(&delay, &delay) != 0) {
        /* 신호로 깨면 남은 시간만큼 다시 잔다 */
    }
}

static bool is_known_id(int64_t id) {
    return id >= SLOW_SOURCE_MIN_PRODUCT_ID && id <= SLOW_SOURCE_MAX_PRODUCT_ID;
}

/* source_mutex를 쥔 채로 부른다 */
static product_t *find_updated_product(int64_t id) {
    for (product_t *product = updated_products; product != NULL; product = product->next) {
        if (product->id == id) {
            return product;
        }
    }
    return NULL;
}

/* source_mutex를 쥔 채로 상품 JSON을 만든다. 메모리가 부족하면 NULL */
static char *build_product_json(int64_t id) {
    const product_t *product = find_updated_product(id);
    char default_name[NAME_BUF_BYTES];
    const char *name = default_name;
    json_buffer_t buffer = {0};

    snprintf(default_name, sizeof(default_name), "product-%lld", (long long)id);
    if (product != NULL) {
        name = product->name;
    }
    json_buffer_append_text(&buffer, "{\"id\":");
    json_buffer_append_int(&buffer, id);
    json_buffer_append_text(&buffer, ",\"name\":");
    json_buffer_append_string(&buffer, name, strlen(name));
    json_buffer_append_text(&buffer, ",\"price\":");
    json_buffer_append_int(&buffer, product != NULL ? product->price : id * DEFAULT_PRICE_PER_ID);
    json_buffer_append_text(&buffer, ",\"version\":");
    json_buffer_append_int(&buffer, product != NULL ? product->version : FIRST_VERSION);
    json_buffer_append_text(&buffer, "}");
    if (buffer.has_error) {
        json_buffer_destroy(&buffer);
        return NULL;
    }
    return buffer.data;
}

slow_source_status_t slow_source_get_product(int64_t id, char **out_product_json) {
    *out_product_json = NULL;
    wait_like_slow_source();
    pthread_mutex_lock(&source_mutex);
    origin_call_count++;
    if (is_known_id(id)) {
        *out_product_json = build_product_json(id);
    }
    pthread_mutex_unlock(&source_mutex);
    if (!is_known_id(id)) {
        return SLOW_SOURCE_NOT_FOUND;
    }
    return *out_product_json != NULL ? SLOW_SOURCE_OK : SLOW_SOURCE_NO_MEMORY;
}

static char *copy_text(const char *text) {
    size_t text_len = strlen(text);
    char *copy = malloc(text_len + 1);

    if (copy != NULL) {
        memcpy(copy, text, text_len + 1);
    }
    return copy;
}

/* source_mutex를 쥔 채로 부른다. 처음 고치는 상품이면 목록 앞에 새로 만든다 */
static product_t *find_or_create_product(int64_t id) {
    product_t *product = find_updated_product(id);

    if (product != NULL) {
        return product;
    }
    product = calloc(1, sizeof(*product));
    if (product == NULL) {
        return NULL;
    }
    product->id = id;
    product->version = FIRST_VERSION;
    product->next = updated_products;
    updated_products = product;
    return product;
}

slow_source_status_t slow_source_update_product(int64_t id, const char *name, int64_t price,
                                                char **out_product_json) {
    char *name_copy = NULL;
    product_t *product = NULL;

    *out_product_json = NULL;
    if (!is_known_id(id)) {
        return SLOW_SOURCE_NOT_FOUND;
    }
    wait_like_slow_source();
    name_copy = copy_text(name);
    if (name_copy == NULL) {
        return SLOW_SOURCE_NO_MEMORY;
    }
    pthread_mutex_lock(&source_mutex);
    product = find_or_create_product(id);
    if (product != NULL) {
        free(product->name);
        product->name = name_copy;
        product->price = price;
        product->version++;
        *out_product_json = build_product_json(id);
    }
    pthread_mutex_unlock(&source_mutex);
    if (product == NULL) {
        free(name_copy);
        return SLOW_SOURCE_NO_MEMORY;
    }
    return *out_product_json != NULL ? SLOW_SOURCE_OK : SLOW_SOURCE_NO_MEMORY;
}

uint64_t slow_source_get_origin_call_count(void) {
    uint64_t count = 0;

    pthread_mutex_lock(&source_mutex);
    count = origin_call_count;
    pthread_mutex_unlock(&source_mutex);
    return count;
}

void slow_source_destroy(void) {
    pthread_mutex_lock(&source_mutex);
    while (updated_products != NULL) {
        product_t *next = updated_products->next;
        free(updated_products->name);
        free(updated_products);
        updated_products = next;
    }
    pthread_mutex_unlock(&source_mutex);
}
