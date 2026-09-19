# CAllocator Architecture
This document describes the architecture of CAllocator.

## Contents
- [CAllocator Structure](#callocator-structure)
  - [`CBlock`](#cblock)
- [Memory Layout](#memory-layout)
- [Block List](#block-list)
- [Memory Allocation](#memory-allocation)
- [Deallocation](#deallocation)
- [Resizing](#resizing)
- [Growing the Self-Owning Arena](#growing-the-self-owning-arena)
- [Limitations](#limitations)

## CAllocator Structure
```c
typedef struct CBlock {
    size_t size;
    struct CBlock *next;
    struct CBlock *previous;
    bool free;
} CBlock;

typedef struct CAllocator {
    void *memory;
    size_t capacity;
    size_t used;
    CBlock *blocks;
    CBlock *hint;
    bool own_memory;
    bool initialized;
} CAllocator;
```

`CAllocator` consists of:
- `*memory` — a pointer to the beginning of the managed memory area;
- `capacity` — the size of the managed memory area in bytes. `capacity` includes both the user areas of the blocks and the
  `CBlock` headers occupied by them;
- `used` — the amount of memory currently allocated to the user;
- `*blocks` — a pointer to the first [`CBlock`](#cblock) (hereinafter simply a “block”) of the doubly linked list of blocks;
- `*hint` — a pointer to the block from which the next search for a free block begins;
- `own_memory` — determines whether the managed memory area belongs to the allocator;
- `initialized` — determines whether the allocator has been initialized; until the flag is set, all operations with the allocator
  are ignored.

### `CBlock`
`CBlock` is the metadata of a managed memory block. The metadata is located directly before the user data. Fields:
- `size` — the size of memory available to the user in bytes;
- `*next` — a pointer to the next block;
- `*previous` — a pointer to the previous block;
- `free` — whether the block is free.

Blocks lie consecutively and cover the entire area without gaps; all pointers returned by `c_alloc` are aligned to `_Alignof(long
double)`. The sum of `sizeof(CBlock) + size` over all blocks is always equal to `capacity` — this invariant is checked by the
validator after each operation in stress tests.

## Memory Layout
```c
static const size_t CA_ALIGNMENT = _Alignof(long double);
```

The allocator manages a contiguous `memory` area of `capacity` bytes. Blocks are laid out in memory as follows (ASCII diagram
below):

```text
┌──────────────┬────────────────┐
│   CBlock     │  user data     │
└──────────────┴────────────────┘
```

User data begins immediately after the header; for an external arena, `cinit` requires `memory + sizeof(CBlock)` to be aligned,
otherwise initialization is rejected (for a self-owning arena, `malloc` guarantees alignment).

The layout implies the following invariants: the sum of `sizeof(CBlock) + size` over all blocks is equal to `capacity`, and `used` is equal to the sum of `size` of all occupied blocks.

When a block is freed, it is merged with free neighboring blocks. When merging, the header of the absorbed block ceases to exist
as a separate element of the list.

A self-owning arena can expand the managed memory area. When expanding, the address of `memory` may change, so internal pointers
to blocks must be adjusted.

## Block List
Each block belongs to a single doubly linked list starting at `CAllocator::blocks`. The order of blocks in the list corresponds
to their physical order in the managed memory area: `next` points to the next block, while `previous` points to the previous one.

The first block has `previous == NULL`, the last one — `next == NULL`.

The `next` and `previous` pointers are not used to determine the physical location of a block; they maintain the doubly linked
list and allow O(1) traversal to neighboring blocks.

## Memory Allocation
The request is rounded up to `CA_ALIGNMENT` (overflow — `NULL`). The search for a free block starts at `*hint`: it is checked
in O(1) using the neighboring links; on failure, the search starts from the head of the list. Scanning is limited by a budget of
`capacity / (sizeof(CBlock) + CA_ALIGNMENT) + 2` — the actual list cannot be longer, so a full traversal is always performed.

If a suitable block is not found by the end of the list, the search wraps around: it continues from the head to the starting block. If a block is still not found:
- external arena — `NULL` is returned;
- self-owning arena — growth is performed (see “Growing the Self-Owning Arena”).

The found block is marked as occupied, `used` is increased by its `size`. If `block->size - size >= sizeof(CBlock) + CA_ALIGNMENT`, the remainder is made into a separate free block; otherwise the entire block is returned. `*hint` points to the
new free block or to the one following the allocated block.

The worst case is a full traversal of the list, O(n); with sequential allocations, the search starts from the position
of the last operation and finds a block in O(1).

## Deallocation
The data lies immediately after the header, so the header is found as `ptr - sizeof(CBlock)` and checked in O(1) by the
boundaries of the area and the `*next`/`*previous` links; if the check is ambiguous (a block without neighbors), a full traversal
is performed. `NULL`, a foreign pointer, and repeated deallocation are ignored.

The block is marked as free, `used` is decreased by its `size`, after which it is merged with free neighbors: first with the
right one, then with the left one. The neighboring free block is absorbed together with its header. `*hint` points to the block
surviving the merge.

Consequence: free blocks never border each other.

## Resizing
`ptr == NULL` — same as `c_alloc`; `size == 0` — same as `cfree`.

Shrinking: if the remainder `block->size - aligned >= sizeof(CBlock) + CA_ALIGNMENT`, the tail is cut off as a free block and
merged with the right neighbor; `*hint` points to this tail. Otherwise the block is unchanged.

Growing:

- in place — if the right neighbor is free and `combined = block->size + sizeof(CBlock) + next->size` is sufficient, the block
    absorbs the neighbor; the remainder is made into a free block (and merged with the right neighbor) or, if it is smaller than
    the threshold, absorbed entirely;
- moving — new `c_alloc` + copying data + freeing the old block. For a self-owning arena, `c_alloc` may trigger growth, and
    growth may move the area: the old `ptr` becomes invalid. Therefore, the data is first copied into a temporary buffer, then
    `c_alloc` is performed, the data is copied back, and the old block is freed using the saved header offset in the new base.
    Failure of `c_alloc` leaves the original block untouched.

## Growing the Self-Owning Arena
Called when the search in `c_alloc` does not find a suitable block. The new capacity is
`max(old * 2, old + needed + sizeof(CBlock) + CA_ALIGNMENT)` with overflow checks. The area is reallocated through `realloc`:
- if the area moved, all internal references (`*blocks`, `*hint`, `*next`, `*previous`) are recalculated by the shift amount —
    headers remain at their previous offsets;
- added bytes: the free tail block is enlarged, or a new free block is created at the end;
- `*hint` points to the tail.

Pointers obtained by the user before growth may become invalid (the area moved); occupied blocks are not lost in the process and
are freed during `creset` or `cdestroy`.
