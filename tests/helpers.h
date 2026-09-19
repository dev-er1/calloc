#pragma once

#include <stddef.h>

#include "calloc.h"

#define CA_TEST_CAPACITY ((size_t)4096)

static inline size_t ca_test_align_up(size_t size) {
    size_t alignment = _Alignof(long double);
    size_t remainder = size % alignment;
    return remainder == 0 ? size : size + (alignment - remainder);
}

// Allocator over a stack-backed, correctly aligned memory region.
typedef struct {
    CAllocator allocator;
    _Alignas(_Alignof(long double)) unsigned char storage[CA_TEST_CAPACITY];
} ca_test_arena;

static inline CAllocator *ca_test_arena_init(ca_test_arena *arena) {
    cinit(&arena->allocator, arena->storage, sizeof(arena->storage));
    return &arena->allocator;
}
