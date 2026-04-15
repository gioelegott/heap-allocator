CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -I include
BUILD   = build

# Thread-safety for sbrk allocators.
# Enabled by default; pass THREAD_SAFE=false to compile it out.
THREAD_SAFE ?= true
ifeq ($(THREAD_SAFE),false)
SBRK_CFLAGS  = $(CFLAGS) -DTHREAD_SAFE=0
SBRK_LDFLAGS =
else
SBRK_CFLAGS  = $(CFLAGS) -pthread
SBRK_LDFLAGS = -pthread
endif

SRC      = src/allocator.c
OBJ      = $(BUILD)/allocator.o

SBRK_SRC      = src/sbrk_allocator.c
SBRK_OBJ      = $(BUILD)/sbrk_allocator.o

SBRK_LIST_SRC = src/sbrk_list_allocator.c
SBRK_LIST_OBJ = $(BUILD)/sbrk_list_allocator.o

OPT_SRC = src/opt_allocator.c
OPT_OBJ = $(BUILD)/opt_allocator.o

TESTS   = $(BUILD)/test_basic $(BUILD)/test_thread \
          $(BUILD)/test_sbrk $(BUILD)/test_sbrk_thread \
          $(BUILD)/test_sbrk_list $(BUILD)/test_sbrk_list_thread

# opt allocator tests are built separately; excluded from 'make test'
# until the implementation is complete.
OPT_TESTS = $(BUILD)/test_opt $(BUILD)/test_opt_thread
CLIENT  = $(BUILD)/client
PROFILE = $(BUILD)/profile

.PHONY: all test test_opt profile clean

all: $(CLIENT) $(SBRK_OBJ) $(SBRK_LIST_OBJ) $(OPT_OBJ)

$(BUILD):
	mkdir -p $(BUILD)

$(OBJ): $(SRC) src/block.h include/allocator.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(SBRK_OBJ): $(SBRK_SRC) src/block.h src/sbrk_lock.h include/sbrk_allocator.h | $(BUILD)
	$(CC) $(SBRK_CFLAGS) -c $< -o $@

$(SBRK_LIST_OBJ): $(SBRK_LIST_SRC) src/sbrk_lock.h include/sbrk_list_allocator.h | $(BUILD)
	$(CC) $(SBRK_CFLAGS) -c $< -o $@

$(OPT_OBJ): $(OPT_SRC) src/sbrk_lock.h include/opt_allocator.h | $(BUILD)
	$(CC) $(SBRK_CFLAGS) -c $< -o $@

$(BUILD)/test_basic: tests/test_basic.c $(OBJ) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/test_thread: tests/test_thread.c $(OBJ) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@ -lpthread

$(BUILD)/test_sbrk: tests/test_sbrk.c $(SBRK_OBJ) | $(BUILD)
	$(CC) $(SBRK_CFLAGS) $^ -o $@ $(SBRK_LDFLAGS)

$(BUILD)/test_sbrk_thread: tests/test_sbrk_thread.c $(SBRK_OBJ) | $(BUILD)
	$(CC) $(SBRK_CFLAGS) $^ -o $@ $(SBRK_LDFLAGS)

$(BUILD)/test_sbrk_list: tests/test_sbrk_list.c $(SBRK_LIST_OBJ) | $(BUILD)
	$(CC) $(SBRK_CFLAGS) $^ -o $@ $(SBRK_LDFLAGS)

$(BUILD)/test_sbrk_list_thread: tests/test_sbrk_list_thread.c $(SBRK_LIST_OBJ) | $(BUILD)
	$(CC) $(SBRK_CFLAGS) $^ -o $@ $(SBRK_LDFLAGS)

$(BUILD)/test_opt: tests/test_opt.c $(OPT_OBJ) | $(BUILD)
	$(CC) $(SBRK_CFLAGS) $^ -o $@ $(SBRK_LDFLAGS)

$(BUILD)/test_opt_thread: tests/test_opt_thread.c $(OPT_OBJ) | $(BUILD)
	$(CC) $(SBRK_CFLAGS) $^ -o $@ $(SBRK_LDFLAGS)

$(CLIENT): client/main.c $(OBJ) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(PROFILE): profiling/profile.c $(OBJ) $(SBRK_OBJ) $(SBRK_LIST_OBJ) $(OPT_OBJ) | $(BUILD)
	$(CC) $(SBRK_CFLAGS) $^ -o $@ -pthread $(SBRK_LDFLAGS)

profile: $(PROFILE)
	$(PROFILE)

test: $(TESTS)
	@for t in $(TESTS); do \
		echo "--- $$t ---"; \
		$$t; \
	done

test_opt: $(OPT_TESTS)
	@for t in $(OPT_TESTS); do \
		echo "--- $$t ---"; \
		$$t; \
	done

clean:
	rm -rf $(BUILD)
