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
    src/zone_common.c
    src/zone_common.h
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
add_executable(sequential_write src/sequential_write.c)

add_executable(multi_zone_write src/multi_zone_write.c)
add_executable(zone_deallocate src/zone_deallocate.c)
add_executable(zone_seq_write src/zone_seq_write.c)
add_executable(zone_seq_read_verify src/zone_seq_read_verify.c)
add_executable(zone_partial_write src/zone_partial_write.c)
add_executable(zone_reset_rewrite src/zone_reset_rewrite.c)
add_executable(zone_boundary_guard src/zone_boundary_guard.c)
add_executable(zone_spot_read src/zone_spot_read.c)
add_executable(zone_full_cycle src/zone_full_cycle.c)
add_executable(zone_partial_cycle src/zone_partial_cycle.c)
add_executable(zone_reset_cycle src/zone_reset_cycle.c)
add_executable(zone_mixed_sanity src/zone_mixed_sanity.c)

# Link directories
target_link_directories(sequential_write PRIVATE
    ${SPDK_BUILD_DIR}/lib
    ${DPDK_LIB_DIR}
)
target_link_directories(multi_zone_write PRIVATE
    ${SPDK_BUILD_DIR}/lib
    ${DPDK_LIB_DIR}
)
target_link_directories(zone_deallocate PRIVATE
    ${SPDK_BUILD_DIR}/lib
    ${DPDK_LIB_DIR}
)
foreach(tgt IN ITEMS zone_seq_write zone_seq_read_verify zone_partial_write zone_reset_rewrite zone_boundary_guard zone_spot_read zone_full_cycle zone_partial_cycle zone_reset_cycle zone_mixed_sanity)
    target_link_directories(${tgt} PRIVATE
        ${SPDK_BUILD_DIR}/lib
        ${DPDK_LIB_DIR}
    )
endforeach()

# Share zone_common object
foreach(tgt IN ITEMS multi_zone_write zone_deallocate zone_seq_write zone_seq_read_verify zone_partial_write zone_reset_rewrite zone_boundary_guard zone_spot_read zone_full_cycle zone_partial_cycle zone_reset_cycle zone_mixed_sanity)
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

# Link SPDK libraries
target_link_libraries(sequential_write
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

target_link_libraries(multi_zone_write
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
    # OpenSSL (NVMe auth)
    ssl
    crypto
    # UUID
    uuid
)

target_link_libraries(zone_deallocate
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

# Apply same link set to other zone tools
set(ZONE_TOOLS
    zone_seq_write
    zone_seq_read_verify
    zone_partial_write
    zone_reset_rewrite
    zone_boundary_guard
    zone_spot_read
    zone_full_cycle
    zone_partial_cycle
    zone_reset_cycle
    zone_mixed_sanity
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
set_target_properties(sequential_write PROPERTIES
    INSTALL_RPATH "${SPDK_BUILD_DIR}/lib:${DPDK_LIB_DIR}"
    BUILD_WITH_INSTALL_RPATH TRUE
)
set_target_properties(multi_zone_write PROPERTIES
    INSTALL_RPATH "${SPDK_BUILD_DIR}/lib:${DPDK_LIB_DIR}"
    BUILD_WITH_INSTALL_RPATH TRUE
)
set_target_properties(zone_deallocate PROPERTIES
    INSTALL_RPATH "${SPDK_BUILD_DIR}/lib:${DPDK_LIB_DIR}"
    BUILD_WITH_INSTALL_RPATH TRUE
)
foreach(tgt IN ITEMS zone_seq_write zone_seq_read_verify zone_partial_write zone_reset_rewrite zone_boundary_guard zone_spot_read zone_full_cycle zone_partial_cycle zone_reset_cycle zone_mixed_sanity)
    set_target_properties(${tgt} PROPERTIES
        INSTALL_RPATH "${SPDK_BUILD_DIR}/lib:${DPDK_LIB_DIR}"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endforeach()

message(STATUS "SPDK Research Project")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "SPDK path: ${CMAKE_SOURCE_DIR}/external/spdk")
message(STATUS "SPDK build dir: ${SPDK_BUILD_DIR}")

