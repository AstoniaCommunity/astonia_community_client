#!/bin/bash
# Docker cross-compilation build script for Astonia Client

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE_NAME="astonia-windows-builder"

echo "=========================================="
echo "Astonia Client - Windows Cross-Compile"
echo "=========================================="
echo ""

# Build Docker image
echo "[1/3] Building Docker image..."
docker build -t "${IMAGE_NAME}" "${SCRIPT_DIR}"

echo ""
echo "[2/3] Compiling project for Windows..."
docker run --rm \
    -v "${SCRIPT_DIR}:/workspace" \
    -u "$(id -u):$(id -g)" \
    "${IMAGE_NAME}"

echo ""
echo "[3/3] Build complete!"
echo ""

# Check if executables were created
if [ -f "${SCRIPT_DIR}/bin/moac.exe" ]; then
    echo "✓ Successfully built:"
    ls -lh "${SCRIPT_DIR}/bin/"*.exe "${SCRIPT_DIR}/bin/"*.dll 2>/dev/null || true
    echo ""
    echo "Windows executables are in: ${SCRIPT_DIR}/bin/"
else
    echo "✗ Build failed - moac.exe not found"
    exit 1
fi

echo ""
echo "To package for distribution, run:"
echo "  docker run --rm -v \"\$(pwd):/workspace\" -u \"\$(id -u):\$(id -g)\" ${IMAGE_NAME} make distrib"
