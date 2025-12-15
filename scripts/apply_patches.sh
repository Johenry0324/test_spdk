#!/bin/bash
# Apply SPDK patches script

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PATCHES_DIR="$PROJECT_ROOT/src/patches"
SPDK_DIR="$PROJECT_ROOT/external/spdk"

echo "Applying SPDK patches..."

# Check if SPDK directory exists
if [ ! -d "$SPDK_DIR" ]; then
    echo "Error: SPDK directory not found at $SPDK_DIR"
    echo "Please run: git submodule update --init --recursive"
    exit 1
fi

# Check if patches directory exists
if [ ! -d "$PATCHES_DIR" ]; then
    echo "Warning: Patches directory not found at $PATCHES_DIR"
    echo "Creating directory..."
    mkdir -p "$PATCHES_DIR"
    echo "No patches to apply. Place patch files (*.patch) in $PATCHES_DIR"
    exit 0
fi

# Find all patch files and sort them
PATCHES=$(find "$PATCHES_DIR" -name "*.patch" -type f | sort)

if [ -z "$PATCHES" ]; then
    echo "No patch files found in $PATCHES_DIR"
    exit 0
fi

echo "Found patch files:"
echo "$PATCHES" | while read -r patch; do
    echo "  - $(basename "$patch")"
done

# Apply patches
cd "$SPDK_DIR"
APPLIED=0
FAILED=0

echo ""
echo "Applying patches..."

for patch in $PATCHES; do
    PATCH_NAME=$(basename "$patch")
    echo ""
    echo "Applying: $PATCH_NAME"
    
    if git apply --check "$patch" 2>/dev/null; then
        if git apply "$patch" 2>/dev/null; then
            echo "  ✓ Successfully applied"
            ((APPLIED++))
        else
            echo "  ✗ Failed to apply (git apply failed)"
            ((FAILED++))
        fi
    else
        # Check if already applied
        if git apply --reverse --check "$patch" 2>/dev/null; then
            echo "  ⊙ Already applied (skipping)"
        else
            echo "  ✗ Failed to apply (check failed - may need manual intervention)"
            ((FAILED++))
        fi
    fi
done

echo ""
echo "Summary:"
echo "  Applied: $APPLIED"
echo "  Failed:  $FAILED"
echo "  Total:   $((APPLIED + FAILED))"

if [ $FAILED -gt 0 ]; then
    echo ""
    echo "Warning: Some patches failed to apply. Please review manually."
    exit 1
fi

echo ""
echo "All patches applied successfully!"

