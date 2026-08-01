#!/bin/bash

# Run from the project root, or via Makefile:
#   build/tools/package_linux.sh
#
# This script:
#   - Copies bin/ and res/ into the distribution directory
#   - Recursively bundles shared library dependencies that are NOT system libraries
#   - Skips core system libraries (glibc, X11, GPU drivers, audio backends, etc.)
#
# The binary already has $ORIGIN rpath set, so bundled .so files placed
# next to the executable will be found at runtime.

set -euo pipefail

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

BIN_DIR="$PROJECT_ROOT/bin"
RES_DIR="$PROJECT_ROOT/res"
DIST_DIR="$PROJECT_ROOT/distrib"
DIST_BIN_DIR="$DIST_DIR/linux_client/bin"
DIST_RES_DIR="$DIST_DIR/linux_client/res"

# ---------------------------------------------------------------------------
# System library exclusion list
# ---------------------------------------------------------------------------
# These libraries are either:
#   - Part of glibc (always present on any Linux system)
#   - Kernel virtual DSOs
#   - Display server libraries (X11/Wayland - must match the running server)
#   - GPU/graphics drivers (hardware-specific)
#   - Audio backends (system-specific)
#   - System services (D-Bus, udev, systemd)
#
# Everything NOT in this list gets bundled.
# ---------------------------------------------------------------------------

SYSTEM_LIB_PATTERNS=(
    # Core system (glibc, kernel, dynamic linker)
    'linux-vdso\.so'
    'linux-gate\.so'
    'ld-linux.*\.so'
    'libc\.so'
    'libm\.so'
    'libpthread\.so'
    'libdl\.so'
    'librt\.so'
    'libresolv\.so'
    'libnss_.*\.so'
    'libnsl\.so'
    'libutil\.so'
    'libcrypt\.so'
    'libgcc_s\.so'
    'libstdc\+\+\.so'
    'libmvec\.so'
    'libanl\.so'
    'libBrokenLocale\.so'

    # X11 and Wayland (display server specific, must come from system)
    'libX11\.so'
    'libX11-xcb\.so'
    'libXext\.so'
    'libXrandr\.so'
    'libXcursor\.so'
    'libXi\.so'
    'libXfixes\.so'
    'libXss\.so'
    'libXrender\.so'
    'libXdamage\.so'
    'libXcomposite\.so'
    'libXinerama\.so'
    'libXtst\.so'
    'libxcb.*\.so'
    'libwayland.*\.so'
    'libxkbcommon\.so'
    'libfribidi\.so'
    'libthai\.so'
    'libdatrie\.so'
    'libXau\.so'
    'libXdmcp\.so'

    # GPU/Graphics drivers (hardware specific, must come from system)
    'libGL\.so'
    'libGLX\.so'
    'libGLdispatch\.so'
    'libEGL\.so'
    'libGLESv2\.so'
    'libvulkan\.so'
    'libdrm\.so'
    'libgbm\.so'
    'libglapi\.so'

    # Audio backends (system specific)
    'libasound\.so'
    'libpulse.*\.so'
    'libjack.*\.so'
    'libsndio\.so'
    'libpipewire.*\.so'
    'libspa.*\.so'

    # System services
    'libdbus.*\.so'
    'libudev\.so'
    'libsystemd\.so'
    'libgio.*\.so'
    'libglib.*\.so'
    'libgobject.*\.so'
    'libgmodule.*\.so'
    'libgthread.*\.so'
    'libibus.*\.so'
    'libpcre2.*\.so'
    'libffi\.so'
    'libmount\.so'
    'libblkid\.so'
    'libselinux\.so'
    'libcap\.so'
)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

is_system_lib() {
    local lib_name
    lib_name="$(basename "$1")"
    for pattern in "${SYSTEM_LIB_PATTERNS[@]}"; do
        if [[ "$lib_name" =~ $pattern ]]; then
            return 0
        fi
    done
    return 1
}

# Track processed libraries to avoid infinite recursion
declare -A PROCESSED_LIBS

# Process a single dependency
process_dep() {
    local dep_path="$1"

    # Skip system libraries
    if is_system_lib "$dep_path"; then
        return
    fi

    # Skip if doesn't exist
    if [[ ! -f "$dep_path" ]]; then
        echo "    (warning: $dep_path not found, skipping)" >&2
        return
    fi

    local lib_name dest
    lib_name="$(basename "$dep_path")"
    dest="$DIST_BIN_DIR/$lib_name"

    # Skip if already processed
    if [[ -n "${PROCESSED_LIBS[$lib_name]+x}" ]]; then
        return
    fi
    PROCESSED_LIBS["$lib_name"]=1

    # Skip if already in distribution
    if [[ -f "$dest" ]]; then
        return
    fi

    # Copy library (follow symlinks to get actual file)
    echo "==>   Copying $dep_path -> $dest"
    cp -L "$dep_path" "$dest" || {
        echo "    (warning: failed to copy $dep_path)" >&2
        return
    }

    # Recurse into this library's dependencies
    fix_deps "$dest"
}

# Recursively bundle shared library dependencies
fix_deps() {
    local bin="$1"

    if [[ ! -f "$bin" ]]; then
        echo "    (error: file does not exist: $bin)" >&2
        return
    fi

    echo "==> Fixing deps for $(basename "$bin")"

    local ldd_output
    ldd_output=$(ldd "$bin" 2>&1) || true

    while IFS= read -r line; do
        [[ -z "$line" ]] && continue
        [[ "$line" != *"=>"* ]] && continue

        # Parse: "  libfoo.so.1 => /usr/lib/libfoo.so.1 (0x...)"
        local dep_path
        dep_path=$(echo "$line" | awk '{print $3}')

        [[ -z "$dep_path" ]] && continue
        [[ "$dep_path" == "not" ]] && continue

        process_dep "$dep_path"
    done <<< "$ldd_output"
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

echo "Building Linux distribution package..."
echo "  Project root:  $PROJECT_ROOT"
echo "  Distribution:  $DIST_DIR/linux_client"

# Create distribution directory structure
mkdir -p "$DIST_BIN_DIR" "$DIST_RES_DIR"

# Copy binaries and resources
echo "==> Copying bin/ and res/ into distribution"
cp -r "$BIN_DIR"/* "$DIST_BIN_DIR/" 2>/dev/null || true
cp -r "$RES_DIR"/* "$DIST_RES_DIR/" 2>/dev/null || true

# Copy other distribution files
for f in eula.txt license.txt; do
    if [[ -f "$PROJECT_ROOT/$f" ]]; then
        cp "$PROJECT_ROOT/$f" "$DIST_DIR/linux_client/"
    fi
done

# Process main executable
MOAC_BIN="$DIST_BIN_DIR/moac"
if [[ ! -f "$MOAC_BIN" ]]; then
    echo "ERROR: Expected $MOAC_BIN inside distribution, but it is missing." >&2
    exit 1
fi

echo "==> Fixing shared library dependencies (recursive)..."
fix_deps "$MOAC_BIN"

# Process libastonia_net.so if it exists
ASTONIA_NET_BIN="$DIST_BIN_DIR/libastonia_net.so"
if [[ -f "$ASTONIA_NET_BIN" ]]; then
    fix_deps "$ASTONIA_NET_BIN"
fi

# Process amod.so if it exists
AMOD_BIN="$DIST_BIN_DIR/amod.so"
if [[ -f "$AMOD_BIN" ]]; then
    fix_deps "$AMOD_BIN"
fi

echo "==> Linux distribution package built:"
echo "    $DIST_DIR/linux_client"
