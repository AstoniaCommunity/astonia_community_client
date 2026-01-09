#!/bin/bash
set -e

# Auto-format C and Rust code
# Used by both CI pipeline and local development (make lint)
# Applies formatting fixes automatically

FIND="/usr/bin/find"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

echo "========================================"
echo "  Auto-Formatting Code"
echo "========================================"
echo ""

# ============================================================================
# C Code Formatting (clang-format)
# ============================================================================
echo ">>> Formatting C Code"

# Detect clang-format
CLANG_FORMAT=$(which clang-format-21 2>/dev/null || which clang-format 2>/dev/null || echo "")
if [ -z "$CLANG_FORMAT" ]; then
    echo "ERROR: clang-format not found"
    echo "Install with:"
    echo "  Ubuntu/Debian: sudo apt-get install clang-format"
    echo "  Arch: sudo pacman -S clang"
    echo "  macOS: brew install clang-format"
    exit 1
fi

CLANG_FORMAT_VERSION=$($CLANG_FORMAT --version 2>/dev/null | grep -oP 'version \K[0-9]+' || echo "0")
if [ "$CLANG_FORMAT_VERSION" != "21" ]; then
    echo "WARNING: Using clang-format version $CLANG_FORMAT_VERSION"
    echo "         CI pipeline uses version 21 - formatting may differ slightly"
fi

# Format all C/H files
$FIND src -type f \( -name "*.c" -o -name "*.h" \) -exec $CLANG_FORMAT -i --style=file {} \;
echo "  ✓ C code formatted"
echo ""

# ============================================================================
# Rust Code Formatting (rustfmt)
# ============================================================================
echo ">>> Formatting Rust Code"

if [ ! -d "astonia_net" ]; then
    echo "  - Rust directory not found, skipping"
    echo ""
elif ! command -v cargo >/dev/null 2>&1; then
    echo "WARNING: cargo not installed, skipping Rust formatting"
    echo ""
else
    cd astonia_net && cargo fmt -- --emit files
    echo "  ✓ Rust code formatted"
    cd "$PROJECT_ROOT"
    echo ""
fi

# ============================================================================
# Summary
# ============================================================================
echo "========================================"
echo "  ✓ Formatting Complete"
echo "========================================"
echo ""
echo "Run 'scripts/check-format.sh' to verify formatting"
