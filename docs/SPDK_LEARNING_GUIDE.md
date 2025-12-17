# SPDK Framework Learning Guide

## Overview

SPDK (Storage Performance Development Kit) is a set of tools and libraries for writing high performance, scalable, user-mode storage applications. This guide provides a structured learning path to effectively understand the SPDK framework.

## Learning Path

### Phase 1: Foundation (Week 1-2)

#### 1.1 Understand Core Concepts
- **Start with documentation:**
  - Read `external/spdk/doc/getting_started.md` - Basic setup and build
  - Read `external/spdk/doc/concepts.md` - Core architectural concepts
  - Read `external/spdk/doc/userspace.md` - Userspace I/O model
  - Read `external/spdk/doc/memory.md` - Memory management (hugepages, DPDK)
  - Read `external/spdk/doc/concurrency.md` - Event-driven, lockless design

#### 1.2 Key Concepts to Master
- **Userspace I/O:** SPDK bypasses kernel for direct hardware access
- **Polling vs Interrupts:** Polling mode for low latency
- **Event Framework:** Asynchronous, event-driven architecture
- **Memory Pools:** Hugepage allocation for zero-copy operations
- **Lockless Design:** Per-core data structures to avoid locks

### Phase 2: Hands-on Examples (Week 3-4)

#### 2.1 Start with Simple Examples
Study these examples in order:

1. **NVMe Hello World** (`external/spdk/examples/nvme/hello_world/hello_world.c`)
   - Basic NVMe controller initialization
   - Namespace enumeration
   - Simple read/write operations
   - Completion callbacks

2. **Bdev Hello World** (`external/spdk/examples/bdev/hello_world/hello_bdev.c`)
   - Block device abstraction layer
   - Bdev operations (read/write)
   - Configuration via JSON

3. **Blob Hello World** (`external/spdk/examples/blob/hello_world/hello_blob.c`)
   - Blobstore (block-level object store)
   - Blob creation and I/O

#### 2.2 Your Existing Code
Review your zone-related code:
- `src/zone_common.c` - Common zone operations
- `src/zone_seq_write.c` - Sequential write patterns
- `src/zone_reset_cycle.c` - Zone reset operations

These demonstrate:
- NVMe ZNS (Zoned Namespace) support
- Zone management operations
- Sequential write constraints

### Phase 3: Core Libraries (Week 5-6)

#### 3.1 Essential Libraries
Study the source code in `external/spdk/lib/`:

1. **NVMe Library** (`lib/nvme/`)
   - Controller management
   - Queue pair (qpair) operations
   - Command submission and completion
   - Transport abstraction (PCIe, RDMA, TCP)

2. **Bdev Library** (`lib/bdev/`)
   - Block device abstraction
   - I/O channel management
   - Bdev modules (null, nvme, malloc, etc.)

3. **Event Framework** (`lib/event/`)
   - Reactor pattern
   - Event scheduling
   - Thread management

4. **Environment Abstraction** (`lib/env_dpdk/`)
   - DPDK integration
   - Memory management
   - CPU affinity

#### 3.2 Key Data Structures
Understand these core structures:
- `struct spdk_nvme_ctrlr` - NVMe controller
- `struct spdk_nvme_ns` - Namespace
- `struct spdk_nvme_qpair` - Queue pair for I/O
- `struct spdk_bdev` - Block device
- `struct spdk_bdev_io` - Block device I/O request

### Phase 4: Applications (Week 7-8)

#### 4.1 Study Real Applications
Examine applications in `external/spdk/app/`:

1. **spdk_tgt** (`app/spdk_tgt/spdk_tgt.c`)
   - Main target application
   - JSON-RPC interface
   - Plugin system

2. **nvmf_tgt** (`app/nvmf_tgt/nvmf_main.c`)
   - NVMe-oF target implementation
   - Network transport (RDMA, TCP)
   - Subsystem management

3. **vhost** (`app/vhost/vhost.c`)
   - vhost-user implementation
   - Virtual machine integration

#### 4.2 Tools
- `spdk_nvme_perf` - Performance testing
- `spdk_nvme_identify` - Device identification
- `bdevperf` - Block device performance

### Phase 5: Advanced Topics (Week 9+)

#### 5.1 Advanced Features
- **NVMe-oF:** Network-based NVMe (`doc/nvmf.md`)
- **Vhost:** Virtualization support (`doc/vhost.md`)
- **iSCSI Target:** SCSI over IP (`doc/iscsi.md`)
- **Blobstore:** High-level storage (`doc/blob.md`)
- **Logical Volumes:** Volume management (`doc/lvol.md`)

#### 5.2 Performance Optimization
- Read `doc/performance_reports.md`
- Understand polling vs interrupt mode
- Learn about CPU affinity
- Study zero-copy techniques

## Practical Learning Steps

### Step 1: Build and Run Examples
```bash
cd external/spdk
./configure
make

# Setup environment (as root)
sudo scripts/setup.sh

# Run hello world example
sudo ./build/examples/hello_world
```

### Step 2: Modify Examples
- Add logging to understand flow
- Modify buffer sizes
- Change I/O patterns
- Add error handling

### Step 3: Read Source Code
- Start with simple functions
- Trace function calls
- Understand data flow
- Use debugger (gdb) with SPDK macros

### Step 4: Write Your Own Code
- Start with simple bdev operations
- Progress to NVMe direct access
- Implement custom bdev modules
- Build complete applications

## Key Documentation Files

Essential documentation in `external/spdk/doc/`:

| File | Purpose |
|------|---------|
| `getting_started.md` | Build and setup |
| `concepts.md` | Core concepts overview |
| `userspace.md` | Userspace I/O model |
| `memory.md` | Memory management |
| `concurrency.md` | Concurrency model |
| `nvme.md` | NVMe library guide |
| `bdev.md` | Block device layer |
| `nvmf.md` | NVMe-oF target |
| `event.md` | Event framework |
| `prog_guides.md` | Programming guides |

## Code Reading Strategy

### 1. Top-Down Approach
- Start with application code (`app/`)
- Trace into library code (`lib/`)
- Understand module code (`module/`)

### 2. Bottom-Up Approach
- Start with low-level NVMe operations
- Build up to bdev abstraction
- Understand application layer

### 3. Use Debugging Tools
- GDB with SPDK macros (`doc/gdb_macros.md`)
- SPDK tracing (`doc/tracing.md`)
- `spdk_top` for runtime monitoring

## Common Patterns

### 1. Initialization Pattern
```c
// 1. Initialize environment
spdk_env_opts_init(&opts);
spdk_env_init(&opts);

// 2. Probe and attach devices
spdk_nvme_probe(...);

// 3. Create qpairs
qpair = spdk_nvme_ctrlr_alloc_io_qpair(...);

// 4. Submit I/O
spdk_nvme_ns_cmd_read(...);

// 5. Poll for completions
spdk_nvme_qpair_process_completions(...);
```

### 2. Event-Driven Pattern
```c
// Register event handler
spdk_event_call(spdk_event_allocate(...));

// In handler, submit async I/O
// Completion callback processes results
```

### 3. Bdev Pattern
```c
// Get bdev by name
bdev = spdk_bdev_get_by_name(name);

// Open bdev
desc = spdk_bdev_open(...);

// Get I/O channel
ch = spdk_bdev_get_io_channel(desc);

// Submit I/O
spdk_bdev_read(...);
```

## Resources

### Official Resources
- **GitHub:** https://github.com/spdk/spdk
- **Documentation:** https://spdk.io/doc/
- **Mailing List:** spdk@lists.01.org
- **Slack:** spdkteam.slack.com

### Learning Materials
- SPDK blog posts
- Conference presentations (SPDK Summit)
- Performance reports
- Example applications

## Tips for Effective Learning

1. **Read Code Regularly:** Spend time reading SPDK source code daily
2. **Run Examples:** Don't just read, run and modify examples
3. **Use Debugger:** Step through code to understand execution flow
4. **Ask Questions:** Use mailing list or Slack for clarification
5. **Contribute:** Fix bugs or add features to deepen understanding
6. **Study Your Code:** Your zone-related code is excellent reference material

## Next Steps

Based on your current codebase:
1. Review your zone operations in `src/zone_*.c`
2. Compare with SPDK's ZNS examples
3. Understand how your code uses SPDK APIs
4. Identify areas for optimization
5. Explore advanced zone management features

## Conclusion

SPDK is a complex framework, but following this structured approach will help you:
- Understand the architecture
- Master the APIs
- Write efficient storage applications
- Optimize performance

Remember: **Code before planning, code after asking for confirm** (per your workspace rules).

Good luck with your SPDK journey!
