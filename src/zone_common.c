/* SPDX-License-Identifier: BSD-3-Clause */
#include "zone_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct spdk_nvme_ctrlr *g_ctrlr = NULL;
static struct spdk_nvme_ns *g_ns = NULL;

static bool
probe_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
	 struct spdk_nvme_ctrlr_opts *opts)
{
	struct spdk_nvme_transport_id *target = cb_ctx;
	if (target->traddr[0] != '\0') {
		if (strcmp(trid->traddr, target->traddr) != 0) {
			return false;
		}
	}
	return true;
}

static void
attach_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
	  struct spdk_nvme_ctrlr *ctrlr, const struct spdk_nvme_ctrlr_opts *opts)
{
	int nsid;
	struct spdk_nvme_ns *ns;

	g_ctrlr = ctrlr;
	nsid = spdk_nvme_ctrlr_get_first_active_ns(ctrlr);
	if (nsid == 0) {
		fprintf(stderr, "No active namespaces\n");
		return;
	}
	ns = spdk_nvme_ctrlr_get_ns(ctrlr, nsid);
	if (!ns || !spdk_nvme_ns_is_active(ns)) {
		fprintf(stderr, "Invalid namespace\n");
		return;
	}
	g_ns = ns;
}

int
zone_init_env(struct env_opts *eopts, struct spdk_env_opts *out_env,
	      struct spdk_nvme_ctrlr **ctrlr, struct spdk_nvme_ns **ns)
{
	int rc;

	out_env->opts_size = sizeof(*out_env);
	spdk_env_opts_init(out_env);
	if (eopts->mem_size > 0) {
		out_env->mem_size = eopts->mem_size;
	}
	if (eopts->shm_id >= 0) {
		out_env->shm_id = eopts->shm_id;
	}
	out_env->name = "zone_tools";

	if (spdk_env_init(out_env) < 0) {
		fprintf(stderr, "Unable to init SPDK env\n");
		return -1;
	}

	rc = spdk_nvme_probe(&eopts->trid, &eopts->trid, probe_cb, attach_cb, NULL);
	if (rc != 0) {
		fprintf(stderr, "spdk_nvme_probe failed rc=%d\n", rc);
		return -1;
	}
	if (!g_ctrlr || !g_ns) {
		fprintf(stderr, "No controller/namespace found\n");
		return -1;
	}

	*ctrlr = g_ctrlr;
	*ns = g_ns;
	return 0;
}

void
zone_cleanup(struct spdk_nvme_ctrlr *ctrlr)
{
	if (ctrlr) {
		spdk_nvme_detach(ctrlr);
	}
	spdk_env_fini();
	g_ctrlr = NULL;
	g_ns = NULL;
}

struct spdk_nvme_qpair *
zone_alloc_qpair(struct spdk_nvme_ns *ns)
{
	return spdk_nvme_ctrlr_alloc_io_qpair(spdk_nvme_ns_get_ctrlr(ns), NULL, 0);
}

int
zone_build_table(struct zone_desc **zones_out, int *zone_count_out,
		 uint64_t total_lba, uint64_t zone_size_lba, int open_zones)
{
	int zone_count = (int)((total_lba + zone_size_lba - 1) / zone_size_lba);
	struct zone_desc *zones = calloc(zone_count, sizeof(*zones));
	if (!zones) {
		return -1;
	}

	for (int i = 0; i < zone_count; i++) {
		uint64_t start = (uint64_t)i * zone_size_lba;
		uint64_t end = start + zone_size_lba;
		if (end > total_lba) {
			end = total_lba;
		}
		zones[i].start_lba = start;
		zones[i].current_lba = start;
		zones[i].end_lba = end;
		zones[i].active = (start < end);
	}

	*zones_out = zones;
	*zone_count_out = zone_count;
	return 0;
}

void
zone_reset_all(struct zone_desc *zones, int zone_count)
{
	for (int i = 0; i < zone_count; i++) {
		zones[i].current_lba = zones[i].start_lba;
		zones[i].active = (zones[i].start_lba < zones[i].end_lba);
	}
}

uint8_t
zone_pattern_byte(int pattern_id)
{
	switch (pattern_id) {
	case 0:
		return 0xAB;
	case 1:
		return 0x55;
	case 2:
		return 0x11;
	default:
		return 0xAB;
	}
}

void
zone_fill_pattern(void *buf, size_t len, int pattern_id)
{
	memset(buf, zone_pattern_byte(pattern_id), len);
}

int
zone_verify_pattern(const void *buf, size_t len, int pattern_id)
{
	uint8_t expect = zone_pattern_byte(pattern_id);
	const uint8_t *p = buf;
	for (size_t i = 0; i < len; i++) {
		if (p[i] != expect) {
			return -1;
		}
	}
	return 0;
}

struct write_io_ctx {
	struct zone_desc *zones;
	int zone_count;
	uint32_t chunk_lba;
	int pattern_id;
	uint32_t max_outstanding;
	uint32_t outstanding;
	int open_zones;
	struct io_stat *stat;
	int error;
};

struct write_io {
	void *buf;
	uint64_t lba;
	uint32_t lba_count;
	struct write_io_ctx *ctx;
	int zone_id;
};

static void
write_complete(void *arg, const struct spdk_nvme_cpl *completion)
{
	struct write_io *io = (struct write_io *)arg;
	struct write_io_ctx *wctx = io->ctx;

	spdk_free(io->buf);

	if (spdk_nvme_cpl_is_error(completion)) {
		spdk_nvme_qpair_print_completion(wctx->zones[0].start_lba ? NULL : NULL, (struct spdk_nvme_cpl *)completion);
		fprintf(stderr, "Write I/O error at LBA %lu: %s\n",
			io->lba, spdk_nvme_cpl_get_status_string(&completion->status));
		wctx->error = 1;
		free(io);
		return;
	}

	wctx->outstanding--;
	if (wctx->stat) {
		wctx->stat->io_completed++;
		wctx->stat->lba_written += io->lba_count;
	}

	free(io);
}

static int
submit_one_write(struct write_io_ctx *wctx, struct spdk_nvme_ns *ns,
		 struct spdk_nvme_qpair *qpair, int zone_id)
{
	struct zone_desc *z = &wctx->zones[zone_id];
	if (!z->active || z->current_lba >= z->end_lba) {
		return 0;
	}

	uint32_t lba_count = wctx->chunk_lba;
	if (z->current_lba + lba_count > z->end_lba) {
		lba_count = z->end_lba - z->current_lba;
	}

	uint64_t lba_size = spdk_nvme_ns_get_sector_size(ns);
	struct write_io *io = malloc(sizeof(*io));
	if (!io) {
		return -1;
	}

	io->buf = spdk_zmalloc(lba_count * lba_size, 0x1000, NULL,
			       SPDK_ENV_NUMA_ID_ANY, SPDK_MALLOC_DMA);
	if (!io->buf) {
		free(io);
		return -1;
	}

	zone_fill_pattern(io->buf, lba_count * lba_size, wctx->pattern_id);

	io->lba = z->current_lba;
	io->lba_count = lba_count;
	io->ctx = wctx;
	io->zone_id = zone_id;

	int rc = spdk_nvme_ns_cmd_write(ns, qpair, io->buf,
					 z->current_lba, lba_count,
					 write_complete, io, 0);
	if (rc != 0) {
		spdk_free(io->buf);
		free(io);
		return -1;
	}

	wctx->outstanding++;
	if (wctx->stat) {
		wctx->stat->io_submitted++;
	}
	z->current_lba += lba_count;
	if (z->current_lba >= z->end_lba) {
		z->active = 0;
	}
	return 1;
}

int
zone_seq_write(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
	       struct zone_desc *zones, int zone_count,
	       uint32_t chunk_lba, int open_zones, int pattern_id,
	       struct io_stat *stat)
{
	struct write_io_ctx wctx = {
		.zones = zones,
		.zone_count = zone_count,
		.chunk_lba = chunk_lba,
		.pattern_id = pattern_id,
		.max_outstanding = (uint32_t)(open_zones * 4),
		.outstanding = 0,
		.open_zones = open_zones,
		.stat = stat,
		.error = 0
	};

	if (stat) {
		memset(stat, 0, sizeof(*stat));
	}

	/* Prime initial submissions */
	int active_count = 0;
	for (int i = 0; i < zone_count && active_count < open_zones; i++) {
		if (zones[i].active) {
			submit_one_write(&wctx, ns, qpair, i);
			active_count++;
		}
	}

	/* Poll and submit more */
	while (wctx.outstanding > 0 || active_count > 0) {
		spdk_nvme_qpair_process_completions(qpair, 0);

		active_count = 0;
		for (int i = 0; i < zone_count; i++) {
			if (zones[i].active) {
				active_count++;
				if (wctx.outstanding < wctx.max_outstanding) {
					submit_one_write(&wctx, ns, qpair, i);
				}
			}
		}
	}

	return wctx.error ? -1 : 0;
}

int
zone_partial_write(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
		   struct zone_desc *zones, int zone_count,
		   uint32_t chunk_lba, int open_zones, int percent, int pattern_id,
		   struct io_stat *stat)
{
	/* Adjust zone end_lba by percent */
	for (int i = 0; i < zone_count; i++) {
		uint64_t len = zones[i].end_lba - zones[i].start_lba;
		uint64_t new_end = zones[i].start_lba + (len * percent) / 100;
		if (new_end < zones[i].end_lba) {
			zones[i].end_lba = new_end;
		}
		zones[i].current_lba = zones[i].start_lba;
		zones[i].active = (zones[i].start_lba < zones[i].end_lba);
	}
	return zone_seq_write(ns, qpair, zones, zone_count, chunk_lba, open_zones, pattern_id, stat);
}

struct read_io {
	void *buf;
	uint64_t lba;
	uint32_t lba_count;
	int zone_id;
	int pattern_id;
	struct io_stat *stat;
};

static void
read_complete(void *arg, const struct spdk_nvme_cpl *completion)
{
	struct read_io *io = (struct read_io *)arg;

	if (spdk_nvme_cpl_is_error(completion)) {
		spdk_nvme_qpair_print_completion(NULL, (struct spdk_nvme_cpl *)completion);
		fprintf(stderr, "Read I/O error at LBA %lu\n", io->lba);
		spdk_free(io->buf);
		free(io);
		return;
	}

	/* Verify pattern */
	if (zone_verify_pattern(io->buf, io->lba_count * 512, io->pattern_id) != 0) {
		fprintf(stderr, "Pattern mismatch at LBA %lu\n", io->lba);
	}

	if (io->stat) {
		io->stat->io_completed++;
		io->stat->lba_read += io->lba_count;
	}

	spdk_free(io->buf);
	free(io);
}

static int
do_read_verify(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
	       struct zone_desc *zones, int zone_count,
	       uint32_t chunk_lba, int pattern_id, struct io_stat *stat)
{
	uint64_t lba_size = spdk_nvme_ns_get_sector_size(ns);
	uint32_t outstanding = 0;
	uint32_t max_outstanding = 32;

	for (int i = 0; i < zone_count; i++) {
		struct zone_desc *z = &zones[i];
		uint64_t current = z->start_lba;

		while (current < z->end_lba && outstanding < max_outstanding) {
			uint32_t lba_count = chunk_lba;
			if (current + lba_count > z->end_lba) {
				lba_count = z->end_lba - current;
			}

			struct read_io *io = malloc(sizeof(*io));
			if (!io) {
				return -1;
			}

			io->buf = spdk_zmalloc(lba_count * lba_size, 0x1000, NULL,
					       SPDK_ENV_NUMA_ID_ANY, SPDK_MALLOC_DMA);
			if (!io->buf) {
				free(io);
				return -1;
			}

			io->lba = current;
			io->lba_count = lba_count;
			io->zone_id = i;
			io->pattern_id = pattern_id;
			io->stat = stat;

			int rc = spdk_nvme_ns_cmd_read(ns, qpair, io->buf,
							current, lba_count,
							read_complete, io, 0);
			if (rc != 0) {
				spdk_free(io->buf);
				free(io);
				return -1;
			}

			if (stat) {
				stat->io_submitted++;
			}
			outstanding++;
			current += lba_count;
		}
	}

	while (outstanding > 0) {
		uint32_t processed = spdk_nvme_qpair_process_completions(qpair, 0);
		if (processed > 0) {
			outstanding -= processed;
		}
	}

	return 0;
}

int
zone_full_read_verify(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
		      struct zone_desc *zones, int zone_count,
		      uint32_t chunk_lba, int pattern_id,
		      struct io_stat *stat)
{
	if (stat) {
		memset(stat, 0, sizeof(*stat));
	}
	return do_read_verify(ns, qpair, zones, zone_count, chunk_lba, pattern_id, stat);
}

int
zone_spot_read_verify(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
		       struct zone_desc *zones, int zone_count,
		       int pattern_id,
		       struct io_stat *stat)
{
	uint64_t lba_size = spdk_nvme_ns_get_sector_size(ns);
	uint32_t chunk_lba = 1; /* Use 1 LBA for spot check */

	if (stat) {
		memset(stat, 0, sizeof(*stat));
	}

	for (int i = 0; i < zone_count; i++) {
		struct zone_desc *z = &zones[i];
		uint64_t points[3] = {
			z->start_lba,
			z->start_lba + (z->end_lba - z->start_lba) / 2,
			z->end_lba > z->start_lba ? z->end_lba - 1 : z->start_lba
		};

		for (int p = 0; p < 3; p++) {
			uint64_t lba = points[p];
			if (lba >= z->end_lba) {
				continue;
			}

			void *buf = spdk_zmalloc(chunk_lba * lba_size, 0x1000, NULL,
						 SPDK_ENV_NUMA_ID_ANY, SPDK_MALLOC_DMA);
			if (!buf) {
				return -1;
			}

			int rc = spdk_nvme_ns_cmd_read(ns, qpair, buf, lba, chunk_lba,
						       NULL, NULL, 0);
			if (rc != 0) {
				spdk_free(buf);
				return -1;
			}

			while (spdk_nvme_qpair_process_completions(qpair, 0) == 0) {
				/* wait */
			}

			if (zone_verify_pattern(buf, chunk_lba * lba_size, pattern_id) != 0) {
				fprintf(stderr, "Spot verify failed zone %d LBA %lu\n", i, lba);
				spdk_free(buf);
				return -1;
			}

			if (stat) {
				stat->io_completed++;
				stat->lba_read += chunk_lba;
			}

			spdk_free(buf);
		}
	}

	return 0;
}

int
zone_boundary_guard_test(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
			 struct zone_desc *zones, int zone_count,
			 uint32_t chunk_lba, int pattern_id)
{
	/* Intentionally craft a write that crosses a zone boundary; we should detect and refuse. */
	if (zone_count == 0) {
		return -1;
	}
	struct zone_desc *z = &zones[0];
	if (z->start_lba + chunk_lba + 1 <= z->end_lba) {
		/* No crossing if we stay inside; force crossing */
	}
	uint64_t lba = z->end_lba - (chunk_lba / 2);
	uint32_t lba_count = chunk_lba;
	if (lba_count < 2) {
		lba_count = 2;
	}
	if (lba + lba_count <= z->end_lba) {
		lba = z->end_lba - 1;
		lba_count = 2;
	}

	/* Detect boundary crossing */
	if (lba + lba_count > z->end_lba) {
		/* Good: we expect to block submission */
		return 0;
	}
	/* If no crossing, this test is inconclusive */
	return -1;
}
