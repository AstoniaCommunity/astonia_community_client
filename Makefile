.PHONY: all debug release windows linux macos macos-appbundle macos-signed-bundle clean distrib distrib-stage amod convert anicopy zig-build docker-linux docker-linux-debug docker-windows docker-windows-debug docker-windows-dev docker-linux-dev docker-distrib-windows docker-distrib-linux appimage zen4-appimage sanitizer coverage test

# Root Makefile - Platform dispatcher
#
# Usage:
#   make                - Build for current platform (auto-detect, release mode)
#   make debug          - Build for current platform in debug mode (enables DEVELOPER)
#   make release        - Build for current platform in release mode (default)
#   make windows        - Build for Windows
#   make linux          - Build for Linux
#   make macos          - Build for macOS
#   make zig-build      - Build with zig for current platform
#   make docker-linux   - Build Linux in Docker (release, Zig build)
#   make docker-linux-debug - Build Linux in Docker (debug with DEVELOPER)
#   make docker-windows - Build Windows in Docker (release)
#   make docker-windows-debug - Build Windows in Docker (debug with DEVELOPER)
#   make linux-appimage - Build Linux AppImage (portable, all distros)
#   make clean          - Clean all platforms
#   make distrib        - Create distribution package
#
# Build types can also be passed to platform targets:
#   make linux BUILD_TYPE=debug
#   make windows BUILD_TYPE=release

# Detect platform (defaults to Windows if unknown)
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    PLATFORM := linux
else ifeq ($(UNAME_S),Darwin)
    PLATFORM := macos
else
    PLATFORM := windows
endif

# ============================================================================
# Configuration Variables
# ============================================================================

# List of all platforms for iteration
ALL_PLATFORMS := windows linux macos

# Docker configuration
DOCKER_CONTAINER_DIR := build/containers
DOCKER_RUN_FLAGS := --rm -e HOST_UID=$(shell id -u) -e HOST_GID=$(shell id -g) -v "$(PWD):/app" -w /app
DOCKER_RUN_FLAGS_INTERACTIVE := --rm -it -e HOST_UID=$(shell id -u) -e HOST_GID=$(shell id -g) -v "$(PWD):/app" -w /app

# ============================================================================
# Reusable Makefile Functions
# ============================================================================

# Function: docker-build-and-run
# Usage: $(call docker-build-and-run,dockerfile-name,image-tag,run-flags,[build-args])
# Args:
#   $1 = Dockerfile name (e.g., Dockerfile.linux)
#   $2 = Docker image tag (e.g., astonia-linux-build)
#   $3 = Docker run flags (e.g., $(DOCKER_RUN_FLAGS))
#   $4 = Optional build arguments (e.g., --build-arg ZEN4=1)
define docker-build-and-run
	@echo "Building Docker image: $(2)..."
	cp $(DOCKER_CONTAINER_DIR)/$(1) .
	docker build -f $(1) -t $(2) $(4) .
	docker run $(3) $(2)
	rm -f $(1)
endef

# Function: docker-build-and-run-appimage
# Usage: $(call docker-build-and-run-appimage,image-tag,output-name,[build-args])
# Args:
#   $1 = Docker image tag (e.g., astonia-appimage-build)
#   $2 = Output AppImage name (e.g., astonia-client.AppImage)
#   $3 = Optional build arguments (e.g., --build-arg ZEN4=1)
define docker-build-and-run-appimage
	@echo "Building Linux AppImage..."
	cp $(DOCKER_CONTAINER_DIR)/Dockerfile.appimage .
	docker build -f Dockerfile.appimage -t $(1) $(3) .
	rm -f Dockerfile.appimage
	docker run --rm -e HOST_UID=$(shell id -u) -e HOST_GID=$(shell id -g) -v "$(PWD):/output" $(1)
	mv astonia-client-*.AppImage $(2)
	@echo ""
	@echo "============================================"
	@echo "AppImage created: $(2)"
	@echo "============================================"
	@echo ""
	@echo "To run: chmod +x $(2) && ./$(2)"
endef

# Default target - build for detected platform (release mode)
all:
	@echo "Building for $(PLATFORM)..."
	@$(MAKE) -f build/make/Makefile.$(PLATFORM)

# Build type targets (pass through to platform makefiles)
debug:
	@echo "Building DEBUG for $(PLATFORM)..."
	@$(MAKE) -f build/make/Makefile.$(PLATFORM) BUILD_TYPE=debug

release:
	@echo "Building RELEASE for $(PLATFORM)..."
	@$(MAKE) -f build/make/Makefile.$(PLATFORM) BUILD_TYPE=release

# Platform-specific targets
windows:
	@echo "Building for Windows..."
	@$(MAKE) -f build/make/Makefile.windows

linux:
	@echo "Building for Linux..."
	@$(MAKE) -f build/make/Makefile.linux

macos:
	@echo "Building for macOS..."
	@$(MAKE) -f build/make/Makefile.macos

macos-appbundle:
	@echo "Building for macOS..."
	@$(MAKE) -f build/make/Makefile.macos appbundle

macos-signed-bundle:
	@echo "Building (locally) signing bundle for macOS..."
	@$(MAKE) -f build/make/Makefile.macos sign

# Clean for all platforms
clean:
	@echo "Cleaning all platforms..."
	@$(foreach platform,$(ALL_PLATFORMS),$(MAKE) -f build/make/Makefile.$(platform) clean 2>/dev/null || true;)
	@echo "Cleaning test binaries..."
	@rm -f bin/test* tests/*.o

# Distribution staging target (delegates to platform-specific Makefile)
distrib-stage:
	@echo "Preparing distribution staging for $(PLATFORM)..."
	@$(MAKE) -f build/make/Makefile.$(PLATFORM) distrib-stage

# Distribution target (delegates to platform-specific Makefile)
distrib:
	@echo "Creating distribution for $(PLATFORM)..."
	@$(MAKE) -f build/make/Makefile.$(PLATFORM) distrib

# Helper targets (delegates to platform-specific Makefile)
amod:
	@$(MAKE) -f build/make/Makefile.$(PLATFORM) amod

convert:
	@$(MAKE) -f build/make/Makefile.$(PLATFORM) convert

anicopy:
	@$(MAKE) -f build/make/Makefile.$(PLATFORM) anicopy

# Code quality targets
sanitizer:
	@echo "Building with AddressSanitizer/UBSan for $(PLATFORM)..."
	@$(MAKE) -f build/make/Makefile.$(PLATFORM) sanitizer

coverage:
	@echo "Building with coverage instrumentation for $(PLATFORM)..."
	@$(MAKE) -f build/make/Makefile.$(PLATFORM) coverage

# Zig build target
zig-build:
	cp build/build.zig .
	zig build --release=fast install --prefix .
	rm -f build.zig

# ============================================================================
# Docker Build Targets
# ============================================================================

# Docker build for Linux (production - release by default, uses Zig)
# For debug: make docker-linux-debug
docker-linux:
	$(call docker-build-and-run,Dockerfile.linux,astonia-linux-build,$(DOCKER_RUN_FLAGS))

# Docker build for Linux (debug mode with DEVELOPER enabled)
docker-linux-debug:
	@echo "Building Linux DEBUG in Docker (with DEVELOPER enabled)..."
	cp $(DOCKER_CONTAINER_DIR)/Dockerfile.linux .
	docker build -f Dockerfile.linux -t astonia-linux-build .
	docker run $(DOCKER_RUN_FLAGS) astonia-linux-build -Ddeveloper=true
	rm -f Dockerfile.linux

# Docker build for Windows (production - release by default)
# For debug: make docker-windows-debug
docker-windows:
	$(call docker-build-and-run,Dockerfile.windows-build,astonia-windows-build,$(DOCKER_RUN_FLAGS))

# Docker build for Windows (debug mode with DEVELOPER enabled)
docker-windows-debug:
	@echo "Building Windows DEBUG in Docker (with DEVELOPER enabled)..."
	cp $(DOCKER_CONTAINER_DIR)/Dockerfile.windows-build .
	docker build -f Dockerfile.windows-build -t astonia-windows-build .
	docker run $(DOCKER_RUN_FLAGS) astonia-windows-build BUILD_TYPE=debug
	rm -f Dockerfile.windows-build

# Docker development environment for Windows (interactive)
docker-windows-dev:
	$(call docker-build-and-run,Dockerfile.windows-dev,astonia-windows-dev,$(DOCKER_RUN_FLAGS_INTERACTIVE))

# Docker development environment for Linux (interactive)
docker-linux-dev:
	$(call docker-build-and-run,Dockerfile.linux-dev,astonia-linux-dev,$(DOCKER_RUN_FLAGS_INTERACTIVE))

# Build Linux AppImage (portable, works on all distributions)
appimage:
	$(call docker-build-and-run-appimage,astonia-appimage-build,astonia-client.AppImage)

# Build Linux AppImage with Zen4 optimizations
zen4-appimage:
	$(call docker-build-and-run-appimage,astonia-appimage-build,astonia-client-zen4.AppImage,--build-arg ZEN4=1)

# Docker distribution builds - create distribution packages
docker-distrib-windows:
	@echo "Building Windows distribution package in Docker..."
	$(call docker-build-and-run,Dockerfile.windows-build,astonia-windows-build,$(DOCKER_RUN_FLAGS))
	@echo "Creating distribution package..."
	@cp $(DOCKER_CONTAINER_DIR)/Dockerfile.windows-build .
	@docker build -f Dockerfile.windows-build -t astonia-windows-build . > /dev/null 2>&1
	@docker run --rm -v "$(PWD):/app" -w /app --entrypoint make astonia-windows-build -f build/make/Makefile.windows distrib
	@rm -f Dockerfile.windows-build
	@echo "Windows distribution created: windows_client.zip"
	@ls -lh windows_client.zip

docker-distrib-linux:
	@echo "Building Linux distribution package in Docker..."
	$(call docker-build-and-run,Dockerfile.linux,astonia-linux-build,$(DOCKER_RUN_FLAGS))
	@echo "Creating distribution package..."
	@cp $(DOCKER_CONTAINER_DIR)/Dockerfile.linux .
	@docker build -f Dockerfile.linux -t astonia-linux-build . > /dev/null 2>&1
	@docker run --rm -v "$(PWD):/app" -w /app --entrypoint make astonia-linux-build -f build/make/Makefile.linux distrib
	@rm -f Dockerfile.linux
	@echo "Linux distribution created: linux_client.tar.gz"
	@ls -lh linux_client.tar.gz

# Include quality checks makefile (see build/make/Makefile.quality)
include build/make/Makefile.quality

# Run unit tests
test:
	@$(MAKE) -C tests run
