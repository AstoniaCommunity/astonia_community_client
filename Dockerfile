# Dockerfile for cross-compiling Astonia Client to Windows
# Uses Fedora which has excellent MinGW package support
FROM fedora:39

# Install MinGW-w64 cross-compiler toolchain and all dependencies
RUN dnf install -y \
    mingw64-gcc \
    mingw64-gcc-c++ \
    mingw64-binutils \
    mingw64-headers \
    mingw64-SDL2 \
    mingw64-SDL2_mixer \
    mingw64-SDL2_ttf \
    mingw64-libpng \
    mingw64-zlib \
    mingw64-libzip \
    mingw64-freetype \
    make \
    && dnf clean all

# Set up environment for cross-compilation
ENV MINGW_PREFIX=x86_64-w64-mingw32
ENV PKG_CONFIG_PATH=/usr/x86_64-w64-mingw32/sys-root/mingw/lib/pkgconfig

# Note: dwarfstack library is Windows-specific and may not be in repos
# We'll make it optional in the build or provide a stub

# Set working directory for the project
WORKDIR /workspace

# Default command: build the project
CMD ["bash", "-c", "make CC=${MINGW_PREFIX}-gcc WINDRES=${MINGW_PREFIX}-windres && make CC=${MINGW_PREFIX}-gcc WINDRES=${MINGW_PREFIX}-windres amod convert anicopy"]
