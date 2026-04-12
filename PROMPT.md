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
