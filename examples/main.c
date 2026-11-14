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

    Str8Array fruits = split(&arena, S8("apple, banana ,cherry"), ',');
    for (isize i = 0; i < fruits.len; i++) {
        printf("%.*s\n", str8_varg(fruits.data[i]));
    }
}