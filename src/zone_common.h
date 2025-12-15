/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once

#include "spdk/stdinc.h"
#include "spdk/nvme.h"
#include "spdk/env.h"
#include "spdk/string.h"
#include "spdk/log.h"

#include <stdint.h>

struct zone_desc {
	uint64_t start_lba;
	uint64_t current_lba;
	uint64_t end_lba; /* exclusive */
	int active;
};

struct zone_opts {
	uint64_t total_lba;
	uint64_t zone_size_lba;
	uint32_t chunk_lba;
	int open_zones;      /* concurrent zones */
	int pattern_id;      /* 0:0xAB, 1:0x55, 2:0x11, default 0 */
	int percent;         /* for partial write, 1-100, default 100 */
};

struct env_opts {
	int mem_size;
	int shm_id;
	struct spdk_nvme_transport_id trid;
	const char *device;
};

struct io_stat {
	uint64_t io_submitted;
	uint64_t io_completed;
	uint64_t lba_written;
	uint64_t lba_read;
};

/* Common init/cleanup */
int zone_init_env(struct env_opts *eopts, struct spdk_env_opts *out_env, struct spdk_nvme_ctrlr **ctrlr, struct spdk_nvme_ns **ns);
void zone_cleanup(struct spdk_nvme_ctrlr *ctrlr);
struct spdk_nvme_qpair *zone_alloc_qpair(struct spdk_nvme_ns *ns);

/* Zone helpers */
int zone_build_table(struct zone_desc **zones_out, int *zone_count_out, uint64_t total_lba, uint64_t zone_size_lba, int open_zones);
void zone_reset_all(struct zone_desc *zones, int zone_count);

/* Pattern helpers */
uint8_t zone_pattern_byte(int pattern_id);
void zone_fill_pattern(void *buf, size_t len, int pattern_id);
int zone_verify_pattern(const void *buf, size_t len, int pattern_id);

/* IO workflows */
int zone_seq_write(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
		   struct zone_desc *zones, int zone_count,
		   uint32_t chunk_lba, int open_zones, int pattern_id,
		   struct io_stat *stat);

int zone_partial_write(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
		       struct zone_desc *zones, int zone_count,
		       uint32_t chunk_lba, int open_zones, int percent, int pattern_id,
		       struct io_stat *stat);

int zone_full_read_verify(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
			  struct zone_desc *zones, int zone_count,
			  uint32_t chunk_lba, int pattern_id,
			  struct io_stat *stat);

int zone_spot_read_verify(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
			  struct zone_desc *zones, int zone_count,
			  int pattern_id,
			  struct io_stat *stat);

/* Boundary guard test: should refuse to submit cross-boundary IOs; returns 0 if guard works. */
int zone_boundary_guard_test(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
			     struct zone_desc *zones, int zone_count,
			     uint32_t chunk_lba, int pattern_id);

