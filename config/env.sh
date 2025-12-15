# Environment variables for SPDK Research Project

# SPDK paths
export SPDK_ROOT="${SPDK_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/external/spdk}"
export SPDK_BUILD_DIR="${SPDK_BUILD_DIR:-${SPDK_ROOT}/build}"

# Build configuration
export CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"

# Hugepages (Linux only)
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    export HUGEMEM="${HUGEMEM:-4096}"
fi

# Add SPDK scripts to PATH
export PATH="${SPDK_ROOT}/scripts:${PATH}"

echo "SPDK Research Project environment loaded"
echo "  SPDK_ROOT: $SPDK_ROOT"
echo "  SPDK_BUILD_DIR: $SPDK_BUILD_DIR"
echo "  CMAKE_BUILD_TYPE: $CMAKE_BUILD_TYPE"

