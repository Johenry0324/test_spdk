/* SPDX-License-Identifier: BSD-3-Clause */
#include "zone_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct spdk_nvme_ctrlr *g_ctrlr = NULL;

static bool
probe_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
	 struct spdk_nvme_ctrlr_opts *opts)
{
	return true;
}

static void
attach_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
	 struct spdk_nvme_ctrlr *ctrlr, const struct spdk_nvme_ctrlr_opts *opts)
{
	g_ctrlr = ctrlr;
}

int
zone_init_env(struct env_opts *eopts, struct spdk_env_opts *env,
	      struct spdk_nvme_ctrlr **ctrlr, struct spdk_nvme_ns **ns)
{
	int rc;

	spdk_env_opts_init(env);
	if (eopts->mem_size > 0) {
		env->mem_size = eopts->mem_size;
	}
	if (eopts->shm_id >= 0) {
		env->shm_id = eopts->shm_id;
	}

	rc = spdk_env_init(env);
	if (rc < 0) {
		fprintf(stderr, "Unable to initialize SPDK env\n");
		return rc;
	}

	rc = spdk_nvme_probe(&eopts->trid, NULL, probe_cb, attach_cb, NULL);
	if (rc != 0) {
		fprintf(stderr, "spdk_nvme_probe() failed\n");
		return rc;
	}

	if (g_ctrlr == NULL) {
		fprintf(stderr, "No NVMe controller found\n");
		return -1;
	}

	*ctrlr = g_ctrlr;
	*ns = spdk_nvme_ctrlr_get_ns(g_ctrlr, spdk_nvme_ctrlr_get_first_active_ns(g_ctrlr));
	if (*ns == NULL) {
		fprintf(stderr, "No active namespace found\n");
		return -1;
	}

	return 0;
}

int
zone_build_table(struct zone_desc **zones, int *zone_count,
		 uint64_t total_lba, uint64_t zone_size_lba, int open_zones)
{
	uint64_t num_zones;
	int i;

	if (zone_size_lba == 0) {
		fprintf(stderr, "Invalid zone_size_lba: 0\n");
		return -1;
	}

	num_zones = total_lba / zone_size_lba;
	if (num_zones == 0) {
		fprintf(stderr, "No zones to build\n");
		return -1;
	}

	*zones = calloc(num_zones, sizeof(struct zone_desc));
	if (*zones == NULL) {
		fprintf(stderr, "Failed to allocate zone table\n");
		return -1;
	}

	for (i = 0; i < num_zones; i++) {
		(*zones)[i].zslba = i * zone_size_lba;
		(*zones)[i].zcap = zone_size_lba;
		(*zones)[i].wp = (*zones)[i].zslba;
		(*zones)[i].state = 0x1; // EMPTY state
	}

	*zone_count = num_zones;
	return 0;
}

struct spdk_nvme_qpair *
zone_alloc_qpair(struct spdk_nvme_ns *ns)
{
	struct spdk_nvme_io_qpair_opts opts;

	spdk_nvme_ctrlr_get_default_io_qpair_opts(spdk_nvme_ns_get_ctrlr(ns), &opts, sizeof(opts));
	return spdk_nvme_ctrlr_alloc_io_qpair(spdk_nvme_ns_get_ctrlr(ns), &opts, sizeof(opts));
}

static void
write_complete(void *arg, const struct spdk_nvme_cpl *completion)
{
	int *is_completed = arg;

	if (spdk_nvme_cpl_is_error(completion)) {
		fprintf(stderr, "Write I/O error: %s\n",
			spdk_nvme_cpl_get_status_string(&completion->status));
		*is_completed = 2;
		return;
	}

	*is_completed = 1;
}

int
zone_seq_write(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
		struct zone_desc *zones, int zone_count,
		uint32_t chunk_lba, int open_zones,
		struct io_stat *stat)
{
	uint32_t lba_size = spdk_nvme_ns_get_sector_size(ns);
	uint32_t chunk_size = chunk_lba * lba_size;
	void *buf = NULL;
	uint64_t *lba_buf = NULL;
	int i, j;
	int rc = 0;
	int is_completed = 0;

	if (stat) {
		stat->bytes_written = 0;
		stat->io_count = 0;
	}

	buf = spdk_zmalloc(chunk_size, lba_size, NULL, SPDK_ENV_NUMA_ID_ANY, SPDK_MALLOC_DMA);
	if (buf == NULL) {
		fprintf(stderr, "Failed to allocate write buffer\n");
		return -1;
	}

	// Cast buffer to uint64_t array for LBA address filling
	lba_buf = (uint64_t *)buf;

	// Write each zone to full capacity
	for (i = 0; i < zone_count; i++) {
		uint64_t current_lba = zones[i].zslba;
		uint64_t zone_end = zones[i].zslba + zones[i].zcap;
		uint64_t write_pointer = zones[i].zslba;  // Track write pointer

		// Write chunks until zone is full
		// Use zone append which writes from write pointer
		while (write_pointer < zone_end) {
			uint32_t nlb = chunk_lba;
			uint64_t remaining = zone_end - write_pointer;
			if (nlb > remaining) {
				nlb = remaining;
			}

			// Fill buffer with LBA addresses
			// For each LBA in the chunk, fill it with its LBA address
			uint32_t lba_words = lba_size / sizeof(uint64_t);  // Words per LBA
			uint32_t total_words = nlb * lba_words;  // Total words to fill
			for (uint32_t lba_idx = 0; lba_idx < nlb; lba_idx++) {
				uint64_t current_lba_addr = write_pointer + lba_idx;
				// Fill each LBA with its address value
				for (j = 0; j < lba_words; j++) {
					lba_buf[lba_idx * lba_words + j] = current_lba_addr;
				}
			}

			is_completed = 0;
			// Zone append: LBA is zone start, device writes from write pointer
			rc = spdk_nvme_zns_zone_append(ns, qpair, buf, zones[i].zslba,
						      nlb, write_complete, &is_completed, 0);
			if (rc != 0) {
				fprintf(stderr, "Zone append failed for zone %d\n", i);
				goto out;
			}

			// Poll for completion
			while (is_completed == 0) {
				spdk_nvme_qpair_process_completions(qpair, 0);
			}

			if (is_completed == 2) {
				fprintf(stderr, "Write failed for zone %d\n", i);
				rc = -1;
				goto out;
			}

			write_pointer += nlb;
			if (stat) {
				stat->bytes_written += nlb * lba_size;
				stat->io_count++;
			}
		}
	}

out:
	if (buf) {
		spdk_free(buf);
	}
	return rc;
}

void
zone_cleanup(struct spdk_nvme_ctrlr *ctrlr)
{
	if (ctrlr) {
		spdk_nvme_detach(ctrlr);
	}
	spdk_env_fini();
}
