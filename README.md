# heap-allocator

A minimal, thread-safe user-space `malloc`/`free` library written in C11 for
Linux.  The implementation serves as a baseline: every allocation is backed by
a dedicated `mmap` region rather than a free list, which makes the allocator
trivially safe under concurrent use at the cost of not reusing freed memory.

---

## Project goal

Provide a drop-in replacement for the standard `malloc`/`free` pair that:

- compiles cleanly with `-std=c11 -Wall -Wextra`,
- is safe to call from multiple POSIX threads simultaneously,
- acquires memory from the OS exclusively via `mmap(2)`, and
- releases memory immediately on `free()` via `munmap(2)`.

---

## Data structures

### `block_header_t` (`src/block.h`)

Every allocation consists of one contiguous `mmap` region laid out as follows:

```
low address                          high address
+-----------------------+---------------------------+
|   block_header_t      |   usable bytes            |
|   (size_t size)       |   (returned to caller)    |
+-----------------------+---------------------------+
^                        ^
mmap base                pointer returned by malloc()
```

`block_header_t` contains a single field:

| Field  | Type     | Description                                           |
|--------|----------|-------------------------------------------------------|
| `size` | `size_t` | Number of usable bytes, not counting the header itself |

`malloc()` writes the header immediately before the pointer it returns.
`free()` recovers the header by subtracting `sizeof(block_header_t)` from the
caller's pointer, which gives it the exact byte count needed for `munmap`.

### Free list

This baseline implementation has **no free list**.  Each `malloc()` call
allocates a fresh `mmap` region; each `free()` call immediately unmaps that
region.  Memory is never reused across allocations.

---

## Allocation policy

| Property             | This implementation                               |
|----------------------|---------------------------------------------------|
| Policy               | None (every call is a fresh `mmap`)               |
| OS acquisition       | `mmap(MAP_PRIVATE | MAP_ANONYMOUS)`               |
| OS release           | `munmap` on every `free()`                        |
| Free-list strategy   | None — no reuse of freed regions                  |
| Alignment            | Page-aligned (inherited from `mmap`)              |
| `malloc(0)` behavior | Returns `NULL`                                    |
| `free(NULL)` behavior| No-op                                             |

---

## Thread safety

`malloc()` and `free()` are fully thread-safe **without any mutex**.  Because
each allocation is an independent `mmap` region and each `free()` unmaps only
that region, there is no shared allocator state that threads could race on.
The OS `mmap`/`munmap` syscalls are themselves thread-safe.

This is the key trade-off of the baseline design: thread safety comes for free,
but at the cost of memory reuse and the overhead of a syscall per allocation.

### `sbrk_malloc` / `sbrk_free`

The sbrk-backed allocator (`include/sbrk_allocator.h`) is **thread-safe by
default**: a static `pthread_mutex_t` guards every access to the program
break.  Thread safety can be compiled out by passing `-DTHREAD_SAFE=0`
(or `make THREAD_SAFE=false`) when lower overhead is acceptable at the
cost of safety.  Its free strategy is also limited — `sbrk_free()` only
lowers the program break when the freed block sits at the top of the heap;
otherwise the memory is abandoned until the process exits.

### `opt_malloc` / `opt_free`

The optimised allocator (`include/opt_allocator.h`) is a stub pending
implementation.  It is **thread-safe by default** (same conditional mutex
pattern as the sbrk allocators) and can be compiled without locking via
`-DTHREAD_SAFE=0` / `make THREAD_SAFE=false`.

### `sbrk_list_malloc` / `sbrk_list_free`

The sbrk list allocator (`include/sbrk_list_allocator.h`) improves on the
simple sbrk allocator by maintaining a singly-linked list of all allocated
blocks inside the heap itself.  Each node stores the usable size and a free
flag.  `sbrk_list_malloc()` performs a first-fit scan and reuses any
sufficiently large free block without touching the program break.  If no
suitable block is found the heap is extended via `sbrk(2)`.
`sbrk_list_free()` marks the block free.  If the freed block is at the tail
of the list it is unlinked and the program break is lowered via `sbrk(2)`;
this reclaim cascades until a live block is reached or the list is empty.
Non-tail blocks remain in the list for first-fit reuse.  Like the simple
sbrk allocator, this allocator is **thread-safe by default** (mutex on
every access to the free list and program break) and can be compiled
without locking via `-DTHREAD_SAFE=0` / `make THREAD_SAFE=false`.

---

## File layout

```
heap-allocator/
├── include/
│   ├── allocator.h           # Public API: malloc() and free() declarations
│   ├── sbrk_allocator.h      # Public API: sbrk_malloc() and sbrk_free() declarations
│   ├── sbrk_list_allocator.h # Public API: sbrk_list_malloc() and sbrk_list_free() declarations
│   └── opt_allocator.h       # Public API: opt_malloc() and opt_free() declarations
├── src/
│   ├── allocator.c           # malloc() and free() implementation (mmap-backed)
│   ├── sbrk_allocator.c      # sbrk_malloc() and sbrk_free() (simple, thread-safe by default)
│   ├── sbrk_list_allocator.c # sbrk_list_malloc() and sbrk_list_free() (free list, thread-safe by default)
│   ├── opt_allocator.c       # opt_malloc() and opt_free() (stub — implementation pending)
│   ├── block.h               # block_header_t definition (internal)
│   └── sbrk_lock.h           # Conditional pthread mutex macros for sbrk allocators (internal)
├── tests/
│   ├── test_basic.c          # Functional correctness tests for malloc/free
│   ├── test_thread.c         # Concurrent correctness test (8 threads)
│   ├── test_sbrk.c               # Functional correctness tests for sbrk_malloc/sbrk_free
│   ├── test_sbrk_thread.c        # Concurrent correctness test for sbrk_malloc/sbrk_free (8 threads)
│   ├── test_sbrk_list.c          # Functional correctness tests for sbrk_list_malloc/sbrk_list_free
│   ├── test_sbrk_list_thread.c   # Concurrent correctness test for sbrk_list_malloc/sbrk_list_free (8 threads)
│   ├── test_opt.c                # Functional correctness tests for opt_malloc/opt_free
│   └── test_opt_thread.c         # Concurrent correctness test for opt_malloc/opt_free (8 threads)
├── client/
│   └── main.c           # Minimal demo program
├── profiling/
│   ├── profile.c        # Allocation/deallocation benchmark scenarios
│   └── timer.h          # Lightweight monotonic timer (prof_timer_t)
├── Makefile
├── PROMPT.md            # GenAI prompts used during development
└── README.md
```

---

## Build instructions

```sh
# Build the client demo (also compiles the allocator object)
make

# Run the full test suite
make test

# Build and run the profiler
make profile

# Build and run the opt allocator tests (stub — expected to fail until implemented)
make test_opt

# Remove all build artefacts
make clean
```

Build output is placed in the `build/` directory.  The Makefile requires GCC
and is tested on Ubuntu/Linux (kernel 5.x or later).

---

## Test suite

| Binary                  | What it tests                                                  |
|-------------------------|----------------------------------------------------------------|
| `build/test_basic`      | `malloc` returns non-NULL; `malloc(0)` returns NULL; memory is writable; two allocations return distinct pointers; `free(NULL)` is a no-op; a 1 MiB allocation succeeds |
| `build/test_thread`     | 8 threads each perform 64 `malloc`/write/verify/`free` cycles concurrently without data corruption |
| `build/test_sbrk`            | `sbrk_malloc` returns non-NULL; `sbrk_malloc(0)` returns NULL; memory is writable; two allocations return distinct pointers; `sbrk_free(NULL)` is a no-op; a 1 MiB allocation succeeds; LIFO free lowers the program break; FIFO free of a non-top block leaves the break unchanged |
| `build/test_sbrk_thread`     | 8 threads each perform 64 `sbrk_malloc`/write/verify/`sbrk_free` cycles concurrently without data corruption |
| `build/test_sbrk_list`       | `sbrk_list_malloc` returns non-NULL; `sbrk_list_malloc(0)` returns NULL; memory is writable; two live allocations return distinct pointers; `sbrk_list_free(NULL)` is a no-op; a 1 MiB allocation succeeds; a freed block is reused by the next same-size allocation; a non-top freed block is reused (FIFO); a larger freed block satisfies a smaller request (first-fit); freeing the tail block lowers the program break; cascade reclaim of consecutive free tail blocks fully restores the break |
| `build/test_sbrk_list_thread`| 8 threads each perform 64 `sbrk_list_malloc`/write/verify/`sbrk_list_free` cycles concurrently without data corruption; exercises the shared free list under concurrent access |
| `build/test_opt`             | `opt_malloc` returns non-NULL; `opt_malloc(0)` returns NULL; memory is writable; two live allocations return distinct pointers; `opt_free(NULL)` is a no-op; a 1 MiB allocation succeeds; a freed block is reused by the next same-size allocation **(stub: expected to fail until implementation is complete)** |
| `build/test_opt_thread`      | 8 threads each perform 64 `opt_malloc`/write/verify/`opt_free` cycles concurrently without data corruption **(stub: expected to fail until implementation is complete)** |

---

## GenAI usage

This project was developed with the assistance of an AI coding assistant
(Claude, Anthropic).  The exact prompts used are recorded in
[PROMPT.md](PROMPT.md).

Prompts covered the following areas:

1. Generating the initial directory and file structure.
2. Implementing the baseline `malloc`/`free` backed by `mmap`.
3. Writing the `Makefile` and configuring the `build/` output directory.
4. Generating the test suite (`test_basic.c`, `test_thread.c`).
5. Creating the profiling infrastructure with symmetric interleaved, FIFO, and LIFO scenarios across single- and multi-threaded workloads.
6. Implementing an alternative `sbrk`-backed allocator (`sbrk_malloc`/`sbrk_free`) as a non-thread-safe counterpart to the baseline.
7. Adding the `test_sbrk.c` test suite and extending `profiling/profile.c` to benchmark both allocators via function pointers.
8. Implementing `sbrk_list_malloc`/`sbrk_list_free`: a smarter sbrk-backed allocator with a singly-linked free list and first-fit block reuse.
9. Adding the `test_sbrk_list.c` test suite and extending `profiling/profile.c` to benchmark the sbrk list allocator.
10. Updating `sbrk_list_free` to lower the program break on tail reclaim with cascade until the list is empty or a live block is found; extending the test suite with two new tail-reclaim tests.
11. Adding conditional mutex support (`src/sbrk_lock.h`) to both sbrk allocators; thread safety is enabled by default and can be disabled with `THREAD_SAFE=false`.
12. Adding multi-threaded test suites (`test_sbrk_thread.c`, `test_sbrk_list_thread.c`) and multi-threaded profiling sections to `profiling/profile.c` for both sbrk allocators; generalising `worker_args_t` and `RUN_MT_ALL_ORDERS` to accept allocator function pointers.
13. Generating a template for `opt_malloc`/`opt_free` with stub implementation, full test suites (`test_opt.c`, `test_opt_thread.c`), ST and MT profiling sections, and Makefile integration; `make test_opt` is a separate target excluded from `make test` until the implementation is complete.

All generated code was reviewed, and Doxygen comments were audited and
completed to ensure accuracy against the actual implementation.
