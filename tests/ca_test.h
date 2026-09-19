#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef void (*ca_test_suite_fn)(void);

typedef struct {
    const char *suite;
    int passed;
    int failed;
} ca_test_run;

void ca_test_begin(const char *suite);
void ca_test_check_impl(bool ok, const char *file, int line, const char *expr);
void ca_test_check_eq_impl(size_t actual, size_t expected, const char *file, int line,
                           const char *aexpr, const char *eexpr);
void ca_test_check_ptr_impl(const void *actual, const void *expected, const char *file, int line,
                            const char *aexpr, const char *eexpr);
void ca_test_subtask(const char *name);
int ca_test_end(void);
int ca_test_run_suite(const char *name, ca_test_suite_fn fn);

#define CHECK(expr) ca_test_check_impl((expr) != 0, __FILE__, __LINE__, #expr)

#define CHECK_SIZE_EQ(actual, expected) \
    ca_test_check_eq_impl((size_t)(actual), (size_t)(expected), __FILE__, __LINE__, #actual, #expected)

#define CHECK_PTR_EQ(actual, expected) \
    ca_test_check_ptr_impl((actual), (expected), __FILE__, __LINE__, #actual, #expected)