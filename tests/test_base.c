#define _POSIX_C_SOURCE 200809L
#include "base.h"

global i32 test_checks;
global i32 test_failures;

#define CHECK(c) do {                                               \
        test_checks++;                                              \
        if (!(c)) {                                                 \
            test_failures++;                                        \
            printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
        }                                                           \
    } while (0)                                                     \

#if defined(__unix__) || defined(__APPLE__)
#  include <sys/wait.h>
#  include <unistd.h>
#  define HAVE_DIES 1
internal b32 dies(void (*fn)(Arena *), Arena *arena) {
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stderr);
        fn(arena);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFSIGNALED(status);
}
#else
#  define HAVE_DIES 0
#endif

internal Arena small_arena(Arena *scratch, isize cap) {
    return arena_sub(scratch, cap);
}

internal b32 is_zero(void *p, isize n) {
    u8 *bytes = p;
    for (isize i = 0; i < n; i++) {
        if (bytes[i]) return 0;
    }
    return 1;
}

internal void test_mem(Arena scratch) {
    (void)scratch;
    u8 a[4] = {1, 2, 3, 4};
    u8 b[4] = {0};

    mem_copy(b, a, 4);
    CHECK(mem_eq(a, b, 4));

    mem_set(b, 0xAB, 4);
    CHECK(b[0] == 0xAB && b[3] == 0xAB);
    CHECK(!mem_eq(a, b, 4));

    mem_copy(0, 0, 0);
    mem_set(0, 0, 0);
    CHECK(mem_eq(0, 0, 0));
}

internal void test_macros(Arena scratch) {
    (void)scratch;
    i32 xs[7];
    CHECK(countof(xs) == 7);
    CHECK(sizeof(u64) == 8);
    CHECK(KB(1) == 1024);
    CHECK(MB(2) == 2 * 1024 * 1024);
}

internal void test_arena_zeroing(Arena scratch) {
    Arena saved = scratch;
    u8 *dirty = new(&scratch, u8, 64, 0);
    mem_set(dirty, 0xFF, 64);
    scratch = saved;

    u8 *clean = new(&scratch, u8, 64);
    CHECK(clean == dirty);
    CHECK(is_zero(clean, 64));

    scratch = saved;
    mem_set(dirty, 0xFF, 64);
    u8 *raw = new(&scratch, u8, 64, 0);
    CHECK(raw[0] == 0xFF);
}

internal void test_arena_alignment(Arena scratch) {
    typedef struct { u8 c; f64 d; }Aligned;

    new(&scratch, u8, 1);
    u64 *p64 = new(&scratch, u64);
    CHECK((uptr)p64 % alignof(u64) == 0);

    new(&scratch, u8, 3);
    Aligned *pa = new(&scratch, Aligned, 4);
    CHECK((uptr)pa % alignof(Aligned) == 0);

    new(&scratch, u8, 1);
    i32 *p32 = new(&scratch, i32);
    CHECK((uptr)p32 % alignof(i32) == 0);
}

internal void test_arena_sequential(Arena scratch) {
    u8 *a = new(&scratch, u8, 10);
    u8 *b = new(&scratch, u8, 10);
    CHECK(b == a + 10);
}

internal void test_arena_sub(Arena scratch) {
    u8 *before = scratch.beg;
    Arena child = arena_sub(&scratch, 100);
    CHECK(child.end - child.beg == 100);
    CHECK(scratch.beg == before + 100);
    CHECK(child.beg == before);
}

internal void test_arena_save_restore(Arena scratch) {
    u8 *start = scratch.beg;
    Arena saved = scratch;
    new(&scratch, u8, 1000);
    CHECK(scratch.beg != start);
    scratch = saved;
    CHECK(scratch.beg == start);
}

internal void use_scratch(Arena scratch) {
    new(&scratch, u8, 500);
}

internal void test_arena_scratch_by_value(Arena scratch) {
    u8 *before = scratch.beg;
    use_scratch(scratch);
    CHECK(scratch.beg == before);
}

internal void fill_64(Arena *a)    { new(a, u8, 64); }
internal void fill_65(Arena *a)    { new(a, u8, 65); }
internal void fill_twice(Arena *a) { new(a, u8, 40); new(a, u8, 40); }

internal void test_arena_out_of_memory(Arena scratch) {
#if HAVE_DIES
    Arena a = small_arena(&scratch, 64);
    CHECK(!dies(fill_64, &a));
    CHECK(dies(fill_65, &a));
    CHECK(dies(fill_twice, &a));

    Arena zero = {0};
    CHECK(dies(fill_64, &zero));
#else
    (void)scratch;
#endif
}

internal void test_str8_basics(Arena scratch) {
    (void)scratch;
    Str8 s = S8("hello");
    CHECK(s.len == 5);
    CHECK(str8_cstr("hello").len == 5);
    CHECK(str8_eq(s, str8_cstr("hello")));
    CHECK(!str8_eq(s, S8("hell0")));
    CHECK(!str8_eq(s, S8("not hello")));

    Str8 empty = S8("");
    Str8 zero = {0};
    CHECK(empty.len == 0);
    CHECK(str8_eq(empty, zero));
    CHECK(str8_eq(str8_cstr(0), zero));
}

internal void test_str8_slice(Arena scratch) {
    (void)scratch;
    Str8 s = S8("hello world");
    CHECK(str8_eq(str8_slice(s, 0, 5), S8("hello")));
    CHECK(str8_eq(str8_slice(s, 6, 11), S8("world")));
    CHECK(str8_slice(s, 3, 3).len == 0);
    CHECK(str8_eq(str8_slice(s, 0, s.len), s));
    CHECK(str8_slice(s, 6, 11).buf == s.buf + 6);
}

internal void test_str8_trim(Arena scratch) {
    (void)scratch;
    CHECK(str8_eq(str8_trim(S8("  hi  ")), S8("hi")));
    CHECK(str8_eq(str8_trim(S8("\t\r\n hi\n")), S8("hi")));
    CHECK(str8_eq(str8_trim(S8("a b")), S8("a b")));
    CHECK(str8_trim(S8("   ")).len == 0);
    CHECK(str8_trim(S8("")).len == 0);
}

internal void test_str8_cut(Arena scratch) {
    (void)scratch;
    Str8Cut c = str8_cut(S8("key=value"), '=');
    CHECK(c.ok);
    CHECK(str8_eq(c.head, S8("key")));
    CHECK(str8_eq(c.tail, S8("value")));

    c = str8_cut(S8("novalue"), '=');
    CHECK(!c.ok);
    CHECK(str8_eq(c.head, S8("novalue")));
    CHECK(c.tail.len == 0);

    c = str8_cut(S8("trailing="), '=');
    CHECK(c.ok && c.tail.len == 0);

    c = str8_cut(S8("=leading"), '=');
    CHECK(c.ok && c.head.len == 0);

    Str8  parts[8];
    isize count = 0;
    for (Str8Cut it = {.tail = S8("a,b,,c")}; it.tail.len;) {
        it = str8_cut(it.tail, ',');
        parts[count++] = it.head;
    }
    CHECK(count == 4);
    CHECK(str8_eq(parts[0], S8("a")));
    CHECK(parts[2].len == 0);
    CHECK(str8_eq(parts[3], S8("c")));
}

internal void test_str8_hash(Arena scratch) {
    Str8 a = S8("hello");
    Str8 b = str8_copy(&scratch, a);
    CHECK(str8_hash(a) == str8_hash(b));
    CHECK(str8_hash(S8("ab")) != str8_hash(S8("ba")));
    CHECK(str8_hash(S8("a")) != str8_hash(S8("")));
}

internal void test_str8_copy(Arena scratch) {
    Str8 s = S8("copy me");
    Str8 c = str8_copy(&scratch, s);
    CHECK(str8_eq(s, c));
    CHECK(c.buf != s.buf);

    Str8 e = str8_copy(&scratch, S8(""));
    CHECK(e.buf == 0 && e.len == 0);

    char *z = str8_to_cstring(&scratch, S8("abc"));
    CHECK(strcmp(z, "abc") == 0);
    z = str8_to_cstring(&scratch, (Str8){0});
    CHECK(z[0] == 0);
}

internal void test_str8_concat(Arena scratch) {
    Str8 s = str8_concat(&scratch, (Str8){0}, S8("ab"));
    CHECK(str8_eq(s, S8("ab")));

    u8 *buf = s.buf;
    s = str8_concat(&scratch, s, S8("cd"));
    CHECK(str8_eq(s, S8("abcd")));
    CHECK(s.buf == buf);

    new(&scratch, u8);
    s = str8_concat(&scratch, s, S8("ef"));
    CHECK(str8_eq(s, S8("abcdef")));
    CHECK(s.buf != buf);

    s = str8_concat(&scratch, S8("x"), S8("y"));
    CHECK(str8_eq(s, S8("xy")));

    s = str8_concat(&scratch, s, (Str8){0});
    CHECK(str8_eq(s, S8("xy")));

    Str8 built = {0};
    for (i32 i = 0; i < 100; i++) {
        built = str8_concat(&scratch, built, S8("ab"));
    }
    CHECK(built.len == 200);
    CHECK(built.buf[0] == 'a' && built.buf[199] == 'b');
}

internal void fmt_hello(Arena *a) { str8_fmt(a, "%s", "hello"); }

internal void test_str8_fmt(Arena scratch) {
    Str8 s = str8_fmt(&scratch, "%d-%s", 42, "x");
    CHECK(str8_eq(s, S8("42-x")));

    Str8 name = S8("bob");
    s = str8_fmt(&scratch, "hi " STR8_FMT "!", str8_varg(name));
    CHECK(str8_eq(s, S8("hi bob!")));

    u8 *next = new(&scratch, u8);
    CHECK(next == s.buf + s.len);
#if HAVE_DIES
    Arena fits   = small_arena(&scratch, 6);
    Arena short_ = small_arena(&scratch, 5);
    CHECK(!dies(fmt_hello, &fits));
    CHECK(dies(fmt_hello, &short_));
#endif
}

typedef Array(i32) I32Array;

internal void test_array_push(Arena scratch) {
    I32Array xs = {0};
    for (i32 i = 0; i < 1000; i++) {
        *array_push(&scratch, &xs) = i;
    }
    CHECK(xs.len == 1000);
    CHECK(xs.cap >= xs.len);
    b32 all_ok = 1;
    for (i32 i = 0; i < 1000; i++) {
        all_ok &= xs.data[i] == i;
    }
    CHECK(all_ok);
}

internal void test_array_zeroed(Arena scratch) {
    Arena saved = scratch;
    mem_set(new(&scratch, u8, 4096), 0xFF, 4096);
    scratch = saved;

    I32Array xs = {0};
    b32 all_zero = 1;
    for (i32 i = 0; i < 100; i++) {
        all_zero &= *array_push(&scratch, &xs) == 0;
    }
    CHECK(all_zero);
}

internal void test_array_growth_paths(Arena scratch) {
    I32Array xs = {0};
    for (i32 i = 0; i < 8; i++) *array_push(&scratch, &xs) = i;
    CHECK(xs.cap == 8);

    i32 *data = xs.data;
    *array_push(&scratch, &xs) = 8;
    CHECK(xs.data == data);
    CHECK(xs.cap == 16);

    for (i32 i = 9; i < 16; i++) *array_push(&scratch, &xs) = i;
    new(&scratch, u8);
    *array_push(&scratch, &xs) = 16;
    CHECK(xs.data != data);
    CHECK(xs.cap == 32);
    b32 all_ok = 1;
    for (i32 i = 0; i <= 16; i++) all_ok &= xs.data[i] == i;
    CHECK(all_ok);
}

internal void test_array_interleaved(Arena scratch) {
    I32Array a = {0}, b = {0};
    for (i32 i = 0; i < 5000; i++) {
        *array_push(&scratch, &a) = i;
        *array_push(&scratch, &b) = -i;
    }
    b32 all_ok = a.len == 5000 && b.len == 5000;
    for (i32 i = 0; i < 5000; i++) {
        all_ok &= a.data[i] == i && b.data[i] == -i;
    }
    CHECK(all_ok);
}

internal void test_array_struct_alignment(Arena scratch) {
    typedef struct { u8 tag; f64 value; } Item;
    typedef Array(Item) ItemArray;

    new (&scratch, u8, 1);
    ItemArray items = {0};
    for (i32 i = 0; i < 100; i++) {
        Item *it = array_push(&scratch, &items);
        it->tag = (u8)i;
        it->value = i * 0.5;
        new(&scratch, u8, 3);
    }
    CHECK((uptr)items.data % alignof(Item) == 0);
    CHECK(items.data[99].tag == 99 && items.data[99].value == 49.5);
}

internal void test_array_slice_push_copies(Arena scratch) {
    I32Array xs = {0};
    for (i32 i = 0; i < 10; i++) *array_push(&scratch, &xs) = i;

    I32Array part = xs;
    part.data += 2;
    part.len = part.cap = 3;

    *array_push(&scratch, &part) = 99;
    CHECK(xs.data[5] == 5);
    CHECK(part.len == 4 && part.data[0] == 2 && part.data[3] == 99);
}

internal void test_array_empty_slice_push(Arena scratch) {
    I32Array xs = {0};
    for (i32 i = 0; i < 4; i++) *array_push(&scratch, &xs) = i;

    I32Array empty = xs;
    empty.data += 2;
    empty.len = empty.cap = 0;

    new(&scratch, u8);
    for (i32 i = 0; i < 3; i++) *array_push(&scratch, &empty) = 100 + i;

    u8 *later = new(&scratch, u8, 64, 0);
    mem_set(later, 0xAA, 64);

    CHECK(empty.cap >= empty.len);
    CHECK(empty.data[0] == 100 && empty.data[1] == 101 && empty.data[2] == 102);
    CHECK(xs.data[2] == 2 && xs.data[3] == 3);
}

typedef struct TestMap TestMap;
struct TestMap {
    TestMap *child[4];
    Str8     key;
    i32      value;
};

internal i32 *testmap_upsert(TestMap **m, Str8 key, Arena *a) {
    for (u64 h = str8_hash(key); *m; h <<= 2) {
        if (str8_eq(key, (*m)->key)) return &(*m)->value;
        m = &(*m)->child[h >> 62];
    }
    if (!a) return 0;
    *m = new(a, TestMap);
    (*m)->key = key;
    return &(*m)->value;
}

internal void test_map_basics(Arena scratch) {
    TestMap *map = 0;
    CHECK(!testmap_upsert(&map, S8("a"), 0));
    CHECK(map == 0);

    i32 *a = testmap_upsert(&map, S8("a"), &scratch);
    CHECK(a && *a == 0);
    *a = 10;

    CHECK(testmap_upsert(&map, S8("a"), &scratch) == a);
    CHECK(*testmap_upsert(&map, S8("a"), 0) == 10);

    Str8 a2 = str8_copy(&scratch, S8("a"));
    CHECK(testmap_upsert(&map, a2, 0) == a);

    CHECK(!testmap_upsert(&map, S8("b"), 0));
    CHECK(!testmap_upsert(&map, S8(""), 0));
}

internal void test_map_many(Arena scratch) {
    enum { N = 20000 };
    TestMap *map = 0;
    for (i32 i = 0; i < N; i++) {
        Str8 key = str8_fmt(&scratch, "key%d", i);
        *testmap_upsert(&map, key, &scratch) = i;
    }

    b32 all_ok = 1;
    for (i32 i = 0; i < N; i++) {
        Arena tmp = scratch;
        i32 *v = testmap_upsert(&map, str8_fmt(&tmp, "key%d", i), 0);
        all_ok &= v && *v == i;
    }
    CHECK(all_ok);
    CHECK(!testmap_upsert(&map, S8("key-1"), 0));
}

typedef struct Test Test;
struct Test {
    char *name;
    void (*fn)(Arena scratch);
};

#define TEST(fn) {#fn, fn}

global Test tests[] = {
    TEST(test_mem),
    TEST(test_macros),
    TEST(test_arena_zeroing),
    TEST(test_arena_alignment),
    TEST(test_arena_sequential),
    TEST(test_arena_sub),
    TEST(test_arena_save_restore),
    TEST(test_arena_scratch_by_value),
    TEST(test_arena_out_of_memory),
    TEST(test_str8_basics),
    TEST(test_str8_slice),
    TEST(test_str8_trim),
    TEST(test_str8_cut),
    TEST(test_str8_hash),
    TEST(test_str8_copy),
    TEST(test_str8_concat),
    TEST(test_str8_fmt),
    TEST(test_array_push),
    TEST(test_array_zeroed),
    TEST(test_array_growth_paths),
    TEST(test_array_interleaved),
    TEST(test_array_interleaved),
    TEST(test_array_struct_alignment),
    TEST(test_array_slice_push_copies),
    TEST(test_array_empty_slice_push),
    TEST(test_map_basics),
    TEST(test_map_many),
};

int main() {
    Arena arena = arena_make(MB(64));

    i32 failed_tests = 0;
    for (isize i = 0; i < countof(tests); i++) {
        i32 failures_before = test_failures;
        tests[i].fn(arena);
        b32 passed = test_failures == failures_before;
        failed_tests += !passed;
        printf("%s %s\n", passed ? "  ok  ": "  FAIL", tests[i].name);
    }

    printf("\n%td tests, %d checks, %d failed checks\n", countof(tests), test_checks, test_failures);
    if (!HAVE_DIES) {
        printf("(out-of-memory tests skipped: no fork() on this platform)\n");
    }
    return failed_tests ? 1 : 0;
}