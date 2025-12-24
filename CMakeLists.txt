cmake_minimum_required(VERSION 3.10)
project(SPDKResearch)

# Set C/C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/external/spdk/include
    ${CMAKE_SOURCE_DIR}/src
)

# SPDK build directory (assumes SPDK is built)
set(SPDK_BUILD_DIR ${CMAKE_SOURCE_DIR}/external/spdk/build)
set(DPDK_BUILD_DIR ${CMAKE_SOURCE_DIR}/external/spdk/dpdk/build)
set(DPDK_LIB_DIR ${DPDK_BUILD_DIR}/lib)

# Force CMake to prefer static libraries (.a) over shared libraries (.so)
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a;.so")

# Helper function to find static library (.a file only)
function(find_static_library VAR_NAME LIB_NAME SEARCH_PATH)
    # Find .a file directly - this ensures we get static library
    find_file(${VAR_NAME}
        NAMES lib${LIB_NAME}.a ${LIB_NAME}.a
        PATHS ${SEARCH_PATH}
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
        REQUIRED
    )
    # Verify it's actually a .a file
    if("${${VAR_NAME}}" MATCHES "\\.so")
        message(FATAL_ERROR "ERROR: Found shared library ${${VAR_NAME}} but static library required for ${LIB_NAME}")
    endif()
    # Set in parent scope
    set(${VAR_NAME} ${${VAR_NAME}} PARENT_SCOPE)
endfunction()

# Common object library for zone tools
add_library(zone_common OBJECT
    src/tests/zone_common.c
    src/tests/zone_common.h
)

# Find SPDK libraries (static .a files only)
find_static_library(SPDK_NVME_LIB spdk_nvme ${SPDK_BUILD_DIR}/lib)
find_static_library(SPDK_ENV_LIB spdk_env_dpdk ${SPDK_BUILD_DIR}/lib)
find_static_library(SPDK_LOG_LIB spdk_log ${SPDK_BUILD_DIR}/lib)
find_static_library(SPDK_UTIL_LIB spdk_util ${SPDK_BUILD_DIR}/lib)
find_static_library(SPDK_DMA_LIB spdk_dma ${SPDK_BUILD_DIR}/lib)
find_static_library(SPDK_KEYRING_LIB spdk_keyring ${SPDK_BUILD_DIR}/lib)
find_static_library(SPDK_SOCK_LIB spdk_sock ${SPDK_BUILD_DIR}/lib)
find_static_library(SPDK_TRACE_LIB spdk_trace ${SPDK_BUILD_DIR}/lib)
find_static_library(SPDK_JSON_LIB spdk_json ${SPDK_BUILD_DIR}/lib)
find_static_library(SPDK_JSONRPC_LIB spdk_jsonrpc ${SPDK_BUILD_DIR}/lib)
find_static_library(SPDK_RPC_LIB spdk_rpc ${SPDK_BUILD_DIR}/lib)

# Sequential write executable
add_executable(zone_seq_write src/tests/zone_seq_write.c)

# Single zone write test executable
add_executable(zone_single_write src/tests/zone_single_write.c)

# Multi-zone concurrent write executable
add_executable(zone_multi_write src/tests/zone_multi_write.c)

# Link directories
foreach(tgt IN ITEMS zone_seq_write zone_single_write zone_multi_write)
    target_link_directories(${tgt} PRIVATE
        ${SPDK_BUILD_DIR}/lib
        ${DPDK_LIB_DIR}
    )
endforeach()

# Share zone_common object
foreach(tgt IN ITEMS zone_seq_write zone_single_write zone_multi_write)
    target_sources(${tgt} PRIVATE $<TARGET_OBJECTS:zone_common>)
endforeach()

# Find DPDK libraries (static .a files only)
find_static_library(RTE_EAL_LIB rte_eal ${DPDK_LIB_DIR})
find_static_library(RTE_MEMPOOL_LIB rte_mempool ${DPDK_LIB_DIR})
find_static_library(RTE_RING_LIB rte_ring ${DPDK_LIB_DIR})
find_static_library(RTE_MBUF_LIB rte_mbuf ${DPDK_LIB_DIR})
find_static_library(RTE_BUS_PCI_LIB rte_bus_pci ${DPDK_LIB_DIR})
find_static_library(RTE_PCI_LIB rte_pci ${DPDK_LIB_DIR})
find_static_library(RTE_MEMPOOL_RING_LIB rte_mempool_ring ${DPDK_LIB_DIR})
find_static_library(RTE_TELEMETRY_LIB rte_telemetry ${DPDK_LIB_DIR})
find_static_library(RTE_KVARGS_LIB rte_kvargs ${DPDK_LIB_DIR})
find_static_library(RTE_RCU_LIB rte_rcu ${DPDK_LIB_DIR})
find_static_library(RTE_POWER_LIB rte_power ${DPDK_LIB_DIR})
find_static_library(RTE_ETHDEV_LIB rte_ethdev ${DPDK_LIB_DIR})
find_static_library(RTE_NET_LIB rte_net ${DPDK_LIB_DIR})
find_static_library(RTE_VHOST_LIB rte_vhost ${DPDK_LIB_DIR})
find_static_library(RTE_DMADEV_LIB rte_dmadev ${DPDK_LIB_DIR})
find_static_library(RTE_CRYPTODEV_LIB rte_cryptodev ${DPDK_LIB_DIR})
find_static_library(RTE_HASH_LIB rte_hash ${DPDK_LIB_DIR})
find_static_library(RTE_LOG_LIB rte_log ${DPDK_LIB_DIR})

# Find optional libraries (ISA-L and FUSE)
# ISA-L library (for CRC and XOR operations)
set(ISAL_DIR ${CMAKE_SOURCE_DIR}/external/spdk/isa-l)
set(ISAL_BUILD_DIR ${CMAKE_SOURCE_DIR}/external/spdk/isalbuild)
find_file(ISAL_LIB
    NAMES libisal.a
    PATHS 
        ${ISAL_DIR}/.libs
        ${ISAL_BUILD_DIR}
        ${ISAL_BUILD_DIR}/.libs
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

# FUSE library (for CUSE functionality - optional, may not be needed)
find_library(FUSE_LIB
    NAMES fuse3 fuse
    PATHS /usr/lib /usr/local/lib
)

# Helper function to find optional static library
function(find_optional_static_library VAR_NAME LIB_NAME SEARCH_PATHS)
    find_file(${VAR_NAME}
        NAMES lib${LIB_NAME}.a ${LIB_NAME}.a
        PATHS ${SEARCH_PATHS}
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
    )
    set(${VAR_NAME} ${${VAR_NAME}} PARENT_SCOPE)
endfunction()

# Apply same link set to zone tools
set(ZONE_TOOLS
    zone_seq_write
    zone_single_write
    zone_multi_write
)

foreach(tgt IN LISTS ZONE_TOOLS)
    # Force static linking for SPDK and DPDK libraries
    # Use --whole-archive for SPDK to ensure constructor functions are included
    # Link order: whole-archive -> static libs -> no-whole-archive -> dynamic libs
    target_link_libraries(${tgt} PRIVATE
        # Start whole-archive and force static linking
        -Wl,--whole-archive
        -Wl,-Bstatic
        # SPDK libraries (static)
        ${SPDK_NVME_LIB}
        ${SPDK_ENV_LIB}
        ${SPDK_LOG_LIB}
        ${SPDK_UTIL_LIB}
        ${SPDK_DMA_LIB}
        ${SPDK_KEYRING_LIB}
        ${SPDK_SOCK_LIB}
        ${SPDK_TRACE_LIB}
        ${SPDK_JSON_LIB}
        ${SPDK_JSONRPC_LIB}
        ${SPDK_RPC_LIB}
        # DPDK libraries (static)
        ${RTE_EAL_LIB}
        ${RTE_MEMPOOL_LIB}
        ${RTE_RING_LIB}
        ${RTE_MBUF_LIB}
        ${RTE_BUS_PCI_LIB}
        ${RTE_PCI_LIB}
        ${RTE_MEMPOOL_RING_LIB}
        ${RTE_TELEMETRY_LIB}
        ${RTE_KVARGS_LIB}
        ${RTE_RCU_LIB}
        ${RTE_POWER_LIB}
        ${RTE_ETHDEV_LIB}
        ${RTE_NET_LIB}
        ${RTE_VHOST_LIB}
        ${RTE_DMADEV_LIB}
        ${RTE_CRYPTODEV_LIB}
        ${RTE_HASH_LIB}
        ${RTE_LOG_LIB}
        # ISA-L library (if found, for CRC/XOR operations)
        $<$<BOOL:${ISAL_LIB}>:${ISAL_LIB}>
        # Close whole-archive and switch back to dynamic linking
        -Wl,--no-whole-archive
        -Wl,-Bdynamic
        # FUSE library (if found, for CUSE functionality)
        $<$<BOOL:${FUSE_LIB}>:${FUSE_LIB}>
        # System libraries (dynamic, usually already installed)
        pthread
        dl
        numa
        rt
        m
        # OpenSSL (required for NVMe authentication)
        ssl
        crypto
        # UUID (required by spdk_util)
        uuid
    )
endforeach()

# Handle optional dependencies
# Allow undefined symbols for optional features (CUSE, ISA-L)
# These features may not be needed for basic NVMe zone operations
foreach(tgt IN LISTS ZONE_TOOLS)
    target_link_options(${tgt} PRIVATE
        # Allow undefined symbols (for optional features like CUSE and ISA-L)
        # Note: This allows undefined symbols which may cause runtime errors if used
        -Wl,--allow-shlib-undefined
    )
endforeach()

if(NOT ISAL_LIB)
    message(WARNING "ISA-L library not found. CRC/XOR operations may fail if used.")
endif()

if(NOT FUSE_LIB)
    message(WARNING "FUSE library not found. CUSE functionality will not be available.")
endif()

# No rpath needed for static linking - all libraries are statically linked
# (except system libraries which are typically already installed)

message(STATUS "SPDK Research Project")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "SPDK path: ${CMAKE_SOURCE_DIR}/external/spdk")
message(STATUS "SPDK build dir: ${SPDK_BUILD_DIR}")
