# base.h

My personal single-file base layer for C11: arena allocation, length-based strings, growable arrays, and a pattern for hash maps.

It's written in the "handmade" / data-oriented style and heavily inspired by [Chris Wellons](https://nullprogram.com) and Ryan Fleury's base layer in the [RAD Debugger](https://github.com/EpicGames/raddebugger).

## Quick start

Copy `base.h` into your project and include it.

```c
#include "base.h"

typedef Array(Str8) Str8Array;

internal Str8Array split(Arena *a, Str8 s, u8 sep) {
    Str8Array parts = {0};
    for (Str8Cut c = {.tail = s}; c.tail.len;) {
        c = str8_cut(c.tail, sep);
        *array_push(a, &parts) = str8_trim(c.head);
    }
    return parts;
}

int main(void) {
    Arena arena = arena_make(MB(64));

    Str8Array fruits = split(&arena, Str8("apple, banana ,cherry"), ',');
    for (isize i = 0; i < fruits.len; i++) {
        printf("%.*s\n", str8_varg(fruits.data[i]));
    }
}
```

```sh
cc -std=c11 -Wall -Wextra -Wno-unused-function main.c -o main
```

## What's inside

- **Arenas.** Bump allocation with `new(&arena, T, n)`, zeroed by default. Child arenas, and save/restore by copying a struct. No `free`.
- **Strings.** `Str8` is a pointer and a length: not null-terminated, and slicing never copies. Includes `cut`, `trim`, `slice`, `eq`, `copy`, `concat` (in place when possible), `fmt` (printf into an arena) and `hash`.
- **Dynamic arrays.** `Array(T)` with `array_push`, growing inside an arena.
- **Hash maps.** A 15-line hash-trie pattern you write once per key/value type, based on [Wellons's design](https://nullprogram.com/blog/2023/09/30/).

## Design

- **Zero is initialization.** `{0}` is always a valid, empty value: an empty string, an empty array or an empty map is just zero.
- **Arenas are passed explicitly.** `Arena *a` is for results the caller keeps. `Arena scratch` is passed by value, so anything a function allocates in it is discarded automatically when the function returns.
- **Signed sizes.** Lengths and indices are `isize` (`ptrdiff_t`), so subtracting two sizes can't silently wrap around to a huge number.
- **Out of memory aborts.** No allocation returns null, so there's nothing to check.

## Documentation

The full reference (every function, conventions, idioms and gotchas) is in the comment at the top of [`base.h`](base.h).

## Tests

```sh
make test
```

The tests run under AddressSanitizer and UndefinedBehaviorSanitizer. They have no framework: just a `CHECK` macro and a table of test functions.

## Inspiration and further reading

- [Arena allocator tips and tricks](https://nullprogram.com/blog/2023/09/27/)
- [An easy-to-implement, arena-friendly hash map](https://nullprogram.com/blog/2023/09/30/)
- [A simple, arena-backed, generic dynamic array for C](https://nullprogram.com/blog/2023/10/05/)
- [Examples of quick hash tables and dynamic arrays in C](https://nullprogram.com/blog/2025/01/19/)

## License

Dual-licensed under MIT or public domain (the Unlicense), at your choice. See the end of [`base.h`](base.h) or [`LICENSE`](LICENSE).