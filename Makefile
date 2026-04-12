CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -I include
BUILD   = build

SRC     = src/allocator.c
OBJ     = $(BUILD)/allocator.o

TESTS   = $(BUILD)/test_basic $(BUILD)/test_thread
CLIENT  = $(BUILD)/client
PROFILE = $(BUILD)/profile

.PHONY: all test profile clean

all: $(CLIENT)

$(BUILD):
	mkdir -p $(BUILD)

$(OBJ): $(SRC) src/block.h include/allocator.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/test_basic: tests/test_basic.c $(OBJ) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/test_thread: tests/test_thread.c $(OBJ) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@ -lpthread

$(CLIENT): client/main.c $(OBJ) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(PROFILE): profiling/profile.c $(OBJ) | $(BUILD)
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
