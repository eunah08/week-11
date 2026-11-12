#include "slow_source.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct product {
    char *id;
    char *value;
    struct product *next;
} product_t;
struct slow_source {
    pthread_mutex_t mutex;
    product_t *products;
    uint64_t origin_call_count;
    unsigned delay_ms;
};

static product_t *find_product(slow_source_t *source, const char *id) {
    for (product_t *product = source->products; product != NULL; product = product->next) {
        if (strcmp(product->id, id) == 0) { return product; }
    }
    return NULL;
}

slow_source_t *slow_source_create(unsigned delay_ms) {
    slow_source_t *source = calloc(1, sizeof(*source));
    if (source == NULL) { return NULL; }
    if (pthread_mutex_init(&source->mutex, NULL) != 0) { free(source); return NULL; }
    source->delay_ms = delay_ms;
    return source;
}

void slow_source_destroy(slow_source_t *source) {
    if (source == NULL) { return; }
    product_t *product = source->products;
    while (product != NULL) {
        product_t *next = product->next;
        free(product->id); free(product->value); free(product);
        product = next;
    }
    pthread_mutex_destroy(&source->mutex);
    free(source);
}

source_status_t slow_source_get_product(slow_source_t *source, const char *id, char **out_value) {
    *out_value = NULL;
    pthread_mutex_lock(&source->mutex);
    source->origin_call_count++;
    pthread_mutex_unlock(&source->mutex);
    struct timespec delay = {source->delay_ms / 1000, (source->delay_ms % 1000) * 1000000L};
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) { }
    if (strcmp(id, "fail") == 0) { return SOURCE_FAILED; }
    pthread_mutex_lock(&source->mutex);
    product_t *product = find_product(source, id);
    *out_value = strdup(product != NULL ? product->value : id);
    pthread_mutex_unlock(&source->mutex);
    return *out_value != NULL ? SOURCE_OK : SOURCE_NO_MEMORY;
}

static product_t *create_product(const char *id, char *value) {
    product_t *product = calloc(1, sizeof(*product));
    if (product == NULL) { return NULL; }
    product->id = strdup(id);
    if (product->id == NULL) { free(product); return NULL; }
    product->value = value;
    return product;
}

source_status_t slow_source_update_product(slow_source_t *source, const char *id,
                                            const char *value) {
    char *copy = strdup(value);
    if (copy == NULL) { return SOURCE_NO_MEMORY; }
    pthread_mutex_lock(&source->mutex);
    product_t *product = find_product(source, id);
    if (product != NULL) {
        free(product->value); product->value = copy;
    } else {
        product = create_product(id, copy);
        if (product != NULL) { product->next = source->products; source->products = product; }
    }
    pthread_mutex_unlock(&source->mutex);
    if (product == NULL) { free(copy); return SOURCE_NO_MEMORY; }
    return SOURCE_OK;
}

uint64_t slow_source_get_origin_call_count(slow_source_t *source) {
    pthread_mutex_lock(&source->mutex);
    uint64_t count = source->origin_call_count;
    pthread_mutex_unlock(&source->mutex);
    return count;
}
