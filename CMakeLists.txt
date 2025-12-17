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

# Common object library for zone tools
add_library(zone_common OBJECT
    src/tests/zone_common.c
    src/tests/zone_common.h
)

# Find SPDK libraries
find_library(SPDK_NVME_LIB 
    NAMES spdk_nvme
    PATHS ${SPDK_BUILD_DIR}/lib
    NO_DEFAULT_PATH
    REQUIRED
)

find_library(SPDK_ENV_LIB
    NAMES spdk_env_dpdk
    PATHS ${SPDK_BUILD_DIR}/lib
    NO_DEFAULT_PATH
    REQUIRED
)

find_library(SPDK_LOG_LIB
    NAMES spdk_log
    PATHS ${SPDK_BUILD_DIR}/lib
    NO_DEFAULT_PATH
    REQUIRED
)

find_library(SPDK_UTIL_LIB
    NAMES spdk_util
    PATHS ${SPDK_BUILD_DIR}/lib
    NO_DEFAULT_PATH
    REQUIRED
)

find_library(SPDK_DMA_LIB
    NAMES spdk_dma
    PATHS ${SPDK_BUILD_DIR}/lib
    NO_DEFAULT_PATH
    REQUIRED
)

find_library(SPDK_KEYRING_LIB
    NAMES spdk_keyring
    PATHS ${SPDK_BUILD_DIR}/lib
    NO_DEFAULT_PATH
)

find_library(SPDK_SOCK_LIB
    NAMES spdk_sock
    PATHS ${SPDK_BUILD_DIR}/lib
    NO_DEFAULT_PATH
)

find_library(SPDK_TRACE_LIB
    NAMES spdk_trace
    PATHS ${SPDK_BUILD_DIR}/lib
    NO_DEFAULT_PATH
)

find_library(SPDK_JSON_LIB
    NAMES spdk_json
    PATHS ${SPDK_BUILD_DIR}/lib
    NO_DEFAULT_PATH
    REQUIRED
)

find_library(SPDK_JSONRPC_LIB
    NAMES spdk_jsonrpc
    PATHS ${SPDK_BUILD_DIR}/lib
    NO_DEFAULT_PATH
    REQUIRED
)

find_library(SPDK_RPC_LIB
    NAMES spdk_rpc
    PATHS ${SPDK_BUILD_DIR}/lib
    NO_DEFAULT_PATH
    REQUIRED
)

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

# Find DPDK libraries
find_library(RTE_EAL_LIB rte_eal PATHS ${DPDK_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
find_library(RTE_MEMPOOL_LIB rte_mempool PATHS ${DPDK_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
find_library(RTE_RING_LIB rte_ring PATHS ${DPDK_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
find_library(RTE_MBUF_LIB rte_mbuf PATHS ${DPDK_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
find_library(RTE_BUS_PCI_LIB rte_bus_pci PATHS ${DPDK_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
find_library(RTE_PCI_LIB rte_pci PATHS ${DPDK_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
find_library(RTE_MEMPOOL_RING_LIB rte_mempool_ring PATHS ${DPDK_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
find_library(RTE_TELEMETRY_LIB rte_telemetry PATHS ${DPDK_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
find_library(RTE_KVARGS_LIB rte_kvargs PATHS ${DPDK_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
find_library(RTE_RCU_LIB rte_rcu PATHS ${DPDK_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
find_library(RTE_POWER_LIB rte_power PATHS ${DPDK_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
find_library(RTE_ETHDEV_LIB rte_ethdev PATHS ${DPDK_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
find_library(RTE_NET_LIB rte_net PATHS ${DPDK_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
find_library(RTE_VHOST_LIB rte_vhost PATHS ${DPDK_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
find_library(RTE_DMADEV_LIB rte_dmadev PATHS ${DPDK_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
find_library(RTE_CRYPTODEV_LIB rte_cryptodev PATHS ${DPDK_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
find_library(RTE_HASH_LIB rte_hash PATHS ${DPDK_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
find_library(RTE_LOG_LIB rte_log PATHS ${DPDK_LIB_DIR} NO_DEFAULT_PATH REQUIRED)

# Apply same link set to zone tools
set(ZONE_TOOLS
    zone_seq_write
    zone_single_write
    zone_multi_write
)

foreach(tgt IN LISTS ZONE_TOOLS)
    target_link_libraries(${tgt}
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
        # DPDK libraries
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
        # System libraries
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

# Set rpath for runtime library loading
foreach(tgt IN ITEMS zone_seq_write zone_single_write zone_multi_write)
    set_target_properties(${tgt} PROPERTIES
        INSTALL_RPATH "${SPDK_BUILD_DIR}/lib:${DPDK_LIB_DIR}"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endforeach()

message(STATUS "SPDK Research Project")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "SPDK path: ${CMAKE_SOURCE_DIR}/external/spdk")
message(STATUS "SPDK build dir: ${SPDK_BUILD_DIR}")
