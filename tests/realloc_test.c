#include <string.h>

#include "calloc.h"
#include "helpers.h"
#include "ca_test.h"

void test_suite_realloc(void) {
    ca_test_subtask("acts as alloc for a `NULL` pointer");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = crealloc(a, NULL, 64);
        CHECK(p != NULL);
        CHECK(a->used > 0);
    }

    ca_test_subtask("acts as free for size 0");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 64);
        CHECK(p != NULL);

        CHECK_PTR_EQ(crealloc(a, p, 0), NULL);
        CHECK_SIZE_EQ(a->used, 0);

        void *q = c_alloc(a, 64);
        CHECK_PTR_EQ(p, q);
    }

    ca_test_subtask("shrinks in place");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        unsigned char data[256];
        for (size_t i = 0; i < sizeof(data); i++)
            data[i] = (unsigned char)i;

        void *p = c_alloc(a, 256);
        CHECK(p != NULL);
        memcpy(p, data, sizeof(data));

        void *q = crealloc(a, p, 100);
        CHECK_PTR_EQ(q, p);
        CHECK_SIZE_EQ(a->used, ca_test_align_up(100));
        CHECK(memcmp(q, data, 100) == 0);
    }

    ca_test_subtask("grows by moving when no adjacent space is free");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 64);
        CHECK(p != NULL);
        memset(p, 0xAB, 64);

        void *q = c_alloc(a, 64);
        CHECK(q != NULL);

        void *r = crealloc(a, p, 200);
        CHECK(r != NULL);
        CHECK(r != p);

        unsigned char *bytes = (unsigned char *)r;
        for (int i = 0; i < 64; i++)
            CHECK(bytes[i] == 0xAB);

        void *s = c_alloc(a, 64);
        CHECK_PTR_EQ(s, p);
    }

    ca_test_subtask("grows in place into a free next block");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 64);
        CHECK(p != NULL);
        memset(p, 0xEE, 64);

        void *q = c_alloc(a, 64);
        CHECK(q != NULL);

        cfree(a, q);

        void *r = crealloc(a, p, 128);
        CHECK_PTR_EQ(r, p);
        CHECK_SIZE_EQ(a->used, 128);

        unsigned char *bytes = (unsigned char *)r;
        for (int i = 0; i < 64; i++)
            CHECK(bytes[i] == 0xEE);
    }

    ca_test_subtask("grows in place with a partial absorb");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 64);
        void *q = c_alloc(a, 64);
        void *r = c_alloc(a, 64);
        void *s = c_alloc(a, 64);
        CHECK(p != NULL && q != NULL && r != NULL && s != NULL);

        cfree(a, s);
        cfree(a, q);

        void *u = crealloc(a, p, 128);
        CHECK_PTR_EQ(u, p);
        CHECK(u != r);

        // The whole free next block (64) is absorbed; the block after it
        // stays untouched and the tail block is still reusable.
        void *t = c_alloc(a, 64);
        CHECK_PTR_EQ(t, s);
    }

    ca_test_subtask("grows in place and splits a larger free block");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 64);
        CHECK(p != NULL);

        void *q = c_alloc(a, 64);
        CHECK(q != NULL);

        cfree(a, q);

        void *r = crealloc(a, p, 128);
        CHECK_PTR_EQ(r, p);

        void *s = c_alloc(a, 128);
        CHECK(s != NULL);
        CHECK(s != p);
    }

    ca_test_subtask("splits when the remainder is exactly the threshold");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 64);
        void *q = c_alloc(a, 64);
        void *r = c_alloc(a, 8);
        CHECK(p != NULL && q != NULL && r != NULL);

        cfree(a, q);

        // combined = 64 + 32 + 64 = 160, aligned = 120, remainder = 40:
        // a tail block of exactly 8 bytes is split off.
        void *s = crealloc(a, p, 120);
        CHECK_PTR_EQ(s, p);
        CHECK_SIZE_EQ(a->used, 128);

        void *t = c_alloc(a, 8);
        CHECK(t != NULL);
        CHECK(t != p && t != r);
        CHECK_SIZE_EQ(a->used, 136);
    }

    ca_test_subtask("absorbs the whole region below the split threshold");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 64);
        void *q = c_alloc(a, 64);
        void *r = c_alloc(a, 64);
        CHECK(p != NULL && q != NULL && r != NULL);

        cfree(a, q);

        // combined = 160, aligned = 128, remainder = 32: the next block is
        // fully absorbed and nothing is lost.
        void *s = crealloc(a, p, 128);
        CHECK_PTR_EQ(s, p);
        CHECK_SIZE_EQ(a->used, 224);

        cfree(a, p);
        cfree(a, r);

        void *t = c_alloc(a, sizeof(arena.storage) - sizeof(CBlock));
        CHECK(t != NULL);
    }

    ca_test_subtask("grows when the free next block is the last one");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 64);
        CHECK(p != NULL);

        void *q = c_alloc(a, 512);
        CHECK(q != NULL);

        cfree(a, q);

        void *r = crealloc(a, p, 1024);
        CHECK_PTR_EQ(r, p);
        CHECK_SIZE_EQ(a->used, 1024);

        void *s = c_alloc(a, 3008);
        CHECK(s != NULL);
        CHECK_SIZE_EQ(a->used, 4032);

        CHECK_PTR_EQ(c_alloc(a, 1), NULL);
    }

    ca_test_subtask("grows repeatedly in place through splits");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 64);
        void *q = c_alloc(a, 64);
        void *r = c_alloc(a, 64);
        void *s = c_alloc(a, 64);
        CHECK(p != NULL && q != NULL && r != NULL && s != NULL);

        memset(p, 0xAB, 64);

        cfree(a, q);
        cfree(a, r);

        size_t sizes[] = {80, 120, 152, 224};
        for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
            void *u = crealloc(a, p, sizes[i]);
            CHECK_PTR_EQ(u, p);
        }

        // The last grow (aligned 224 of combined 256) has remainder 32 and
        // claims the whole combined region.
        CHECK_SIZE_EQ(a->used, 320);

        unsigned char *bytes = (unsigned char *)p;
        for (int i = 0; i < 64; i++)
            CHECK(bytes[i] == 0xAB);

        cfree(a, p);
        cfree(a, s);

        void *t = c_alloc(a, sizeof(arena.storage) - sizeof(CBlock));
        CHECK(t != NULL);
    }

    ca_test_subtask("shrinks then grows back in place");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 256);
        CHECK(p != NULL);

        void *q = crealloc(a, p, 16);
        CHECK_PTR_EQ(q, p);
        CHECK_SIZE_EQ(a->used, 16);

        void *r = crealloc(a, p, 64);
        CHECK_PTR_EQ(r, p);
        CHECK_SIZE_EQ(a->used, 64);

        void *s = crealloc(a, p, 8);
        CHECK_PTR_EQ(s, p);
        CHECK_SIZE_EQ(a->used, 8);

        void *t = crealloc(a, p, 40);
        CHECK_PTR_EQ(t, p);
        CHECK_SIZE_EQ(a->used, 40);
    }

    ca_test_subtask("grows when the new aligned size matches combined");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 96);
        void *q = c_alloc(a, 64);
        void *r = c_alloc(a, 8);
        CHECK(p != NULL && q != NULL && r != NULL);

        cfree(a, q);

        // combined = 96 + 32 + 64 = 192, aligned = 192, remainder = 0.
        void *s = crealloc(a, p, 192);
        CHECK_PTR_EQ(s, p);
        CHECK_SIZE_EQ(a->used, 200);

        cfree(a, p);
        cfree(a, r);

        void *t = c_alloc(a, sizeof(arena.storage) - sizeof(CBlock));
        CHECK(t != NULL);
    }

    ca_test_subtask("grows into a tiny free next block");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 64);
        void *q = c_alloc(a, 8);
        void *r = c_alloc(a, 8);
        CHECK(p != NULL && q != NULL && r != NULL);

        cfree(a, q);

        // combined = 64 + 32 + 8 = 104, aligned = 96, remainder = 8: the
        // tiny next block is absorbed without a split.
        void *s = crealloc(a, p, 96);
        CHECK_PTR_EQ(s, p);
        CHECK_SIZE_EQ(a->used, 112);
    }

    ca_test_subtask("keeps the old block when growth fails");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 64);
        CHECK(p != NULL);
        memset(p, 0xCD, 64);

        while (c_alloc(a, 128) != NULL) {
        }

        void *q = crealloc(a, p, CA_TEST_CAPACITY);
        CHECK_PTR_EQ(q, NULL);

        unsigned char *bytes = (unsigned char *)p;
        for (int i = 0; i < 64; i++)
            CHECK(bytes[i] == 0xCD);

        cfree(a, p);

        void *s = c_alloc(a, 64);
        CHECK_PTR_EQ(s, p);
    }

    ca_test_subtask("rejects foreign pointers");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        CHECK_PTR_EQ(crealloc(a, arena.storage + 64, 64), NULL);
    }

    ca_test_subtask("rejects freed pointers");
    {
        ca_test_arena arena;
        CAllocator *a = ca_test_arena_init(&arena);

        void *p = c_alloc(a, 64);
        cfree(a, p);

        CHECK_PTR_EQ(crealloc(a, p, 128), NULL);
    }

    ca_test_subtask("rejects invalid allocators");
    {
        CHECK_PTR_EQ(crealloc(NULL, NULL, 64), NULL);

        CAllocator bad = {0};
        CHECK_PTR_EQ(crealloc(&bad, NULL, 64), NULL);
    }
}