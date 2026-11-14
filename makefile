# Usage: make            build the example (debug, with sanitizers)
#        make run        build and run the example
#        make release    build the example (optimized)
#        make test       build and run the tests (with sanitizers)
#        make clean      delete the build folder
#        make CC=clang   use a different compiler with any of the above

CC     ?= cc
CFLAGS  = -std=c11 -Wall -Wextra -Wno-unused-function -I.
DEBUG   = -O0 -g -fsanitize=address,undefined
RELEASE = -O2

export ASAN_OPTIONS = detect_leaks=0

.PHONY: all run release test clean

all: build/main

run: build/main
	./build/main

release: build/main_release

test: build/test_base
	./build/test_base

build/main: examples/main.c base.h | build
	$(CC) $(CFLAGS) $(DEBUG) $< -o $@

build/main_release: examples/main.c base.h | build
	$(CC) $(CFLAGS) $(RELEASE) $< -o $@

build/test_base: tests/test_base.c base.h | build
	$(CC) $(CFLAGS) $(DEBUG) $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build