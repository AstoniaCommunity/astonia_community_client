#!/usr/bin/env bash

# Run from the project root, or via Makefile:
#   APP_VERSION=1.2.3 build/tools/package_macos.sh
#
# This script:
#   - Builds Astonia.app bundle structure
#   - Copies bin/ and res/ into the bundle
#   - Rewrites dylib dependencies and copies non-system dylibs into Resources/bin
#   - Creates AppIcon.icns from res/moa3.ico
#
# It does NOT do any code signing. Use rcodesign (locally or in CI) to sign.

set -euo pipefail

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

APP_NAME="${APP_NAME:-Astonia Community Client}"
APP_BUNDLE_NAME="${APP_BUNDLE_NAME:-Astonia.app}"
BUNDLE_ID="${BUNDLE_ID:-com.prismaphonic.astonia}"
APP_VERSION="${APP_VERSION:-1.0.0}"
MIN_MACOS="${MIN_MACOS:-11.0}"

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

BIN_DIR="$PROJECT_ROOT/bin"
RES_DIR="$PROJECT_ROOT/res"

DIST_DIR="$PROJECT_ROOT/distrib"
APP_BUNDLE="$DIST_DIR/$APP_BUNDLE_NAME"
APP_CONTENTS="$APP_BUNDLE/Contents"
APP_MACOS="$APP_CONTENTS/MacOS"
APP_RESOURCES="$APP_CONTENTS/Resources"
APP_RES_BIN="$APP_RESOURCES/bin"
APP_RES_RES="$APP_RESOURCES/res"

INFO_PLIST="$APP_CONTENTS/Info.plist"
PKGINFO_FILE="$APP_CONTENTS/PkgInfo"
LAUNCHER="$APP_MACOS/astonia"

if [[ ! -x "$BIN_DIR/moac" ]]; then
  echo "ERROR: $BIN_DIR/moac not found (or not executable). Build the macOS binary first." >&2
  exit 1
fi

if [[ ! -d "$RES_DIR" ]]; then
  echo "ERROR: $RES_DIR not found. 'res' directory with assets is required." >&2
  exit 1
fi

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

is_system_lib() {
  local path="$1"
  case "$path" in
    /usr/lib/*|/System/*)
      return 0 ;;
    *)
      return 1 ;;
  esac
}

# Recursively fix deps of a given Mach-O binary:
# - Copy non-system dylibs into APP_RES_BIN
# - Set their install_name id to @loader_path/<basename>
# - Rewrite this binary's dep paths to @loader_path/<basename>
fix_deps() {
  local bin="$1"

  echo "==> Fixing deps for $bin"
  if ! otool -L "$bin" >/dev/null 2>&1; then
    echo "    (warning: $bin is not a valid Mach-O or otool failed, skipping deps)" >&2
    return
  fi

  # otool -L output: first line is the binary itself, skip it.
  otool -L "$bin" | tail -n +2 | awk '{print $1}' | while read -r dep; do
    [[ -z "$dep" ]] && continue

    # Skip already-relative entries
    case "$dep" in
      @*) continue ;;
    esac

    # Skip system libs
    if is_system_lib "$dep"; then
      continue
    fi

    local base dest new_ref
    base="$(basename "$dep")"
    dest="$APP_RES_BIN/$base"
    new_ref="@loader_path/$base"

    # Copy dep if not already in our bundle
    if [[ ! -f "$dest" ]]; then
      echo "==>   Copying $dep -> $dest"
      cp "$dep" "$dest"

      echo "==>   Setting install_name id on $dest -> $new_ref"
      install_name_tool -id "$new_ref" "$dest" || true

      # Recurse into this dylib's deps
      fix_deps "$dest"
    fi

    echo "==>   Rewriting dep in $bin: $dep -> $new_ref"
    install_name_tool -change "$dep" "$new_ref" "$bin" || true
  done
}

make_icns_from_ico() {
  local ICO_IN="$1"           # e.g. res/moa3.ico (inside bundle)
  local ICNS_OUT="$2"         # e.g. Astonia.icns
  local TMP_ICONSET
  TMP_ICONSET="$(mktemp -d /tmp/astonia-iconset.XXXXXX)"

  echo "==> Building .icns from $ICO_IN -> $ICNS_OUT"

  if ! command -v convert >/dev/null 2>&1; then
    echo "ERROR: 'convert' (ImageMagick) not found. Install via 'brew install imagemagick'." >&2
    rm -rf "$TMP_ICONSET"
    return 1
  fi

  local BASE_PNG="$TMP_ICONSET/base.png"
  convert "$ICO_IN[0]" "$BASE_PNG"

  if ! command -v sips >/dev/null 2>&1; then
    echo "ERROR: 'sips' not found (it should be present on macOS)." >&2
    rm -rf "$TMP_ICONSET"
    return 1
  fi

  resize_icon() {
    local SIZE="$1"      # logical size (16, 32, 64, 128, 256, 512)
    local SCALE="$2"     # 1 or 2
    local NAME="$3"      # icon_16x16.png, icon_16x16@2x.png, etc.
    local PIXELS
    PIXELS=$(( SIZE * SCALE ))
    sips -z "$PIXELS" "$PIXELS" "$BASE_PNG" --out "$TMP_ICONSET/$NAME" >/dev/null
  }

  # 16x16
  resize_icon 16 1 icon_16x16.png
  resize_icon 16 2 icon_16x16@2x.png
  # 32x32
  resize_icon 32 1 icon_32x32.png
  resize_icon 32 2 icon_32x32@2x.png
  # 128x128
  resize_icon 128 1 icon_128x128.png
  resize_icon 128 2 icon_128x128@2x.png
  # 256x256
  resize_icon 256 1 icon_256x256.png
  resize_icon 256 2 icon_256x256@2x.png
  # 512x512
  resize_icon 512 1 icon_512x512.png
  resize_icon 512 2 icon_512x512@2x.png

  local ICONSET_DIR="${TMP_ICONSET%.XXXXXX}.iconset"
  mv "$TMP_ICONSET" "$ICONSET_DIR"

  if ! command -v iconutil >/dev/null 2>&1; then
    echo "ERROR: 'iconutil' not found (should be on macOS with Xcode tools)." >&2
    rm -rf "$ICONSET_DIR"
    return 1
  fi

  iconutil -c icns "$ICONSET_DIR" -o "$ICNS_OUT"
  rm -rf "$ICONSET_DIR"

  echo "==> Created $ICNS_OUT"
}

# ---------------------------------------------------------------------------
# Bundle skeleton
# ---------------------------------------------------------------------------

echo "Building macOS .app bundle (UNSIGNED)..."
echo "  Project root:  $PROJECT_ROOT"
echo "  App bundle:    $APP_BUNDLE"

rm -rf "$APP_BUNDLE"
mkdir -p "$APP_MACOS" "$APP_RES_BIN" "$APP_RES_RES"

# Info.plist
cat > "$INFO_PLIST" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
 "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key>
  <string>${APP_NAME}</string>
  <key>CFBundleDisplayName</key>
  <string>${APP_NAME}</string>
  <key>CFBundleIdentifier</key>
  <string>${BUNDLE_ID}</string>
  <key>CFBundleIconFile</key>
  <string>AppIcon</string>
  <key>CFBundleVersion</key>
  <string>${APP_VERSION}</string>
  <key>CFBundleShortVersionString</key>
  <string>${APP_VERSION}</string>
  <key>CFBundleExecutable</key>
  <string>astonia</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>LSMinimumSystemVersion</key>
  <string>${MIN_MACOS}</string>
  <key>NSHighResolutionCapable</key>
  <true/>
</dict>
</plist>
EOF

# PkgInfo
echo "APPL????" > "$PKGINFO_FILE"

# ---------------------------------------------------------------------------
# Copy bin/ and res/ into Resources
# ---------------------------------------------------------------------------

echo "==> Copying bin/ and res/ into bundle Resources"
cp -R "$BIN_DIR"/. "$APP_RES_BIN/"
cp -R "$RES_DIR"/. "$APP_RES_RES/"

# Build .icns from our ICO and place it in Resources
if [[ -f "$APP_RES_RES/moa3.ico" ]]; then
  make_icns_from_ico "$APP_RES_RES/moa3.ico" "$APP_RESOURCES/AppIcon.icns" || true
  rm -f "$APP_RES_RES/moa3.ico"
fi

# Remove any dSYM directories
find "$APP_RES_BIN" -name "*.dSYM" -exec rm -rf {} + 2>/dev/null || true

# ---------------------------------------------------------------------------
# Launcher binary (Contents/MacOS/astonia)
# ---------------------------------------------------------------------------

LAUNCHER_SRC_BIN="$BIN_DIR/astonia_launcher"

if [[ ! -x "$LAUNCHER_SRC_BIN" ]]; then
  echo "ERROR: $LAUNCHER_SRC_BIN not found (or not executable). Build the macOS launcher first." >&2
  exit 1
fi

cp "$LAUNCHER_SRC_BIN" "$LAUNCHER"
chmod +x "$LAUNCHER"

# ---------------------------------------------------------------------------
# Rewrite dependencies & copy non-system dylibs
# ---------------------------------------------------------------------------

MOAC_BIN="$APP_RES_BIN/moac"
ASTONIA_NET_BIN="$APP_RES_BIN/libastonia_net.dylib"

if [[ ! -x "$MOAC_BIN" ]]; then
  echo "ERROR: Expected $MOAC_BIN inside bundle, but it is missing or not executable." >&2
  exit 1
fi

echo "==> Fixing dylib dependencies (recursive)..."
fix_deps "$MOAC_BIN"

if [[ -f "$ASTONIA_NET_BIN" ]]; then
  fix_deps "$ASTONIA_NET_BIN"
fi

echo "==> macOS .app bundle built (UNSIGNED):"
echo "    $APP_BUNDLE"