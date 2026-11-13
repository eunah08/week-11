#ifndef FAIL_ALLOC_H
#define FAIL_ALLOC_H
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
void *test_malloc(size_t size);
void *test_calloc(size_t count, size_t size);
char *test_strdup(const char *value);
void test_free(void *pointer);
#define malloc test_malloc
#define calloc test_calloc
#define strdup test_strdup
#define free test_free
#endif
