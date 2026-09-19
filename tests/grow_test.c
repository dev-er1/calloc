#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "calloc.h"
#include "helpers.h"
#include "ca_test.h"

// Validates the block list invariants of a self-managed arena.
typedef struct {
    size_t blocks;
    size_t allocated;
} ca_grow_state;

static bool ca_grow_validate(const CAllocator *a, ca_grow_state *state) {
    const unsigned char *begin = (const unsigned char *)a->memory;
    const unsigned char *end = begin + a->capacity;
    size_t allocated = 0;
    size_t blocks = 0;

    for (const CBlock *block = a->blocks; block != NULL; block = block->next) {
        const unsigned char *addr = (const unsigned char *)block;

        if (addr < begin || addr >= end)
            return false;

        if (block->previous != NULL) {
            if ((const unsigned char *)block->previous < begin ||
                (const unsigned char *)block->previous >= end ||
                block->previous->next != block)
                return false;
        }

        if (block->next != NULL) {
            if ((const unsigned char *)block->next < begin ||
                (const unsigned char *)block->next >= end ||
                block->next->previous != block)
                return false;
        }

        const unsigned char *data_end = addr + sizeof(CBlock) + block->size;

        if (block->next != NULL && data_end != (const unsigned char *)block->next)
            return false;

        if (block->next == NULL && data_end != end)
            return false;

        if (!block->free)
            allocated += block->size;

        blocks++;
    }

    if (allocated != a->used)
        return false;

    state->blocks = blocks;
    state->allocated = allocated;
    return true;
}

void test_suite_grow(void) {
    ca_test_subtask("initializes a self-managed arena");
    {
        CAllocator a;
        cinit(&a, NULL, CA_TEST_CAPACITY);

        CHECK(a.initialized);
        CHECK(a.own_memory);
        CHECK(a.memory != NULL);
        CHECK_SIZE_EQ(a.capacity, CA_TEST_CAPACITY);

        void *p = c_alloc(&a, 64);
        CHECK(p != NULL);
        CHECK((uintptr_t)p % _Alignof(long double) == 0);

        ca_grow_state s;
        CHECK(ca_grow_validate(&a, &s));
        CHECK_SIZE_EQ(s.allocated, ca_test_align_up(64));

        cdestroy(&a);
    }

    ca_test_subtask("rejects an empty initial capacity");
    {
        CAllocator a = {0};
        cinit(&a, NULL, 0);

        CHECK(!a.initialized);
        CHECK_PTR_EQ(c_alloc(&a, 64), NULL);
    }

    ca_test_subtask("grows when a request exceeds the initial capacity");
    {
        CAllocator a;
        cinit(&a, NULL, sizeof(CBlock) + 64);

        void *p = c_alloc(&a, 4096);
        CHECK(p != NULL);
        CHECK((uintptr_t)p % _Alignof(long double) == 0);
        CHECK(a.capacity > sizeof(CBlock) + 64);

        ca_grow_state s;
        CHECK(ca_grow_validate(&a, &s));
        CHECK_SIZE_EQ(s.allocated, ca_test_align_up(4096));

        cdestroy(&a);
    }

    ca_test_subtask("grows repeatedly, content survives");
    {
        CAllocator a;
        cinit(&a, NULL, 256);

        for (int i = 0; i < 40; i++) {
            size_t sz = 128 + (size_t)i * 32;
            void *p = c_alloc(&a, sz);

            CHECK(p != NULL);
            memset(p, (int)((sz - 128) / 32), sz);

            ca_grow_state s;
            CHECK(ca_grow_validate(&a, &s));
        }

        size_t found = 0;

        for (CBlock *block = a.blocks; block != NULL; block = block->next) {
            if (block->free)
                continue;

            CHECK(block->size >= 128);
            CHECK(block->size <= 128 + 39 * 32);

            unsigned char *data = (unsigned char *)block + sizeof(CBlock);
            unsigned char expect = (unsigned char)((block->size - 128) / 32);

            for (size_t j = 0; j < block->size; j++)
                CHECK(data[j] == expect);

            found++;
        }

        CHECK_SIZE_EQ(found, 40);
        cdestroy(&a);
    }

    ca_test_subtask("frees and reuses after growth");
    {
        CAllocator a;
        cinit(&a, NULL, 256);

        void *p = c_alloc(&a, 64);
        CHECK(p != NULL);
        cfree(&a, p);

        void *q = c_alloc(&a, 128);
        CHECK(q != NULL);

        void *r = c_alloc(&a, 64);
        CHECK(r != NULL);

        ca_grow_state s;
        CHECK(ca_grow_validate(&a, &s));
        CHECK_SIZE_EQ(s.allocated, ca_test_align_up(128) + 64);

        cdestroy(&a);
    }

    ca_test_subtask("realloc moves across a growth");
    {
        CAllocator a;
        cinit(&a, NULL, 256);

        void *p = c_alloc(&a, 64);
        CHECK(p != NULL);
        memset(p, 0xAB, 64);

        void *q = crealloc(&a, p, 8192);
        CHECK(q != NULL);

        unsigned char *b = q;

        for (int i = 0; i < 64; i++)
            CHECK(b[i] == 0xAB);

        ca_grow_state s;
        CHECK(ca_grow_validate(&a, &s));
        CHECK_SIZE_EQ(s.allocated, ca_test_align_up(8192));

        cdestroy(&a);
    }

    ca_test_subtask("reset restores a self-managed arena");
    {
        CAllocator a;
        cinit(&a, NULL, 256);

        CHECK(c_alloc(&a, 4096) != NULL);

        creset(&a);
        CHECK_SIZE_EQ(a.used, 0);
        CHECK(a.blocks->free);
        CHECK_SIZE_EQ(a.blocks->size, a.capacity - sizeof(CBlock));

        void *p = c_alloc(&a, a.capacity - sizeof(CBlock));
        CHECK(p != NULL);
        CHECK(c_alloc(&a, 64) != NULL);

        cdestroy(&a);
    }

    ca_test_subtask("destroy releases and disables the arena");
    {
        CAllocator a;
        cinit(&a, NULL, 256);

        CHECK(c_alloc(&a, 64) != NULL);

        cdestroy(&a);
        CHECK(!a.initialized);
        CHECK_PTR_EQ(c_alloc(&a, 64), NULL);

        cinit(&a, NULL, 256);
        CHECK(c_alloc(&a, 64) != NULL);
        cdestroy(&a);
    }

    ca_test_subtask("destroy is a no-op for external arenas");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        CHECK(c_alloc(a, 64) != NULL);

        cdestroy(a);
        CHECK(!a->initialized);
        CHECK_PTR_EQ(c_alloc(a, 64), NULL);

        cinit(a, arena.storage, sizeof(arena.storage));
        CHECK(c_alloc(a, 64) != NULL);
    }

    ca_test_subtask("churn with growth keeps the list valid");
    {
        CAllocator a;
        cinit(&a, NULL, 128);

        void *live[16] = {0};
        ca_grow_state s;

        for (size_t i = 0; i < 300; i++) {
            void *region = a.memory;
            size_t slot = i % 16;
            size_t sz = (i % 8) * 64 + 8;

            if (live[slot] != NULL) {
                cfree(&a, live[slot]);
                live[slot] = NULL;
            }

            if (a.memory != region) {
                // The region was relocated: earlier pointers are stale.
                for (int k = 0; k < 16; k++)
                    live[k] = NULL;
            }

            live[slot] = c_alloc(&a, sz);
            CHECK(live[slot] != NULL);

            CHECK(ca_grow_validate(&a, &s));
        }

        for (int k = 0; k < 16; k++) {
            if (live[k] != NULL)
                cfree(&a, live[k]);
        }

        // Blocks whose pointers went stale on relocation stay allocated
        // until destroy; only the list itself must stay consistent.
        CHECK(ca_grow_validate(&a, &s));

        cdestroy(&a);
    }
}