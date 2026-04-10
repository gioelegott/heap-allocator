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

## Documentation
All functions and data structures must be documented using Doxygen-style comments (`/** ... */`). This applies to every new or modified function, struct, and public header.

## Reproducibility
PROMPT.md contains the most important prompts used to generate the repo.

When asked to commit:
1. Append the key prompt(s) for this batch of changes to PROMPT.md under a new numbered heading.
2. Include that same prompt text in the commit description.
3. Run the pre-commit-ci agent (`make test`) before every commit. Block the commit if any tests fail.
4. Run the doc-commit-guardian agent after testing passes. It checks Doxygen completeness, README coherence, and PROMPT.md coverage, and suggests a commit message.
