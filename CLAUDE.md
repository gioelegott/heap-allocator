# Heap Allocator Project

## Goal
Implement a thread-safe user-space malloc/free library in C for Linux.

## Language & Toolchain
- C (C11 standard)
- GCC, Make
- Linux / Ubuntu target

## Key Requirements
- malloc(size_t) and free(void*) API
- Thread-safe (pthreads)
- Use mmap/sbrk for OS memory acquisition
- Include a Makefile, test suite, and client demo
- Document data structures and allocation policy in README.md
- Document GenAI usage in README.md

## Build command
make

## Test command
make test

## File layout
src/      → allocator implementation
include/  → public headers
tests/    → test cases
client/   → demo client program

## Reproducibility
PROMPT.md contains the most important prompts used to generate the repo.