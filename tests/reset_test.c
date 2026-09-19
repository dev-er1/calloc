#include "calloc.h"
#include "helpers.h"
#include "ca_test.h"

void test_suite_reset(void) {
    ca_test_subtask("restores the full region");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 64);
        void *q = c_alloc(a, 64);
        CHECK(p != NULL && q != NULL);
        CHECK(a->used > 0);

        creset(a);
        CHECK_SIZE_EQ(a->used, 0);
        CHECK(a->blocks->free);
        CHECK_SIZE_EQ(a->blocks->size, sizeof(arena.storage) - sizeof(CBlock));

        void *r = c_alloc(a, sizeof(arena.storage) - sizeof(CBlock));
        CHECK(r != NULL);
        CHECK_PTR_EQ(c_alloc(a, 64), NULL);
    }

    ca_test_subtask("remains usable after a reset");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        creset(a);

        void *p = c_alloc(a, 100);
        CHECK(p != NULL);

        cfree(a, p);
        CHECK_SIZE_EQ(a->used, 0);
    }

    ca_test_subtask("ignores uninitialized allocators");
    {
        CAllocator bad = {0};
        creset(&bad);

        CAllocator rejected = {0};
        cinit(&rejected, NULL, 0);
        creset(&rejected);
    }
}