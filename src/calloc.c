#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "calloc.h"

static const size_t CA_ALIGNMENT = _Alignof(long double);

// ============ Helpers ============

static bool ca_internal_block_is_live(const CAllocator *allocator, const CBlock *block);
static CBlock *ca_internal_merge_block(CAllocator *allocator, CBlock *block);

static CBlock *ca_internal_find_block(const CAllocator *allocator, void *ptr) {
    if (ptr == NULL || (uintptr_t)ptr % CA_ALIGNMENT != 0)
        return NULL;

    // Fast path: the block header sits right before the user pointer. A
    // block with at least one neighbour can be verified in O(1) via its
    // links; a singleton (no neighbours at all) is indistinguishable from
    // foreign data, so it falls back to the full walk.
    unsigned char *b = (unsigned char *)ptr - sizeof(CBlock);
    const unsigned char *begin = (const unsigned char *)allocator->memory;
    const unsigned char *end = begin + allocator->capacity;

    if (b >= begin && b < end) {
        CBlock *block = (CBlock *)b;

        if ((block->previous != NULL || block->next != NULL) &&
            ca_internal_block_is_live(allocator, block))
            return block;
    }

    CBlock *block = allocator->blocks;

    while (block != NULL) {
        if ((char *)block + sizeof(CBlock) == (char *)ptr)
            return block;

        block = block->next;
    }

    return NULL;
}

// Verifies in O(1) that a block pointer is a member of the block list by
// checking its neighbour links. All reads are bounds-checked against the
// arena so a stale or foreign pointer can never cause an out-of-bounds
// access; an ambiguous result is handled by the caller via a full walk.
static bool ca_internal_block_is_live(const CAllocator *allocator, const CBlock *block) {
    const unsigned char *begin = (const unsigned char *)allocator->memory;
    const unsigned char *end = begin + allocator->capacity;

    if ((const unsigned char *)block < begin ||
        (const unsigned char *)block >= end)
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

    return true;
}

// Grows a self-managed arena so its tail free space holds at least
// `needed` bytes. The region may move; every internal link is fixed up
// accordingly. Returns false when the region cannot be grown.
static bool ca_internal_grow(CAllocator *allocator, size_t needed) {
    size_t old_capacity = allocator->capacity;

    if (needed > SIZE_MAX - (sizeof(CBlock) + CA_ALIGNMENT))
        return false;

    size_t min_capacity = old_capacity + needed + sizeof(CBlock) + CA_ALIGNMENT;

    if (min_capacity < old_capacity)  // Overflow
        return false;

    size_t new_capacity = min_capacity;

    if (old_capacity <= SIZE_MAX / 2) {
        size_t doubled = old_capacity * 2;

        if (doubled > new_capacity)
            new_capacity = doubled;
    }

    void *new_memory = realloc(allocator->memory, new_capacity);

    if (new_memory == NULL)
        return false;

    ptrdiff_t delta = (char *)new_memory - (char *)allocator->memory;

    // Fix up all internal links that point into the region.
    allocator->blocks = (CBlock *)((char *)allocator->blocks + delta);

    if (allocator->hint != NULL)
        allocator->hint = (CBlock *)((char *)allocator->hint + delta);

    for (CBlock *block = allocator->blocks; block != NULL; block = block->next) {
        if (block->previous != NULL)
            block->previous = (CBlock *)((char *)block->previous + delta);

        if (block->next != NULL)
            block->next = (CBlock *)((char *)block->next + delta);
    }

    allocator->memory = new_memory;
    allocator->capacity = new_capacity;

    // Find the tail block and extend it, or append a new block after it.
    CBlock *tail = allocator->blocks;

    while (tail->next != NULL)
        tail = tail->next;

    size_t growth = new_capacity - old_capacity;

    if (tail->free) {
        tail->size += growth;
    } else {
        CBlock *new_tail = (CBlock *)((char *)allocator->memory + old_capacity);

        new_tail->size = growth - sizeof(CBlock);
        new_tail->next = NULL;
        new_tail->previous = tail;
        new_tail->free = true;

        tail->next = new_tail;
        tail = new_tail;
    }

    allocator->hint = tail;

    return true;
}



static size_t ca_internal_align_up(size_t size) {
    size_t remainder = size % CA_ALIGNMENT;

    if (remainder == 0)
        return size;

    if (size > SIZE_MAX - (CA_ALIGNMENT - remainder))
        return 0;

    return size + (CA_ALIGNMENT - remainder);
}

// Marks the block free, merging it with adjacent free blocks, and returns
// the surviving block (the merged region).
static CBlock *ca_internal_release_block(CAllocator *allocator, CBlock *block) {
    block->free = true;
    allocator->used -= block->size;

    block = ca_internal_merge_block(allocator, block);

    allocator->hint = block;

    return block;
}

static void *ca_internal_realloc_grow(CAllocator *allocator, CBlock *block, size_t aligned) {
    CBlock *next = block->next;

    if (next == NULL || !next->free)
        return NULL;

    size_t combined = block->size + sizeof(CBlock) + next->size;

    if (combined < aligned)
        return NULL;

    size_t remainder = combined - aligned;

    if (remainder >= sizeof(CBlock) + CA_ALIGNMENT) {
        CBlock *tail = (CBlock *)(
            (char *)block + sizeof(CBlock) + aligned
        );
        CBlock *next_next = next->next;

        tail->size = remainder - sizeof(CBlock);
        tail->next = next_next;
        tail->previous = block;
        tail->free = true;

        if (next_next != NULL)
            next_next->previous = tail;

        block->next = tail;

        ca_internal_merge_block(allocator, tail);

        allocator->hint = tail;

        allocator->used += aligned - block->size;
        block->size = aligned;
    } else {
        // The remainder is too small for a tail block, so the whole next
        // block is absorbed. Keeping only the aligned size would lose the
        // remainder forever, so the block claims the whole combined region.
        block->next = next->next;

        if (block->next != NULL)
            block->next->previous = block;

        allocator->used += combined - block->size;
        block->size = combined;
    }

    return (char *)block + sizeof(CBlock);
}

// Marks the block free, merging it with adjacent free blocks, and returns
// the surviving block (the merged region).
static CBlock *ca_internal_merge_block(CAllocator *allocator, CBlock *block) {
    if (block->next != NULL && block->next->free) {
        CBlock *next = block->next;

        block->size += sizeof(CBlock) + next->size;
        block->next = next->next;

        if (block->next != NULL)
            block->next->previous = block;
    }

    if (block->previous != NULL && block->previous->free) {
        CBlock *previous = block->previous;

        previous->size += sizeof(CBlock) + block->size;
        previous->next = block->next;

        if (previous->next != NULL)
            previous->next->previous = previous;

        return previous;
    }

    return block;
}

// =================================

// calloc.h:61
void cinit(CAllocator *allocator, void *memory, size_t capacity) {
    if (allocator == NULL)
        return;

    allocator->memory = NULL;
    allocator->capacity = 0;
    allocator->used = 0;
    allocator->blocks = NULL;
    allocator->hint = NULL;
    allocator->own_memory = false;
    allocator->initialized = false;

    if (capacity < sizeof(CBlock))
        return;

    if (memory == NULL) {
        // Self-managed arena: allocate the initial region ourselves.
        allocator->own_memory = true;
        memory = malloc(capacity);

        if (memory == NULL)
            return;
    } else if (((uintptr_t)memory + sizeof(CBlock)) % CA_ALIGNMENT != 0) {
        return;
    }

    allocator->memory = memory;
    allocator->capacity = capacity;
    allocator->used = 0;

    allocator->blocks = (CBlock *)memory;

    allocator->blocks->size = capacity - sizeof(CBlock);
    allocator->blocks->next = NULL;
    allocator->blocks->previous = NULL;
    allocator->blocks->free = true;

    allocator->hint = allocator->blocks;

    allocator->initialized = true;
}

// calloc.h:71
void *c_alloc(CAllocator *allocator, size_t size) {
    if (allocator == NULL || size == 0
        || !allocator->initialized)
        return NULL;

    size = ca_internal_align_up(size);

    if (size == 0)
        return NULL;

    CBlock *start = allocator->hint;

    if (start == NULL || !ca_internal_block_is_live(allocator, start))
        start = allocator->blocks;

    // Bounded scan: protects against a corrupted hint chain. The real list
    // can never exceed this budget (every block takes at least header +
    // one alignment unit), so a real list is always fully scanned.
    size_t budget = allocator->capacity / (sizeof(CBlock) + CA_ALIGNMENT) + 2;

    CBlock *block = start;
    CBlock *found = NULL;

    while (block != NULL && budget-- > 0) {
        if (block->free && block->size >= size) {
            found = block;
            break;
        }

        block = block->next;
    }

    if (found == NULL && start != allocator->blocks) {
        // Wrap around: scan from the head up to the search start.
        block = allocator->blocks;

        while (block != NULL && block != start) {
            if (block->free && block->size >= size) {
                found = block;
                break;
            }

            block = block->next;
        }
    }

    if (found == NULL) {
        // A self-managed arena grows on demand; the hint now points at the
        // free tail block, which is guaranteed to hold at least `size`.
        if (!allocator->own_memory || !ca_internal_grow(allocator, size))
            return NULL;

        found = allocator->hint;
    }

    CBlock *take = found;

    // Split only if the remainder can hold a usable free block.
    if (take->size - size >= sizeof(CBlock) + CA_ALIGNMENT) {
        CBlock *next = (CBlock *)(
            (char *)take + sizeof(CBlock) + size
        );

        next->size = take->size - size - sizeof(CBlock);
        next->next = take->next;
        next->previous = take;
        next->free = true;

        if (next->next != NULL)
            next->next->previous = next;

        take->size = size;
        take->next = next;

        allocator->hint = next;
    } else {
        allocator->hint = take->next;
    }

    take->free = false;
    allocator->used += take->size;

    return (char *)take + sizeof(CBlock);
}

void cfree(CAllocator *allocator, void *ptr) {
    if (allocator == NULL || !allocator->initialized)
        return;

    CBlock *block = ca_internal_find_block(allocator, ptr);

    if (block == NULL || block->free)
        return;

    ca_internal_release_block(allocator, block);
}

// calloc.h:94
void *crealloc(CAllocator *allocator, void *ptr, size_t size) {
    if (allocator == NULL || !allocator->initialized)
        return NULL;

    if (ptr == NULL)
        return c_alloc(allocator, size);

    CBlock *block = ca_internal_find_block(allocator, ptr);

    if (block == NULL || block->free)
        return NULL;

    if (size == 0) {
        cfree(allocator, ptr);
        return NULL;
    }

    size_t aligned = ca_internal_align_up(size);

    if (aligned == 0)
        return NULL;

    if (aligned <= block->size) {
        if (block->size - aligned >= sizeof(CBlock) + CA_ALIGNMENT) {
            size_t old_size = block->size;

            CBlock *tail = (CBlock *)(
                (char *)block + sizeof(CBlock) + aligned
            );

            tail->size = block->size - aligned - sizeof(CBlock);
            tail->next = block->next;
            tail->previous = block;
            tail->free = true;

            if (tail->next != NULL)
                tail->next->previous = tail;

            block->size = aligned;
            block->next = tail;

            allocator->used -= old_size - aligned;

            ca_internal_merge_block(allocator, tail);

            allocator->hint = tail;
        }

        return ptr;
    }

    void *in_place = ca_internal_realloc_grow(allocator, block, aligned);

    if (in_place != NULL)
        return in_place;

    void *next;

    if (allocator->own_memory) {
        // The new allocation may grow the region and relocate it, which
        // would invalidate `ptr`. Copy the data out first, then release
        // the original block through its header offset in the (possibly
        // new) base; the header contents survive the relocation.
        ptrdiff_t offset = (char *)block - (char *)allocator->memory;
        size_t old_size = block->size;
        void *copy = malloc(old_size);

        if (copy == NULL)
            return NULL;

        memcpy(copy, ptr, old_size);

        next = c_alloc(allocator, size);

        if (next == NULL) {
            free(copy);
            return NULL;
        }

        memcpy(next, copy, old_size);
        free(copy);

        ca_internal_release_block(
            allocator, (CBlock *)((char *)allocator->memory + offset)
        );
    } else {
        next = c_alloc(allocator, size);

        if (next == NULL)
            return NULL;

        memcpy(next, ptr, block->size);

        cfree(allocator, ptr);
    }

    return next;
}

// calloc.h:101
void creset(CAllocator *allocator) {
    if (allocator == NULL ||
        !allocator->initialized)
        return;

    allocator->used = 0;

    allocator->blocks = (CBlock *)allocator->memory;

    allocator->blocks->size =
        allocator->capacity - sizeof(CBlock);

    allocator->blocks->next = NULL;
    allocator->blocks->previous = NULL;
    allocator->blocks->free = true;

    allocator->hint = allocator->blocks;
}

// calloc.h:109
void cdestroy(CAllocator *allocator) {
    if (allocator == NULL)
        return;

    if (allocator->own_memory && allocator->memory != NULL)
        free(allocator->memory);

    allocator->memory = NULL;
    allocator->capacity = 0;
    allocator->used = 0;
    allocator->blocks = NULL;
    allocator->hint = NULL;
    allocator->own_memory = false;
    allocator->initialized = false;
}
