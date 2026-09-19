#include "calloc.h"
#include "helpers.h"
#include "ca_test.h"

void test_suite_cainit(void) {
    ca_test_subtask("initializes a valid region");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        CHECK(a->initialized);
        CHECK_PTR_EQ(a->memory, arena.storage);
        CHECK_SIZE_EQ(a->capacity, sizeof(arena.storage));
        CHECK_SIZE_EQ(a->used, 0);
        CHECK_PTR_EQ(a->blocks, arena.storage);
        CHECK(a->blocks->free);
        CHECK_SIZE_EQ(a->blocks->size, sizeof(arena.storage) - sizeof(CBlock));
        CHECK_PTR_EQ(a->blocks->next, NULL);
        CHECK_PTR_EQ(a->blocks->previous, NULL);
    }

    ca_test_subtask("rejects `NULL` allocator");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);
        cinit(NULL, arena.storage, sizeof(arena.storage));
        CHECK(a->initialized);
    }

    ca_test_subtask("accepts `NULL` memory as a self-managed arena");
    {
        CAllocator a = {0};
        cinit(&a, NULL, 4096);
        CHECK(a.initialized);
        CHECK(a.memory != NULL);
        CHECK(a.own_memory);
        CHECK_SIZE_EQ(a.used, 0);
        CHECK_PTR_EQ(a.blocks, a.memory);
    }

    ca_test_subtask("rejects capacity smaller than a block header");
    {
        CAllocator a = {0};
        unsigned char small[sizeof(CBlock) - 1];
        cinit(&a, small, sizeof(small));
        CHECK(!a.initialized);
        CHECK_SIZE_EQ(a.capacity, 0);
    }

    ca_test_subtask("rejects misaligned user memory");
    {
        ca_test_arena arena;
        CAllocator a = {0};
        cinit(&a, arena.storage + 1, sizeof(arena.storage) - 1);
        CHECK(!a.initialized);
    }
}
