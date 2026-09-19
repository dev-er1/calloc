#pragma once

#include <stddef.h>
#include <stdbool.h>

/// @brief Memory block managed by CAllocator.
///
/// Contains allocator metadata followed by the memory available to the user.
typedef struct CBlock {
    /// @brief Size of the user-accessible memory in bytes.
    size_t size;

    /// @brief Pointer to the next block in the block list.
    struct CBlock *next;

    /// @brief Pointer to the previous block in the block list.
    struct CBlock *previous;

    /// @brief Non-zero if the block is currently free.
    bool free;
} CBlock;

/// @brief Memory allocator state.
///
/// Manages a contiguous memory region divided into memory blocks.
typedef struct CAllocator {
    /// @brief Beginning of the memory region managed by the allocator.
    void *memory;

    /// @brief Total size of the managed memory region in bytes.
    size_t capacity;

    /// @brief Total amount of memory currently allocated to the user.
    size_t used;

    /// @brief First block in the allocator's block list.
    CBlock *blocks;

    /// @brief Search hint for the next free block (lazily validated).
    CBlock *hint;

    /// @brief Non-zero if the allocator owns and grows its own memory region.
    bool own_memory;

    /// @brief Non-zero if the allocator has been initialized.
    bool initialized;
} CAllocator;

/// @brief Initializes an allocator over an existing memory region.
///
/// Pass `NULL` as `memory` to create a self-managed arena: the allocator
/// allocates its own region and grows it on demand. Note that the region
/// may move when it grows, invalidating pointers to blocks allocated
/// before the growth; release such an arena with `cdestroy()`.
///
/// @param allocator Allocator state to initialize.
/// @param memory Beginning of the memory region, or `NULL` for a
///               self-managed arena.
/// @param capacity Size of the memory region in bytes (for a self-managed
///                 arena: the initial capacity).
void cinit(CAllocator *allocator, void *memory, size_t capacity);

/// @brief Allocates a block of memory.
///
/// Searches for a suitable free block and splits it if necessary.
///
/// @param allocator Initialized allocator.
/// @param size Number of bytes to allocate.
///
/// @return Pointer to allocated memory, or `NULL` if allocation fails.
void *c_alloc(CAllocator *allocator, size_t size);

/// @brief Releases a previously allocated block.
///
/// Marks the block as free and may merge it with adjacent free blocks.
///
/// @param allocator Allocator that owns the block.
/// @param ptr Pointer to the user memory previously returned by `c_alloc()`.
void cfree(CAllocator *allocator, void *ptr);

/// @brief Resizes a previously allocated block.
///
/// Shrinks or grows the block in place when possible, otherwise allocates
/// a new block and copies the contents. Passing `NULL` as `ptr` allocates
/// a new block; passing `0` as `size` releases the block.
///
/// @param allocator Allocator that owns the block.
/// @param ptr Pointer to the user memory previously returned by `c_alloc()`,
///            or `NULL` to allocate a new block.
/// @param size New size in bytes, or `0` to release the block.
///
/// @return Pointer to the resized memory, or `NULL` on failure. The original
///         block is left untouched when the operation fails.
void *crealloc(CAllocator *allocator, void *ptr, size_t size);

/// @brief Resets the allocator to its initial state.
///
/// Marks the entire managed memory region as one free block.
///
/// @param allocator Allocator to reset.
void creset(CAllocator *allocator);

/// @brief Destroys an allocator, releasing its managed memory.
///
/// Only affects self-managed arenas created with `cinit(NULL, ...)`;
/// for allocators over caller-provided memory it just resets the state.
///
/// @param allocator Allocator to destroy.
void cdestroy(CAllocator *allocator);
