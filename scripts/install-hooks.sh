#!/bin/bash
#
# Install git hooks for development
#
# Usage: scripts/install-hooks.sh
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
HOOKS_SRC="$SCRIPT_DIR/hooks"
HOOKS_DST="$REPO_ROOT/.git/hooks"

echo "Installing git hooks..."

# Ensure we're in a git repository
if [ ! -d "$REPO_ROOT/.git" ]; then
    echo "Error: Not a git repository"
    exit 1
fi

# Install pre-commit hook
if [ -f "$HOOKS_SRC/pre-commit" ]; then
    cp "$HOOKS_SRC/pre-commit" "$HOOKS_DST/pre-commit"
    chmod +x "$HOOKS_DST/pre-commit"
    echo "  ✓ Installed pre-commit hook (auto-formats code on commit)"
fi

echo ""
echo "Done! Hooks installed successfully."
echo ""
echo "The pre-commit hook will automatically run 'make lint' and"
echo "re-stage formatted files before each commit."
echo ""
echo "To bypass the hook for a single commit, use: git commit --no-verify"
