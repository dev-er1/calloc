#include <stdint.h>

#include "calloc.h"
#include "helpers.h"
#include "ca_test.h"

void test_suite_alloc(void) {
    ca_test_subtask("allocates from a fresh region");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 100);
        CHECK(p != NULL);
        CHECK((uintptr_t)p % _Alignof(long double) == 0);
        CHECK(a->used > 0);
        CHECK(a->used <= sizeof(arena.storage) - sizeof(CBlock));
    }

    ca_test_subtask("returns distinct non-overlapping blocks");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p1 = c_alloc(a, 64);
        void *p2 = c_alloc(a, 64);
        CHECK(p1 != NULL);
        CHECK(p2 != NULL);
        CHECK(p1 != p2);

        size_t diff = p1 < p2 ? (size_t)((char *)p2 - (char *)p1)
                              : (size_t)((char *)p1 - (char *)p2);
        CHECK(diff >= 64);
    }

    ca_test_subtask("aligns all returned pointers");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        for (int i = 0; i < 24; i++) {
            void *p = c_alloc(a, (size_t)(3 + i * 7));
            CHECK(p != NULL);
            CHECK((uintptr_t)p % _Alignof(long double) == 0);
        }
    }

    ca_test_subtask("rejects requests larger than the region");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        CHECK_PTR_EQ(c_alloc(a, CA_TEST_CAPACITY), NULL);
        CHECK_PTR_EQ(c_alloc(a, CA_TEST_CAPACITY - sizeof(CBlock) + 1), NULL);
    }

    ca_test_subtask("exhausts the region");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        size_t count = 0;
        while (c_alloc(a, 64) != NULL)
            count++;

        CHECK(count >= 10);
        CHECK(count * (64 + sizeof(CBlock)) <= sizeof(arena.storage) - sizeof(CBlock));
        CHECK_SIZE_EQ(a->used, count * 64);
        CHECK_PTR_EQ(c_alloc(a, 64), NULL);
    }

    ca_test_subtask("splits large free blocks");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *big = c_alloc(a, 1000);
        CHECK(big != NULL);

        void *small = c_alloc(a, 64);
        CHECK(small != NULL);
        CHECK(big != small);
        CHECK((uintptr_t)small % _Alignof(long double) == 0);
    }

    ca_test_subtask("reuses freed blocks");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 128);
        CHECK(p != NULL);

        cfree(a, p);

        void *q = c_alloc(a, 128);
        CHECK_PTR_EQ(p, q);
    }

    ca_test_subtask("rejects invalid arguments");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        CHECK_PTR_EQ(c_alloc(a, 0), NULL);
        CHECK_PTR_EQ(c_alloc(NULL, 64), NULL);

        CAllocator bad = {0};
        CHECK_PTR_EQ(c_alloc(&bad, 64), NULL);

        CAllocator rejected = {0};
        unsigned char small[sizeof(CBlock) - 1];
        cinit(&rejected, small, sizeof(small));
        CHECK_PTR_EQ(c_alloc(&rejected, 64), NULL);
    }
}
