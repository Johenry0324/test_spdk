#!/bin/bash
# Environment setup script for SPDK Research Project

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "Setting up SPDK Research Project environment..."

# Check if running on Windows (Git Bash or WSL)
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    echo "Detected Windows environment"
    # Windows-specific setup
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "Detected Linux environment"
    # Linux-specific setup
    # Check for hugepages
    if [ -d /sys/kernel/mm/hugepages ]; then
        echo "Hugepages support detected"
    fi
fi

# Check for required tools
echo "Checking required tools..."

command -v git >/dev/null 2>&1 || { echo "Error: git is required but not installed."; exit 1; }
command -v cmake >/dev/null 2>&1 || { echo "Error: cmake is required but not installed."; exit 1; }
command -v make >/dev/null 2>&1 || { echo "Warning: make not found. You may need to install build tools."; }

# Initialize submodules if not already done
if [ -f "$PROJECT_ROOT/.gitmodules" ]; then
    echo "Initializing Git submodules..."
    cd "$PROJECT_ROOT"
    git submodule update --init --recursive || echo "Warning: Submodule initialization failed"
fi

# Check SPDK submodule
if [ -d "$PROJECT_ROOT/external/spdk" ]; then
    echo "SPDK submodule found at: $PROJECT_ROOT/external/spdk"
    cd "$PROJECT_ROOT/external/spdk"
    SPDK_VERSION=$(git describe --tags --always 2>/dev/null || echo "unknown")
    echo "SPDK version: $SPDK_VERSION"
else
    echo "Warning: SPDK submodule not found. Run: git submodule update --init --recursive"
fi

echo ""
echo "Environment setup complete!"
echo ""
echo "Next steps:"
echo "  1. Apply patches: ./scripts/apply_patches.sh"
echo "  2. Build project: mkdir build && cd build && cmake .. && make"

