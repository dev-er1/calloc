#include "ca_test.h"

static ca_test_run g_ca_test;

void ca_test_begin(const char *suite) {
    g_ca_test = (ca_test_run){.suite = suite};
    printf("== %s ==\n", suite);
}

void ca_test_check_impl(bool ok, const char *file, int line, const char *expr) {
    if (ok) {
        g_ca_test.passed++;
    } else {
        g_ca_test.failed++;
        printf("FAIL %s:%d: %s\n", file, line, expr);
    }
}

void ca_test_check_eq_impl(size_t actual, size_t expected, const char *file, int line,
                           const char *aexpr, const char *eexpr) {
    if (actual == expected) {
        g_ca_test.passed++;
    } else {
        g_ca_test.failed++;
        printf("FAIL %s:%d: %s == %s (actual %llu, expected %llu)\n", file, line, aexpr, eexpr,
               (unsigned long long)actual, (unsigned long long)expected);
    }
}

void ca_test_check_ptr_impl(const void *actual, const void *expected, const char *file, int line,
                            const char *aexpr, const char *eexpr) {
    if (actual == expected) {
        g_ca_test.passed++;
    } else {
        g_ca_test.failed++;
        printf("FAIL %s:%d: %s == %s (actual %p, expected %p)\n", file, line, aexpr, eexpr,
               actual, expected);
    }
}

void ca_test_subtask(const char *name) {
    printf("  - %s\n", name);
}

int ca_test_end(void) {
    int failed = g_ca_test.failed;
    printf("%s: %d checks passed, %d failed\n\n", g_ca_test.suite, g_ca_test.passed,
           g_ca_test.failed);
    return failed;
}

int ca_test_run_suite(const char *name, ca_test_suite_fn fn) {
    ca_test_begin(name);
    fn();
    return ca_test_end();
}