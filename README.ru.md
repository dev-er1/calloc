# CAllocator
CAllocator (сокращённо «CAlloc») — **sequential-fit memory allocator общего назначения** написанный на C (отсюда и название «C Allocator»).

## Содержание
- [API](#api)
- [Пример использования](#пример-использования)
- [Лицензия](#лицензия)

## API
```c
void cinit(CAllocator *allocator, void *memory, size_t capacity);
void *c_alloc(CAllocator *allocator, size_t size);
void cfree(CAllocator *allocator, void *ptr);
void *crealloc(CAllocator *allocator, void *ptr, size_t size);
void creset(CAllocator *allocator);
void cdestroy(CAllocator *allocator);
```

- **`cinit`** — инициализирует allocator над переданной областью памяти. Арена должна быть выровнена под `CA_ALIGNMENT`.
    При `memory == NULL` и достаточной ёмкости allocator сам выделяет начальную область и может расти по мере необходимости.
- **`c_alloc`** — выделяет блок минимум `size` байт; возвращает `NULL` при исчерпании арены (внешняя) или при невозможности
    роста.
- **`cfree`** — освобождает блок, ранее возвращённый `c_alloc` или `crealloc`; `NULL` и повторное освобождение игнорируются.
- **`crealloc`** — изменяет размер блока: растёт на месте, когда возможно, иначе выделяет новый блок и копирует содержимое.
    При неудаче исходный блок не трогается.
- **`creset`** — возвращает арену в начальное состояние (один свободный блок на всю область).
- **`cdestroy`** — освобождает область и выключает allocator. Для самовладеющих арен обязателен; для внешних — только
    сбрасывает состояние.

## Пример использования
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

Использовать allocator можно двумя способами: скопировать `src/calloc.h` и `src/calloc.c` прямо в проект либо слинковать
предсобранную статическую библиотеку.

## Сборка и проверка
Требуется [xmake](https://xmake.io):

```
xmake f -m release
xmake build
```

Apache License 2.0 — см. [LICENSE](LICENSE).
