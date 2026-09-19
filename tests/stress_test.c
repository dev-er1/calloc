#include "calloc.h"
#include "helpers.h"
#include "ca_test.h"

#include <stdint.h>

#define CA_STRESS_OPS 4000
#define CA_STRESS_MAX_LIVE 64
#define CA_STRESS_MAX_SIZE 64
#define CA_STRESS_ALIGN _Alignof(long double)

typedef struct {
    unsigned char *ptr;
    size_t size;
    unsigned seed;
} ca_stress_block;

static unsigned ca_stress_rand = 0x12345678;
static int g_ca_stress_op;

static unsigned ca_stress_next(void) {
    ca_stress_rand ^= ca_stress_rand << 13;
    ca_stress_rand ^= ca_stress_rand >> 17;
    ca_stress_rand ^= ca_stress_rand << 5;
    return ca_stress_rand;
}

static void ca_stress_fill(unsigned char *p, size_t n, unsigned seed) {
    for (size_t i = 0; i < n; i++)
        p[i] = (unsigned char)(seed * 31u + i * 7u);
}

static bool ca_stress_verify(const unsigned char *p, size_t n, unsigned seed) {
    for (size_t i = 0; i < n; i++)
        if (p[i] != (unsigned char)(seed * 31u + i * 7u))
            return false;
    return true;
}

static void ca_stress_fail(int op, const char *kind, const unsigned char *p, size_t n,
                           unsigned seed) {
    size_t bad = 0;
    while (bad < n && p[bad] == (unsigned char)(seed * 31u + bad * 7u))
        bad++;
    printf("STRESS FAIL op=%d %s ptr=%p size=%llu seed=%u first_bad=%llu\n", op, kind, p,
           (unsigned long long)n, seed, (unsigned long long)bad);
}

static bool ca_stress_check(const unsigned char *p, size_t n, unsigned seed, int op,
                            const char *kind) {
    if (ca_stress_verify(p, n, seed)) {
        ca_test_check_impl(true, __FILE__, __LINE__, kind);
        return true;
    }
    ca_stress_fail(op, kind, p, n, seed);
    ca_test_check_impl(false, __FILE__, __LINE__, kind);
    return false;
}

// Walks the block list after every stress operation and verifies the
// structural invariants of the allocator.
static bool ca_stress_validate(const CAllocator *a, const ca_stress_block *live,
                               size_t live_count, int op) {
    bool ok = true;
    const CBlock *b = a->blocks;
    const unsigned char *arena_begin = (const unsigned char *)a->memory;
    const unsigned char *arena_end = arena_begin + a->capacity;
    size_t used_sum = 0;
    size_t n = 0;

    while (b != NULL && ok) {
        const unsigned char *block_begin = (const unsigned char *)b;
        const unsigned char *block_end = block_begin + sizeof(CBlock) + b->size;

        if (block_begin < arena_begin || block_end > arena_end ||
            (b->next == NULL && block_end != arena_end)) {
            printf("STRESS INVARIANT op=%d blk[%zu] out of bounds: %p..%p arena %p..%p\n", op,
                   n, (const void *)block_begin, (const void *)block_end,
                   (const void *)arena_begin, (const void *)arena_end);
            ok = false;
        } else if ((uintptr_t)b % CA_STRESS_ALIGN != 0 ||
                   b->size % CA_STRESS_ALIGN != 0 || b->size == 0) {
            printf("STRESS INVARIANT op=%d blk[%zu] misaligned: addr=%p size=%llu\n", op, n,
                   (const void *)b, (unsigned long long)b->size);
            ok = false;
        } else if (b->next != NULL && b->next != (const CBlock *)block_end) {
            printf("STRESS INVARIANT op=%d blk[%zu] non-contiguous: next=%p expected=%p\n", op,
                   n, (const void *)b->next, (const void *)block_end);
            ok = false;
        } else if ((b->previous == NULL) != (n == 0) ||
                   (b->previous != NULL && b->previous->next != b)) {
            printf("STRESS INVARIANT op=%d blk[%zu] broken previous link\n", op, n);
            ok = false;
        } else if (n == 0 && b != (const CBlock *)a->memory) {
            printf("STRESS INVARIANT op=%d head not at memory start: %p\n", op,
                   (const void *)b);
            ok = false;
        }

        if (!b->free)
            used_sum += b->size;

        b = b->next;
        n++;
    }

    if (ok && used_sum != a->used) {
        printf("STRESS INVARIANT op=%d used=%llu sum-of-allocated=%llu\n", op,
               (unsigned long long)a->used, (unsigned long long)used_sum);
        ok = false;
    }

    for (size_t i = 0; ok && i < live_count; i++) {
        b = a->blocks;
        n = 0;
        while (b != NULL && (const unsigned char *)b + sizeof(CBlock) != live[i].ptr) {
            b = b->next;
            n++;
        }
        if (b == NULL) {
            printf("STRESS INVARIANT op=%d live[%zu] ptr=%p has no block\n", op, i,
                   (const void *)live[i].ptr);
            ok = false;
        } else if (b->free) {
            printf("STRESS INVARIANT op=%d live[%zu] ptr=%p points to a free block\n", op, i,
                   (const void *)live[i].ptr);
            ok = false;
        } else if (b->size < ca_test_align_up(live[i].size)) {
            printf("STRESS INVARIANT op=%d live[%zu] ptr=%p size=%llu block=%llu\n", op, i,
                   (const void *)live[i].ptr, (unsigned long long)live[i].size,
                   (unsigned long long)b->size);
            ok = false;
        }
    }

    ca_test_check_impl(ok, __FILE__, __LINE__, "invariants");
    return ok;
}

void test_suite_stress(void) {
    ca_test_arena arena;
    CAllocator *a = ca_test_arena_init(&arena);

    ca_stress_block live[CA_STRESS_MAX_LIVE];
    size_t live_count = 0;

    for (int op = 0; op < CA_STRESS_OPS; op++) {
        g_ca_stress_op = op;
        unsigned roll = ca_stress_next() % 100;

        if (live_count == CA_STRESS_MAX_LIVE || (roll < 45 && live_count > 0)) {
            size_t i = ca_stress_next() % live_count;
            ca_stress_check(live[i].ptr, live[i].size, live[i].seed, op, "pre-free");
            cfree(a, live[i].ptr);
            live[i] = live[--live_count];
        } else if (roll < 65 && live_count > 0) {
            size_t i = ca_stress_next() % live_count;
            ca_stress_check(live[i].ptr, live[i].size, live[i].seed, op, "pre-realloc");

            size_t new_size = ca_stress_next() % CA_STRESS_MAX_SIZE + 1;
            unsigned char *q = crealloc(a, live[i].ptr, new_size);

            if (q != NULL) {
                size_t kept = live[i].size < new_size ? live[i].size : new_size;
                ca_stress_check(q, kept, live[i].seed, op, "post-realloc");
                ca_stress_fill(q, new_size, live[i].seed);
                live[i].ptr = q;
                live[i].size = new_size;
            } else {
                ca_stress_check(live[i].ptr, live[i].size, live[i].seed, op,
                                "post-realloc-fail");
            }
        } else {
            size_t size = ca_stress_next() % CA_STRESS_MAX_SIZE + 1;
            unsigned seed = ca_stress_next();
            unsigned char *p = c_alloc(a, size);

            if (p == NULL) {
                size_t j = ca_stress_next() % live_count;
                cfree(a, live[j].ptr);
                live[j] = live[--live_count];
            } else {
                ca_stress_fill(p, size, seed);
                live[live_count++] = (ca_stress_block){p, size, seed};
            }
        }

        ca_stress_validate(a, live, live_count, op);

        if (op % 100 == 0) {
            for (size_t i = 0; i < live_count; i++)
                ca_stress_check(live[i].ptr, live[i].size, live[i].seed, op, "periodic");
        }
    }

    for (size_t i = 0; i < live_count; i++) {
        ca_stress_check(live[i].ptr, live[i].size, live[i].seed, CA_STRESS_OPS, "final");
        cfree(a, live[i].ptr);
    }

    ca_stress_validate(a, live, 0, CA_STRESS_OPS);

    CHECK_SIZE_EQ(a->used, 0);

    void *p = c_alloc(a, sizeof(arena.storage) - sizeof(CBlock));
    CHECK(p != NULL);
}