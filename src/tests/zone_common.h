/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef ZONE_COMMON_H
#define ZONE_COMMON_H

#include "spdk/stdinc.h"
#include "spdk/nvme.h"
#include "spdk/nvme_zns.h"
#include "spdk/env.h"
#include "spdk/log.h"

struct env_opts {
	const char *device;
	int mem_size;
	int shm_id;
	struct spdk_nvme_transport_id trid;
};

struct zone_opts {
	uint64_t total_lba;
	uint64_t zone_size_lba;
	uint32_t chunk_lba;
	int open_zones;
	int pattern_id;
};

struct zone_desc {
	uint64_t zslba;  // Zone start LBA
	uint64_t zcap;   // Zone capacity
	uint64_t wp;     // Write pointer
	uint8_t state;   // Zone state
};

struct io_stat {
	uint64_t bytes_written;
	uint64_t io_count;
};

// Function declarations
int zone_init_env(struct env_opts *eopts, struct spdk_env_opts *env,
		  struct spdk_nvme_ctrlr **ctrlr, struct spdk_nvme_ns **ns);

int zone_build_table(struct zone_desc **zones, int *zone_count,
		     uint64_t total_lba, uint64_t zone_size_lba, int open_zones);

struct spdk_nvme_qpair *zone_alloc_qpair(struct spdk_nvme_ns *ns);

int zone_seq_write(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
		   struct zone_desc *zones, int zone_count,
		   uint32_t chunk_lba, int open_zones,
		   struct io_stat *stat);

void zone_cleanup(struct spdk_nvme_ctrlr *ctrlr);

#endif /* ZONE_COMMON_H */
