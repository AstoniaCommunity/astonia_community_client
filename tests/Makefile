# Test Makefile for Astonia Client
# Compiles and runs unit tests without external test frameworks

CC = clang
CFLAGS = -O2 -g -Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-unused-function
CFLAGS += -DUNIT_TEST -DDEVELOPER
CFLAGS += -I../src -I../include
CFLAGS += $(shell pkg-config --cflags sdl2)

LDFLAGS = $(shell pkg-config --libs sdl2)
LDFLAGS += -lz -lpng -lzip -lm -lmimalloc

# SDL source files needed for testing
# Note: We use REAL I/O (sdl_image.c included) and only stub GPU operations
SDL_SRCS = ../src/sdl/sdl_test.c \
           ../src/sdl/sdl_core.c \
           ../src/sdl/sdl_texture.c \
           ../src/sdl/sdl_image.c \
           ../src/sdl/sdl_effects.c \
           ../src/sdl/sdl_draw.c

# Helper source files
HELPER_SRCS = ../src/game/memory.c \
              ../src/helper/helper.c \
              test_stubs.c

# Platform-specific memory implementation
ifeq ($(shell uname),Linux)
    HELPER_SRCS += ../src/game/memory_linux.c
else ifeq ($(shell uname),Darwin)
    HELPER_SRCS += ../src/game/memory_macos.c
else
    HELPER_SRCS += ../src/game/memory_windows.c
endif

ALL_SRCS = $(SDL_SRCS) $(HELPER_SRCS)

# Output directory
BIN_DIR = ../bin

# Test executables
TEST_SERIALIZED = $(BIN_DIR)/test_serialized
TEST_CONCURRENT = $(BIN_DIR)/test_concurrent

all: $(TEST_SERIALIZED) $(TEST_CONCURRENT)

$(TEST_SERIALIZED): test_texture_cache.c $(ALL_SRCS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_CONCURRENT): test_texture_workers.c $(ALL_SRCS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Run serialized tests (single-threaded cache tests)
test_serialized: $(TEST_SERIALIZED)
	@echo ""
	@echo "==============================================="
	@echo "Running serialized tests (single-threaded)..."
	@echo "==============================================="
	cd .. && ./bin/test_serialized

# Run concurrent tests (multi-threaded worker tests)
test_concurrent: $(TEST_CONCURRENT)
	@echo ""
	@echo "==============================================="
	@echo "Running concurrent tests (multi-threaded)..."
	@echo "==============================================="
	cd .. && ./bin/test_concurrent

# Run all tests in sequence
run: test_serialized test_concurrent
	@echo ""
	@echo "==============================================="
	@echo "All tests passed!"
	@echo "==============================================="

clean:
	rm -f $(TEST_SERIALIZED) $(TEST_CONCURRENT) *.o

.PHONY: all clean run test_serialized test_concurrent
