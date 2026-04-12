CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -I include
BUILD   = build

SRC      = src/allocator.c
OBJ      = $(BUILD)/allocator.o

SBRK_SRC = src/sbrk_allocator.c
SBRK_OBJ = $(BUILD)/sbrk_allocator.o

TESTS   = $(BUILD)/test_basic $(BUILD)/test_thread $(BUILD)/test_sbrk
CLIENT  = $(BUILD)/client
PROFILE = $(BUILD)/profile

.PHONY: all test profile clean

all: $(CLIENT) $(SBRK_OBJ)

$(BUILD):
	mkdir -p $(BUILD)

$(OBJ): $(SRC) src/block.h include/allocator.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(SBRK_OBJ): $(SBRK_SRC) src/block.h include/sbrk_allocator.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/test_basic: tests/test_basic.c $(OBJ) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/test_thread: tests/test_thread.c $(OBJ) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@ -lpthread

$(BUILD)/test_sbrk: tests/test_sbrk.c $(SBRK_OBJ) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(CLIENT): client/main.c $(OBJ) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(PROFILE): profiling/profile.c $(OBJ) $(SBRK_OBJ) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@ -lpthread

profile: $(PROFILE)
	$(PROFILE)

test: $(TESTS)
	@for t in $(TESTS); do \
		echo "--- $$t ---"; \
		$$t; \
	done

clean:
	rm -rf $(BUILD)
