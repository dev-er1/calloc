# CAllocator
CAllocator (short for "CAlloc") is a **general-purpose sequential-fit memory allocator** written in C (hence the name "C Allocator").

## Contents
- [API](#api)
- [Usage Example](#usage-example)
- [License](#license)

## API
```c
void cinit(CAllocator *allocator, void *memory, size_t capacity);
void *c_alloc(CAllocator *allocator, size_t size);
void cfree(CAllocator *allocator, void *ptr);
void *crealloc(CAllocator *allocator, void *ptr, size_t size);
void creset(CAllocator *allocator);
void cdestroy(CAllocator *allocator);
```

- **`cinit`** — initializes the allocator over the given memory region. The arena must be aligned to `CA_ALIGNMENT`.
    When `memory == NULL` and the capacity is sufficient, the allocator allocates the initial region itself and can grow as needed.
- **`c_alloc`** — allocates a block of at least `size` bytes; returns `NULL` when the arena is exhausted (external) or when growth
    is not possible.
- **`cfree`** — frees a block previously returned by `c_alloc` or `crealloc`; `NULL` and double free are ignored.
- **`crealloc`** — resizes a block: grows in place when possible, otherwise allocates a new block and copies the contents.
    On failure the original block is left untouched.
- **`creset`** — returns the arena to its initial state (a single free block spanning the whole region).
- **`cdestroy`** — frees the region and shuts down the allocator. Required for self-owned arenas; for external ones it only
    resets the state.

## Usage Example
```c
#include <calloc.h>

static unsigned char arena_memory[256 * 1024];

int main(void) {
    CAllocator arena;
    cinit(&arena, arena_memory, sizeof(arena_memory));

    void *p = c_alloc(&arena, 1024);
    p = crealloc(&arena, p, 2048);
    cfree(&arena, p);
    creset(&arena);
}
```

There are two ways to use the allocator: copy `src/calloc.h` and `src/calloc.c` directly into your project, or link against
the prebuilt static library.

## Build and Verify
Requires [xmake](https://xmake.io):

```
xmake f -m release
xmake build
```

Apache License 2.0 — see [LICENSE](LICENSE).
