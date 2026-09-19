#include "calloc.h"
#include "helpers.h"
#include "ca_test.h"

void test_suite_free(void) {
    ca_test_subtask("returns memory to the pool");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 128);
        CHECK(p != NULL);
        CHECK_SIZE_EQ(a->used, 128);

        cfree(a, p);
        CHECK_SIZE_EQ(a->used, 0);

        void *q = c_alloc(a, 128);
        CHECK_PTR_EQ(p, q);
    }

    ca_test_subtask("ignores `NULL` pointers");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        cfree(a, NULL);
        cfree(NULL, NULL);

        void *p = c_alloc(a, 64);
        CHECK(p != NULL);
    }

    ca_test_subtask("ignores double free");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 64);
        cfree(a, p);
        cfree(a, p);

        void *q = c_alloc(a, 64);
        CHECK_PTR_EQ(p, q);
    }

    ca_test_subtask("ignores foreign pointers");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        cfree(a, arena.storage);
        cfree(a, arena.storage + 64);
        cfree(a, arena.storage + sizeof(CBlock) + 1);

        void *p = c_alloc(a, 64);
        CHECK(p != NULL);
    }

    ca_test_subtask("merges a free block with its next neighbour");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 64);
        void *q = c_alloc(a, 64);
        CHECK(p != NULL && q != NULL);

        cfree(a, p);
        cfree(a, q);

        void *r = c_alloc(a, 160);
        CHECK(r != NULL);
    }

    ca_test_subtask("merges a free block with its previous neighbour");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 64);
        void *q = c_alloc(a, 64);
        void *r = c_alloc(a, 64);
        CHECK(p != NULL && q != NULL && r != NULL);

        cfree(a, q);
        cfree(a, p);

        void *s = c_alloc(a, 160);
        CHECK(s != NULL);
    }

    ca_test_subtask("merges a free block with both neighbours");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 64);
        void *q = c_alloc(a, 64);
        void *r = c_alloc(a, 64);
        CHECK(p != NULL && q != NULL && r != NULL);

        cfree(a, p);
        cfree(a, r);
        cfree(a, q);

        void *s = c_alloc(a, 224);
        CHECK(s != NULL);
    }
}