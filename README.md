# heap-allocator

This project implements a thread-safe user-space `malloc`/`free` library written in C11 for
Linux.  Several implementations are proposed and evaluated.

---

## Implementations

### `malloc/free` (`src/allocator.c`)

This is the simplest implementation, where `malloc` and `free` map directly to the `mmap` and
`munmap` system calls.  Every allocation consists of one contiguous `mmap` region laid out as
follows:

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

This implementation has **no free list**.  Each `malloc()` call allocates a fresh `mmap` region;
each `free()` call immediately unmaps that region.  Memory is never reused across allocations.

---

### `sbrk_malloc/sbrk_free` (`src/sbrk_allocator.c`)

This is a very simple implementation that uses the program break to allocate new memory.  It
uses memory very inefficiently: if a block is freed and is not at the top of the heap, it is
abandoned and never reused.  While this makes it unsuitable for programs that use the heap
intensively, it serves as an upper bound on performance for sbrk-based allocators.  Its simple
logic means it outperforms the other sbrk allocators in every time-based benchmark.

Like the mmap-based allocator, `sbrk_malloc` and `sbrk_free` use `block_header_t` (defined in
`src/block.h`) to store the usable size of each allocation.

---

### `sbrk_list_malloc/sbrk_list_free` (`src/sbrk_list_allocator.c`)

Given the poor memory efficiency of the simple sbrk allocator, this implementation keeps track
of allocations with a singly-linked list.  Each node (`block_node_t`) stores a pointer to the
next node, the usable size, and a free flag.  `sbrk_list_malloc()` performs a first-fit scan
and reuses any sufficiently large free block without touching the program break.  If no suitable
block is found, the heap is extended via `sbrk(2)`.  `sbrk_list_free()` marks the block free.
If the freed block is at the tail of the list, it is unlinked and the program break is lowered
via `sbrk(2)`; this reclaim cascades until a live block is reached or the list is empty.

As expected, this implementation is much slower than the naive sbrk allocator: in the worst
case, `malloc` needs O(N) operations to find a free block (where N is the number of allocated
blocks), and `free` needs O(N²) operations to fully reset the program break via cascade reclaim.
On the other hand, this implementation is much more memory-efficient, and it always returns zero
residual memory (if all memory is freed, the program break is fully restored to its initial
value).

---

### `opt_malloc/opt_free` (`src/opt_allocator.c`)

This allocator is an optimisation of the sbrk_list allocator.  It uses an implicit free list,
first-fit search, on-the-fly forward coalescing of adjacent free blocks, and block splitting.
The free list and first-fit search are identical to the sbrk_list allocator, except that the
pointer to the next block is implicit: it is computed from the current block's address and size,
since all blocks are laid out contiguously in memory.

Two additional features improve memory reuse:

- **Block coalescing:** during the first-fit scan, `opt_malloc` merges adjacent free blocks
  on-the-fly to reduce fragmentation and increase the chance of satisfying larger requests.
- **Block splitting:** when a free block is larger than needed, only the required portion is
  used; the remainder is returned to the free list as a new, smaller free block.

`opt_free` uses the same tail-reclaim strategy as `sbrk_free`: if the freed block is at the
top of the heap, the program break is lowered immediately.  Coalescing of non-tail free blocks
is deferred lazily to the next `opt_malloc` scan.

---

## Thread safety

`malloc()` and `free()` are fully thread-safe **without any mutex**.  Because each allocation
is an independent `mmap` region and each `free()` unmaps only that region, there is no shared
allocator state that threads could race on.  The OS `mmap`/`munmap` syscalls are themselves
thread-safe.

All other implementations use a `pthread_mutex` for thread safety, which can be disabled with
`-DTHREAD_SAFE=0` / `make THREAD_SAFE=false`.  Each function is almost entirely wrapped in the
mutex to prevent data races.  More fine-grained approaches may be possible but are considerably
more complex given the shared data structures of the list-based designs.  This is a notable
limitation of sbrk-based implementations, suggesting that mmap-based allocators may scale
better for multi-threaded applications.

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
│   ├── opt_allocator.c       # opt_malloc() and opt_free() (implicit free-list, first-fit, thread-safe by default)
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
│   ├── profile_mem.c    # Memory-usage scenarios (sbrk-based allocators, single-threaded)
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

# Build and run the timing profiler
make profile

# Build and run the memory-usage profiler (sbrk-based allocators only)
make profile_mem

# Build and run the opt allocator tests (also run by make test)
make test_opt

# Remove all build artefacts
make clean
```

Build output is placed in the `build/` directory.

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
| `build/test_opt`             | `opt_malloc` returns non-NULL; `opt_malloc(0)` returns NULL; memory is writable; two live allocations return distinct pointers; `opt_free(NULL)` is a no-op; a 1 MiB allocation succeeds; a freed block is reused without heap growth (`sbrk(0)` does not advance after re-allocation) |
| `build/test_opt_thread`      | 8 threads each perform 64 `opt_malloc`/write/verify/`opt_free` cycles concurrently without data corruption |

---

## GenAI usage

This project was developed with the assistance of an AI coding assistant
(Claude, Anthropic).  The exact prompts used are recorded in
[PROMPT.md](PROMPT.md) (only the main prompts are recorded for clarity).

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
13. Generating a template for `opt_malloc`/`opt_free` with stub implementation, full test suites (`test_opt.c`, `test_opt_thread.c`), ST and MT profiling sections, and Makefile integration.
14. Implementing `opt_malloc`/`opt_free` as an implicit free-list allocator with first-fit search, on-the-fly forward coalescing of adjacent free blocks, block splitting, and alignment rounding; `make test_opt` is now also run by `make test`.

All generated code was reviewed, and Doxygen comments were audited and
completed to ensure accuracy against the actual implementation.
