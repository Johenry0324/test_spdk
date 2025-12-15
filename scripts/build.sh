#!/bin/bash
# Build script for sequential_write tool

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=========================================="
echo "Building sequential_write tool"
echo "=========================================="

# Check if cmake is installed
if ! command -v cmake &> /dev/null; then
    echo "ERROR: cmake is not installed"
    echo "Please install cmake:"
    echo "  Ubuntu/Debian: sudo apt-get install cmake"
    echo "  Fedora: sudo dnf install cmake"
    exit 1
fi

# Check if make is installed
if ! command -v make &> /dev/null; then
    echo "ERROR: make is not installed"
    echo "Please install build-essential:"
    echo "  Ubuntu/Debian: sudo apt-get install build-essential"
    exit 1
fi

# Check if SPDK is built
if [ ! -d "$PROJECT_ROOT/external/spdk/build/lib" ]; then
    echo "ERROR: SPDK is not built yet"
    echo "Please build SPDK first:"
    echo "  cd external/spdk"
    echo "  ./configure"
    echo "  make"
    exit 1
fi

# Create build directory
cd "$PROJECT_ROOT"
mkdir -p build
cd build

# Configure with CMake
echo ""
echo "Configuring CMake..."
cmake ..

# Build
echo ""
echo "Building sequential_write..."
make -j$(nproc 2>/dev/null || echo 4)

echo ""
echo "=========================================="
echo "Build completed successfully!"
echo "=========================================="
echo ""
echo "Executable location: $PROJECT_ROOT/build/bin/sequential_write"
echo ""
echo "Usage example:"
echo "  ./build/bin/sequential_write -r 0000:01:00.0 1000 32"
echo "  (Replace 0000:01:00.0 with your NVMe device PCIe address)"
echo ""

