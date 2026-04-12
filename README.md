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

The sbrk-backed allocator (`include/sbrk_allocator.h`) is a secondary
implementation and is **not thread-safe**: the program break is a process-wide
resource shared by all threads.  Its free strategy is also more limited —
`sbrk_free()` only lowers the program break when the freed block sits at the
top of the heap; otherwise the memory is abandoned until the process exits.

---

## File layout

```
heap-allocator/
├── include/
│   ├── allocator.h      # Public API: malloc() and free() declarations
│   └── sbrk_allocator.h # Public API: sbrk_malloc() and sbrk_free() declarations
├── src/
│   ├── allocator.c      # malloc() and free() implementation (mmap-backed)
│   ├── sbrk_allocator.c # sbrk_malloc() and sbrk_free() implementation (not thread-safe)
│   └── block.h          # block_header_t definition (internal)
├── tests/
│   ├── test_basic.c     # Functional correctness tests for malloc/free
│   ├── test_thread.c    # Concurrent correctness test (8 threads)
│   └── test_sbrk.c      # Functional correctness tests for sbrk_malloc/sbrk_free
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
| `build/test_sbrk`       | `sbrk_malloc` returns non-NULL; `sbrk_malloc(0)` returns NULL; memory is writable; two allocations return distinct pointers; `sbrk_free(NULL)` is a no-op; a 1 MiB allocation succeeds; LIFO free lowers the program break; FIFO free of a non-top block leaves the break unchanged |

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

All generated code was reviewed, and Doxygen comments were audited and
completed to ensure accuracy against the actual implementation.
