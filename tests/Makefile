# Test Makefile for Astonia Client
# Compiles and runs unit tests without external test frameworks

# Detect platform
UNAME_S := $(shell uname -s 2>/dev/null)
ifeq ($(UNAME_S),Linux)
    PLATFORM := linux
else ifeq ($(UNAME_S),Darwin)
    PLATFORM := macos
else
    PLATFORM := windows
endif

CC = clang
CFLAGS = -O2 -g -Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-unused-function
CFLAGS += -DUNIT_TEST -DDEVELOPER
CFLAGS += -I../src -I../include
CFLAGS += $(shell pkg-config --cflags sdl3)

# Platform-specific SDL linking
ifeq ($(PLATFORM),windows)
    LDFLAGS = -lmingw32 -lSDL3 -lSDL3_mixer -lz -lpng -lzip -lmimalloc
else
    LDFLAGS = $(shell pkg-config --libs sdl3)
    LDFLAGS += -lSDL3_mixer -lz -lpng -lzip -lm -lmimalloc
endif

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
ifeq ($(PLATFORM),linux)
    HELPER_SRCS += ../src/game/memory_linux.c
else ifeq ($(PLATFORM),macos)
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
TEST_HASH_DIAG = $(BIN_DIR)/test_hash_distribution
TEST_RENDER_PRIMS = $(BIN_DIR)/test_render_primitives

all: $(TEST_SERIALIZED) $(TEST_CONCURRENT) $(TEST_HASH_DIAG) $(TEST_RENDER_PRIMS)
test: run

$(TEST_SERIALIZED): test_texture_cache.c $(ALL_SRCS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_CONCURRENT): test_texture_workers.c $(ALL_SRCS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_HASH_DIAG): test_hash_distribution.c $(ALL_SRCS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_RENDER_PRIMS): test_render_primitives.c $(ALL_SRCS)
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

# Run hash distribution diagnostic (separate from main test suite)
test_hash_diag: $(TEST_HASH_DIAG)
	@echo ""
	@echo "==============================================="
	@echo "Running hash distribution diagnostics..."
	@echo "==============================================="
	cd .. && ./bin/test_hash_distribution

# Run render primitives tests
test_render_prims: $(TEST_RENDER_PRIMS)
	@echo ""
	@echo "==============================================="
	@echo "Running render primitives tests..."
	@echo "==============================================="
	cd .. && ./bin/test_render_primitives

# Run all tests in sequence
run: test_serialized test_concurrent test_hash_diag test_render_prims
	@echo ""
	@echo "==============================================="
	@echo "All tests passed!"
	@echo "==============================================="

clean:
	rm -f $(TEST_SERIALIZED) $(TEST_CONCURRENT) $(TEST_HASH_DIAG) $(TEST_RENDER_PRIMS) *.o

.PHONY: all clean run test_serialized test_concurrent test_render_prims
