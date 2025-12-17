# Tests

This directory contains test code for the project.

## Structure

- `unit/` - Unit tests
- `integration/` - Integration tests
- `performance/` - Performance tests

## Zone Write Test Tools

This directory contains three zone write test tools for NVMe ZNS devices:

### zone_seq_write

Sequential write tool that writes multiple zones sequentially.

**Usage:**
```bash
zone_seq_write [options] <device> <total_lba> <zone_size_lba> <chunk_lba>
```

**Options:**
- `-o <n>`   - Number of open zones (default: 8)
- `-p <id>`  - Pattern ID (0: 0xAB default, 1: 0x55, 2: 0x11)
- `-d <MB>`  - DPDK memory size in MB
- `-i <id>`  - Shared memory ID
- `-r <trid>` - NVMe transport ID (e.g., 0000:01:00.0)
- `-h`       - Show help

**Parameters:**
- `<device>` - NVMe device identifier
- `<total_lba>` - Total LBA to cover
- `<zone_size_lba>` - Size of each zone in LBA
- `<chunk_lba>` - Chunk size in LBA for each write operation

**Example:**
```bash
zone_seq_write -o 8 device 1000000 100000 1000
```

### zone_single_write

Writes a single zone to full capacity.

**Usage:**
```bash
zone_single_write [options] <device> <zone_size_lba> <zone_index> <chunk_lba>
```

**Options:**
- `-i <id>`  - Shared memory ID
- `-r <trid>` - NVMe transport ID (e.g., 0000:01:00.0)
- `-h`       - Show help

**Parameters:**
- `<device>` - NVMe device identifier
- `<zone_size_lba>` - Size of each zone in LBA
- `<zone_index>` - Index of zone to write (0-based). LBA = zone_index * zone_size_lba
- `<chunk_lba>` - Chunk size in LBA for each write operation

**Notes:**
- Writes data using LBA address as data (no pattern)
- DPDK memory size is fixed at 2048MB (for up to 8 open zones, 200MB each)

**Example:**
```bash
zone_single_write device 100000 5 1000
```

### zone_multi_write

Writes multiple zones concurrently using threads. Writes 8 zones starting from base_zone_index.

**Usage:**
```bash
zone_multi_write [options] <device> <zone_size_lba> <base_zone_index> <chunk_lba>
```

**Options:**
- `-i <id>`  - Shared memory ID
- `-r <trid>` - NVMe transport ID (e.g., 0000:01:00.0)
- `-h`       - Show help

**Parameters:**
- `<device>` - NVMe device identifier
- `<zone_size_lba>` - Size of each zone in LBA
- `<base_zone_index>` - Starting zone index (0-based). Writes zones from base_zone_index to base_zone_index + 7
- `<chunk_lba>` - Chunk size in LBA for each write operation

**Notes:**
- Writes 8 zones concurrently (zone_index = base_zone_index .. base_zone_index + 7)
- LBA = zone_index * zone_size_lba
- Writes data using LBA address as data (no pattern)
- DPDK memory size is fixed at 2048MB (for up to 8 open zones, 200MB each)

**Example:**
```bash
zone_multi_write device 100000 0 1000
```

## Running Tests

```bash
# From build directory
ctest

# Or run specific test
./bin/test_name
```

## Test Guidelines

1. Use a consistent testing framework
2. Keep tests independent and isolated
3. Document test requirements and setup
4. Include both positive and negative test cases

