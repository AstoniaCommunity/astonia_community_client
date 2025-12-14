#!/bin/bash

# Run from the project root, or via Makefile:
#   build/tools/package_windows.sh
#
# This script:
#   - Copies bin/ and res/ into the distribution directory
#   - Recursively bundles DLL dependencies that are NOT system libraries
#   - Skips DLLs from Windows system directories (e.g., /c/Windows/System32)
#
# Environment support:
#   - MSYS2/Cygwin: Uses `ldd` (always available in these environments)
#   - Docker/Linux: Uses `objdump` for cross-compilation (SYSROOT must be set)

set -euo pipefail

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

BIN_DIR="$PROJECT_ROOT/bin"
RES_DIR="$PROJECT_ROOT/res"
DIST_DIR="$PROJECT_ROOT/distrib"
DIST_BIN_DIR="$DIST_DIR/windows_client/bin"
DIST_RES_DIR="$DIST_DIR/windows_client/res"

# Track processed DLLs to avoid infinite recursion
DLL_PROCESSED_FILE="$PROJECT_ROOT/.dll_processed_$$.txt"

# Cleanup trap to ensure temp file is removed
cleanup() {
  rm -f "$DLL_PROCESSED_FILE"
}
trap cleanup EXIT

# ---------------------------------------------------------------------------
# Environment Detection
# ---------------------------------------------------------------------------

# Detect if we're in MSYS2/Cygwin environment
is_msys2_or_cygwin() {
  [[ -n "${MSYSTEM:-}" ]] || \
  [[ -n "${MSYSTEM_PREFIX:-}" ]] || \
  [[ "$(uname -o 2>/dev/null)" == "Cygwin" ]]
}

# Detect if we're in WSL
is_wsl() {
  grep -qEi "(Microsoft|WSL)" /proc/version 2>/dev/null || false
}

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

is_system_lib() {
  local path="$1"
  # Normalize path separators (handle both / and \)
  local normalized_path="${path//\\//}"
  
  # Check for Windows system directories
  case "$normalized_path" in
    /c/Windows/System32/*|/c/Windows/SysWOW64/*|/c/Windows/*)
      return 0 ;;
    C:/Windows/System32/*|C:/Windows/SysWOW64/*|C:/Windows/*)
      return 0 ;;
    c:/Windows/System32/*|c:/Windows/SysWOW64/*|c:/Windows/*)
      return 0 ;;
    *)
      return 1 ;;
  esac
}

# Convert MSYS2 path to WSL path if needed
# /c/... -> /mnt/c/...
# /clang64/bin/... -> /mnt/c/msys64/clang64/bin/...
msys2_to_wsl_path() {
  local path="$1"
  
  if [[ "$path" == /c/* ]]; then
    echo "/mnt${path}"
  elif [[ "$path" == /clang64/* ]]; then
    echo "/mnt/c/msys64${path}"
  elif [[ "$path" == /mingw64/* ]]; then
    echo "/mnt/c/msys64${path}"
  else
    echo "$path"
  fi
}

# Find MSYS2 ldd when running from WSL
# Only needed when we're in WSL but need to access MSYS2's ldd
find_msys2_ldd_in_wsl() {
  if ! is_wsl; then
    return 1
  fi
  
  # Try common MSYS2 locations (WSL paths)
  for ldd_path in \
    "/mnt/c/msys64/usr/bin/ldd.exe" \
    "/mnt/c/msys64/usr/bin/ldd" \
    "/mnt/c/msys64/clang64/bin/ldd.exe" \
    "/mnt/c/msys64/clang64/bin/ldd"
  do
    if [[ -x "$ldd_path" ]] 2>/dev/null || [[ -f "$ldd_path" ]]; then
      echo "$ldd_path"
      return 0
    fi
  done
  
  return 1
}

# Process a single dependency DLL
process_dep() {
  local dep_path="$1"
  
  # Skip system libraries
  if is_system_lib "$dep_path"; then
    return
  fi
  
  # Convert MSYS2 paths to WSL paths if needed (when running from WSL)
  # ldd outputs MSYS2 paths like /clang64/bin/... but in WSL we need /mnt/c/msys64/clang64/bin/...
  if is_wsl && ([[ "$dep_path" == /c/* ]] || [[ "$dep_path" == /clang64/* ]] || [[ "$dep_path" == /mingw64/* ]]); then
    local wsl_path
    wsl_path=$(msys2_to_wsl_path "$dep_path")
    if [[ "$wsl_path" != "$dep_path" ]] && [[ -f "$wsl_path" ]]; then
      dep_path="$wsl_path"
    fi
  fi
  
  # Skip if DLL doesn't exist
  if [[ ! -f "$dep_path" ]]; then
    echo "    (warning: $dep_path not found, skipping)" >&2
    return
  fi
  
  local dll_name dest
  dll_name="$(basename "$dep_path")"
  dest="$DIST_BIN_DIR/$dll_name"
  
  # Skip if DLL is already in distribution (avoid infinite recursion and duplicate work)
  if [[ -f "$dest" ]]; then
    return
  fi
  
  # Skip if already processed
  if grep -Fxq "$dll_name" "$DLL_PROCESSED_FILE" 2>/dev/null; then
    return
  fi
  
  # Mark as processed
  echo "$dll_name" >> "$DLL_PROCESSED_FILE"
  
  # Copy DLL
  echo "==>   Copying $dep_path -> $dest"
  cp "$dep_path" "$dest" || {
    echo "    (warning: failed to copy $dep_path)" >&2
    return
  }
  
  # Recurse into this DLL's dependencies
  fix_deps "$dest"
}

# Recursively bundle DLL dependencies
fix_deps() {
  local bin="$1"

  echo "==> Fixing deps for $bin"
  
  # Check if file exists first
  if [[ ! -f "$bin" ]]; then
    echo "    (error: file does not exist: $bin)" >&2
    return
  fi
  
  # Convert to relative path from project root (script always runs from project root)
  local rel_path="${bin#$PROJECT_ROOT/}"
  if [[ "$rel_path" == "$bin" ]]; then
    rel_path="$bin"
  fi
  
  # Try ldd first (works in MSYS2/Cygwin and when MSYS2 ldd is available in WSL)
  local ldd_cmd=""
  local use_ldd=false
  
  if is_msys2_or_cygwin; then
    # In MSYS2/Cygwin, ldd should be available
    ldd_cmd="ldd"
    use_ldd=true
    
    # If we're in WSL but detected MSYS2 env vars, try to find MSYS2's ldd
    if is_wsl; then
      local wsl_ldd
      if wsl_ldd=$(find_msys2_ldd_in_wsl); then
        ldd_cmd="$wsl_ldd"
      fi
    fi
  elif is_wsl; then
    # In WSL, try to find MSYS2's ldd
    local wsl_ldd
    if wsl_ldd=$(find_msys2_ldd_in_wsl); then
      ldd_cmd="$wsl_ldd"
      use_ldd=true
    fi
  elif command -v ldd >/dev/null 2>&1; then
    # Try system ldd (might work if it's MSYS2/Cygwin ldd)
    ldd_cmd="ldd"
    use_ldd=true
  fi
  
  # Try ldd if we found one
  if [[ "$use_ldd" == true ]] && [[ -n "$ldd_cmd" ]]; then
    local ldd_output
    ldd_output=$("$ldd_cmd" "$rel_path" 2>&1) || true
    
    # Check if ldd worked (not "not a dynamic executable" error from Linux ldd)
    if [[ -n "$ldd_output" ]] && [[ "$ldd_output" != *"not a dynamic executable"* ]] && echo "$ldd_output" | grep -q "=>"; then
      # Process ldd output: "        DLL => /path/to/dll (0x...)"
      while IFS= read -r line; do
        [[ -z "$line" ]] && continue
        [[ "$line" != *"=>"* ]] && continue
        
        # Extract DLL path
        local dep_path
        dep_path=$(echo "$line" | awk -F ' => ' '{print $2}' | awk '{print $1}')
        
        if [[ -z "$dep_path" ]] || [[ "$dep_path" == "not" ]]; then
          continue
        fi
        
        process_dep "$dep_path"
      done <<< "$ldd_output"
      return
    fi
  fi
  
  # Fall back to objdump for Docker/Linux cross-compilation
  # Only use objdump if SYSROOT is set (indicates Docker environment)
  if [[ -z "${SYSROOT:-}" ]]; then
    echo "    (error: ldd failed and SYSROOT not set - cannot determine dependencies)" >&2
    return
  fi
  
  local objdump_cmd=""
  if command -v x86_64-w64-mingw32-objdump >/dev/null 2>&1; then
    objdump_cmd="x86_64-w64-mingw32-objdump"
  elif command -v objdump >/dev/null 2>&1; then
    objdump_cmd="objdump"
  else
    echo "    (error: objdump not found - required for Docker cross-compilation)" >&2
    return
  fi
  
  # Get DLL names from PE imports
  local dll_names
  dll_names=$("$objdump_cmd" -p "$rel_path" 2>/dev/null | grep "DLL Name:" | awk '{print $3}') || true
  
  if [[ -z "$dll_names" ]]; then
    echo "    (warning: objdump produced no dependencies)" >&2
    return
  fi
  
  # Find each DLL in SYSROOT
  while IFS= read -r dll_name; do
    [[ -z "$dll_name" ]] && continue
    
    local dep_path=""
    if [[ -f "$SYSROOT/bin/$dll_name" ]]; then
      dep_path="$SYSROOT/bin/$dll_name"
    fi
    
    if [[ -n "$dep_path" ]]; then
      process_dep "$dep_path"
    fi
  done <<< "$dll_names"
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

echo "Building Windows distribution package..."
echo "  Project root:  $PROJECT_ROOT"
echo "  Distribution:  $DIST_DIR/windows_client"

# Initialize processing file
rm -f "$DLL_PROCESSED_FILE"
touch "$DLL_PROCESSED_FILE"

# Create distribution directory structure
mkdir -p "$DIST_BIN_DIR" "$DIST_RES_DIR"

# Copy binaries and resources
echo "==> Copying bin/ and res/ into distribution"
cp -r "$BIN_DIR"/* "$DIST_BIN_DIR/" 2>/dev/null || true
cp -r "$RES_DIR"/* "$DIST_RES_DIR/" 2>/dev/null || true

# Copy other distribution files
if [[ -f "$PROJECT_ROOT/create_shortcut.bat" ]]; then
  cp "$PROJECT_ROOT/create_shortcut.bat" "$DIST_DIR/windows_client/"
fi
if [[ -f "$PROJECT_ROOT/eula.txt" ]]; then
  cp "$PROJECT_ROOT/eula.txt" "$DIST_DIR/windows_client/"
fi

# Process main executable
MOAC_BIN="$DIST_BIN_DIR/moac.exe"
if [[ ! -f "$MOAC_BIN" ]]; then
  echo "ERROR: Expected $MOAC_BIN inside distribution, but it is missing." >&2
  exit 1
fi

echo "==> Fixing DLL dependencies (recursive)..."
fix_deps "$MOAC_BIN"

# Process astonia_net.dll if it exists
ASTONIA_NET_BIN="$DIST_BIN_DIR/astonia_net.dll"
if [[ -f "$ASTONIA_NET_BIN" ]]; then
  fix_deps "$ASTONIA_NET_BIN"
fi

echo "==> Windows distribution package built:"
echo "    $DIST_DIR/windows_client"
