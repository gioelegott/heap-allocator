## Prompts used to generate this repository

### 1. Directory structure
Generate the directory structure including subdirectories and files. Don't generate the file contents yet.

### 2. Baseline allocator
Generate the allocator code for malloc and free mapping them to mmap. Make the code as simple as possible. This first implementation is going to be used as a baseline.

### 3. Makefile and build directory
Generate the Makefile and a build directory where the executables are stored.

### 4. Tests
Generate the tests for the malloc and free functions.

### 5. README and documentation
Use the doc-commit-guardian agent to generate the README and update the CLAUDE.md so that this agent is used before a commit but after testing.

### 6. Profiling infrastructure
Create a new folder containing the profiling infrastructure for malloc and free. The profiling should test different allocation/deallocation scenarios both single- and multi-threaded, with symmetric experiments across interleaved, batch FIFO, and batch LIFO free orders.

### 7. sbrk allocator
Generate a new allocator/deallocator using brk/sbrk. Use the same programming style as the existing malloc/free. The new allocator doesn't need to be thread-safe so ignore any type of locking mechanism.

### 8. Tests and profiling for sbrk allocator
Add tests and profiling for the sbrk allocator.

### 9. sbrk list allocator
Create a new smarter sbrk_allocator that reclaims blocks even if they are not at the top of the heap. Do so with a list that keeps track of where each block is, their size, and if it has been freed or not. Again disregard any thread-safety mechanism for now.

### 10. Tests and profiling for sbrk list allocator
Add testing and profiling for the smarter sbrk allocator, similarly to how the base sbrk allocator is tested/profiled.

### 11. Tail reclaim in sbrk list allocator
Update the sbrk_list_allocator so that when the last block in the list is freed, it is removed from the list and the program break is lowered.

### 12. Thread safety for sbrk allocators
Add a locking mechanism to both sbrk allocators for thread safety. You should add a mutex whenever global structures are accessed. The thread safety mechanism should be designed such that it can be disabled with a define THREAD_SAFE=false which is by default true.

### 13. Multi-threaded tests and profiling for sbrk allocators
Add the testing and profiling infrastructure for multi-threaded sbrk_allocator and sbrk_list_allocator. Use the infrastructure for testing and profiling the baseline allocator as a template.

### 14. Optimal allocator template
Generate a template for an optimal allocator and all the surrounding infrastructure, including testing and profiling. Leave the actual implementation empty.

### 15. Optimal allocator implementation
Implement opt_malloc and opt_free as an implicit free-list allocator with first-fit search, on-the-fly forward coalescing of adjacent free blocks, block splitting, alignment rounding via _Alignof, and sbrk-backed heap extension. Add comments and format correctly.

### 16. Memory-usage profiler
Add profiling infrastructure for memory usage by querying sbrk. Test only sbrk-based allocators (sbrk_malloc, sbrk_list_malloc, opt_malloc) and only single-threaded. Put the memory profiler in a separate file.

### 17. Memory-usage profiler scenarios
Focus on one fixed-size case and one mixed-size case. For each test interleaved, FIFO, LIFO, cyclic FIFO, and cyclic LIFO free orders. Example of cyclic FIFO: a=malloc, b=malloc, c=malloc, free(a), free(b), free(c), a=malloc, b=malloc, ...

### 18. Isolated memory-usage runs
Make sure that all profiler runs are independent. Memory is expected to be clean at the start as well as sbrk to be reset.

### 19. opt_free tail reclaim
Make it so that opt_free modifies sbrk similarly to the sbrk_allocator: lower the program break when the freed block is at the top of the heap. Fix the split tail-trim bug (hdr->size not updated). Update the reuse test to use sbrk(0)-based assertion instead of pointer equality.
