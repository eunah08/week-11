#include "slow_source.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "json.h"

#define MS_PER_SECOND 1000
#define NS_PER_MS 1000000
#define DEFAULT_NAME_BYTES 64
#define PRODUCT_WRAPPER_BYTES 32

/* 수정된 상품만 연결 리스트로 기억한다. 수정되지 않은 상품은 기본 이름 "상품 {id}"를 쓴다 */
typedef struct product {
    char *id;
    char *name;
    struct product *next;
} product_t;

static product_t *updated_products = NULL;
static uint64_t origin_call_count = 0;
static pthread_mutex_t source_lock = PTHREAD_MUTEX_INITIALIZER;

static void sleep_ms(int duration_ms) {
    struct timespec duration;
    duration.tv_sec = duration_ms / MS_PER_SECOND;
    duration.tv_nsec = (long)(duration_ms % MS_PER_SECOND) * NS_PER_MS;
    nanosleep(&duration, NULL);
}

static product_t *find_product(const char *product_id) {
    for (product_t *product = updated_products; product != NULL; product = product->next) {
        if (strcmp(product->id, product_id) == 0) {
            return product;
        }
    }
    return NULL;
}

/* 상품 JSON을 만든다. source_lock을 잡은 채로 부른다. 해제 책임 : 호출한 쪽이 free() */
static char *create_product_json(const char *product_id) {
    char default_name[DEFAULT_NAME_BYTES];
    const product_t *product = find_product(product_id);
    snprintf(default_name, sizeof(default_name), "상품 %s", product_id);

    char *escaped_name = json_escape_string(product != NULL ? product->name : default_name);
    if (escaped_name == NULL) {
        return NULL;
    }
    size_t json_size = strlen(product_id) + strlen(escaped_name) + PRODUCT_WRAPPER_BYTES;
    char *json = malloc(json_size);
    if (json != NULL) {
        snprintf(json, json_size, "{\"id\": \"%s\", \"name\": %s}", product_id, escaped_name);
    }
    free(escaped_name);
    return json;
}

char *slow_source_get_product(const char *product_id) {
    sleep_ms(SLOW_SOURCE_DELAY_MS);   /* 잠금 밖에서 기다려서 여러 요청이 동시에 기다릴 수 있다 */

    pthread_mutex_lock(&source_lock);
    origin_call_count++;
    char *json = create_product_json(product_id);
    pthread_mutex_unlock(&source_lock);
    return json;
}

static product_t *create_product(const char *product_id) {
    product_t *product = calloc(1, sizeof(*product));
    if (product == NULL) {
        return NULL;
    }
    product->id = strdup(product_id);
    if (product->id == NULL) {
        free(product);
        return NULL;
    }
    product->next = updated_products;
    updated_products = product;
    return product;
}

bool slow_source_update_product(const char *product_id, const char *name) {
    char *new_name = strdup(name);
    if (new_name == NULL) {
        return false;
    }

    pthread_mutex_lock(&source_lock);
    product_t *product = find_product(product_id);
    if (product == NULL) {
        product = create_product(product_id);
    }
    if (product != NULL) {
        free(product->name);
        product->name = new_name;
    }
    pthread_mutex_unlock(&source_lock);

    if (product == NULL) {
        free(new_name);
        return false;
    }
    return true;
}

uint64_t slow_source_get_origin_call_count(void) {
    pthread_mutex_lock(&source_lock);
    uint64_t call_count = origin_call_count;
    pthread_mutex_unlock(&source_lock);
    return call_count;
}
