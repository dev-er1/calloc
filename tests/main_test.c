#include "ca_test.h"

void test_suite_cainit(void);
void test_suite_alloc(void);
void test_suite_free(void);
void test_suite_realloc(void);
void test_suite_reset(void);
void test_suite_grow(void);
void test_suite_stress(void);

int main(void) {
    int failures = 0;
    failures += ca_test_run_suite("cinit", test_suite_cainit);
    failures += ca_test_run_suite("alloc", test_suite_alloc);
    failures += ca_test_run_suite("free", test_suite_free);
    failures += ca_test_run_suite("realloc", test_suite_realloc);
    failures += ca_test_run_suite("reset", test_suite_reset);
    failures += ca_test_run_suite("grow", test_suite_grow);
    failures += ca_test_run_suite("stress", test_suite_stress);

    if (failures > 0) {
        printf("TOTAL: %d failed\n", failures);
        return 1;
    }

    printf("TOTAL: all passed\n");
    return 0;
}