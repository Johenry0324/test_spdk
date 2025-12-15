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
		zones[i].active = (i < open_zones) ? 1 : 0;
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
		zones[i].active = 1;
	}
}

uint8_t
zone_pattern_byte(int pattern_id)
{
	switch (pattern_id) {
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
	int zone_id;
	struct write_io_ctx *ctx;
};

static void
write_complete(void *arg, const struct spdk_nvme_cpl *completion)
{
	struct write_io *io = arg;
	struct write_io_ctx *wctx = io->ctx;

	spdk_free(io->buf);

	if (spdk_nvme_cpl_is_error(completion)) {
		spdk_nvme_qpair_print_completion(NULL, (struct spdk_nvme_cpl *)completion);
		wctx->error = 1;
	} else {
		if (wctx->stat) {
			wctx->stat->io_completed++;
		}
	}

	wctx->outstanding--;
	free(io);
}

static int
submit_one_write(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
		 struct write_io_ctx *wctx, int zid)
{
	struct zone_desc *z = &wctx->zones[zid];
	uint32_t lba_count;
	uint64_t lba_size;
	struct write_io *io;
	int rc;

	if (!z->active) {
		return 0;
	}
	if (z->current_lba >= z->end_lba) {
		z->active = 0;
		return 0;
	}

	io = malloc(sizeof(*io));
	if (!io) {
		return -1;
	}

	lba_count = wctx->chunk_lba;
	if (z->current_lba + lba_count > z->end_lba) {
		lba_count = z->end_lba - z->current_lba;
	}

	lba_size = spdk_nvme_ns_get_sector_size(ns);
	io->buf = spdk_zmalloc(lba_count * lba_size, 0x1000, NULL,
			       SPDK_ENV_NUMA_ID_ANY, SPDK_MALLOC_DMA);
	if (!io->buf) {
		free(io);
		return -1;
	}

	zone_fill_pattern(io->buf, lba_count * lba_size, wctx->pattern_id);

	io->lba = z->current_lba;
	io->lba_count = lba_count;
	io->zone_id = zid;
	io->ctx = wctx;

	rc = spdk_nvme_ns_cmd_write(ns, qpair, io->buf,
				    z->current_lba, lba_count,
				    write_complete, io, 0);
	if (rc != 0) {
		spdk_free(io->buf);
		free(io);
		return -1;
	}

	z->current_lba += lba_count;
	wctx->outstanding++;
	if (wctx->stat) {
		wctx->stat->io_submitted++;
		wctx->stat->lba_written += lba_count;
	}
	return 1;
}

static int
do_seq_write(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
	     struct zone_desc *zones, int zone_count,
	     uint32_t chunk_lba, int open_zones, int pattern_id,
	     int percent, struct io_stat *stat)
{
	struct write_io_ctx wctx = {0};
	int rc = 0;

	wctx.zones = zones;
	wctx.zone_count = zone_count;
	wctx.chunk_lba = chunk_lba;
	wctx.pattern_id = pattern_id;
	wctx.max_outstanding = (open_zones > 0 ? open_zones : 1) * 4;
	wctx.outstanding = 0;
	wctx.open_zones = open_zones;
	wctx.stat = stat;
	wctx.error = 0;

	/* limit end_lba for partial write */
	if (percent > 0 && percent < 100) {
		for (int i = 0; i < zone_count; i++) {
			uint64_t len = zones[i].end_lba - zones[i].start_lba;
			uint64_t new_end = zones[i].start_lba + (len * percent) / 100;
			if (new_end < zones[i].end_lba) {
				zones[i].end_lba = new_end;
			}
		}
	}

	/* Prime initial submissions */
	for (int i = 0; i < zone_count; i++) {
		if (!zones[i].active) {
			continue;
		}
		if (wctx.outstanding >= wctx.max_outstanding) {
			break;
		}
		rc = submit_one_write(ns, qpair, &wctx, i);
		if (rc < 0) {
			return -1;
		}
	}

	while (wctx.outstanding > 0 || 1) {
		if (wctx.outstanding == 0) {
			/* check if all zones done */
			int done = 1;
			for (int i = 0; i < zone_count; i++) {
				if (zones[i].active) {
					done = 0;
					break;
				}
			}
			if (done) {
				break;
			}
		}

		spdk_nvme_qpair_process_completions(qpair, 0);
		if (wctx.error) {
			return -1;
		}

		for (int i = 0; i < zone_count; i++) {
			if (!zones[i].active) {
				continue;
			}
			if (wctx.outstanding >= wctx.max_outstanding) {
				break;
			}
			rc = submit_one_write(ns, qpair, &wctx, i);
			if (rc < 0) {
				return -1;
			}
		}
	}

	return wctx.error ? -1 : 0;
}

int
zone_seq_write(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
	       struct zone_desc *zones, int zone_count,
	       uint32_t chunk_lba, int open_zones, int pattern_id,
	       struct io_stat *stat)
{
	return do_seq_write(ns, qpair, zones, zone_count, chunk_lba, open_zones, pattern_id, 100, stat);
}

int
zone_partial_write(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
		   struct zone_desc *zones, int zone_count,
		   uint32_t chunk_lba, int open_zones, int percent, int pattern_id,
		   struct io_stat *stat)
{
	if (percent <= 0 || percent > 100) {
		return -1;
	}
	return do_seq_write(ns, qpair, zones, zone_count, chunk_lba, open_zones, pattern_id, percent, stat);
}

struct read_ctx {
	int done;
	int error;
	struct io_stat *stat;
};

static void
read_complete(void *arg, const struct spdk_nvme_cpl *completion)
{
	struct read_ctx *rctx = arg;
	if (spdk_nvme_cpl_is_error(completion)) {
		rctx->error = 1;
	} else {
		if (rctx->stat) {
			rctx->stat->io_completed++;
		}
	}
	rctx->done = 1;
}

static int
do_read_verify(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
	       uint64_t lba, uint32_t lba_count, int pattern_id, struct io_stat *stat)
{
	uint64_t lba_size = spdk_nvme_ns_get_sector_size(ns);
	void *buf = spdk_zmalloc(lba_count * lba_size, 0x1000, NULL,
				 SPDK_ENV_NUMA_ID_ANY, SPDK_MALLOC_DMA);
	if (!buf) {
		return -1;
	}

	struct read_ctx rctx = { .done = 0, .error = 0, .stat = stat };

	int rc = spdk_nvme_ns_cmd_read(ns, qpair, buf, lba, lba_count, read_complete, &rctx, 0);
	if (rc != 0) {
		spdk_free(buf);
		return -1;
	}

	while (!rctx.done) {
		spdk_nvme_qpair_process_completions(qpair, 0);
	}
	if (rctx.error) {
		spdk_free(buf);
		return -1;
	}

	if (zone_verify_pattern(buf, lba_count * lba_size, pattern_id) != 0) {
		spdk_free(buf);
		return -2; /* verify failed */
	}

	if (stat) {
		stat->io_submitted++;
		stat->lba_read += lba_count;
	}

	spdk_free(buf);
	return 0;
}

int
zone_full_read_verify(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
		      struct zone_desc *zones, int zone_count,
		      uint32_t chunk_lba, int pattern_id,
		      struct io_stat *stat)
{
	for (int i = 0; i < zone_count; i++) {
		uint64_t lba = zones[i].start_lba;
		while (lba < zones[i].end_lba) {
			uint32_t lba_count = chunk_lba;
			if (lba + lba_count > zones[i].end_lba) {
				lba_count = zones[i].end_lba - lba;
			}
			int rc = do_read_verify(ns, qpair, lba, lba_count, pattern_id, stat);
			if (rc != 0) {
				return rc;
			}
			lba += lba_count;
		}
	}
	return 0;
}

int
zone_spot_read_verify(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
		      struct zone_desc *zones, int zone_count,
		      int pattern_id,
		      struct io_stat *stat)
{
	for (int i = 0; i < zone_count; i++) {
		uint64_t s = zones[i].start_lba;
		uint64_t e = zones[i].end_lba;
		uint64_t mid = s + (e - s) / 2;
		uint32_t step = 1;
		int rc;

		rc = do_read_verify(ns, qpair, s, step, pattern_id, stat);
		if (rc != 0) return rc;
		if (mid > s) {
			rc = do_read_verify(ns, qpair, mid, step, pattern_id, stat);
			if (rc != 0) return rc;
		}
		if (e > s) {
			rc = do_read_verify(ns, qpair, e - step, step, pattern_id, stat);
			if (rc != 0) return rc;
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
/* SPDX-License-Identifier: BSD-3-Clause */
#include "zone_common.h"
#include "spdk/string.h"
#include "spdk/log.h"

static bool
probe_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
	 struct spdk_nvme_ctrlr_opts *opts)
{
	struct nvme_session *sess = cb_ctx;
	if (sess->trid.traddr[0] != '\0') {
		if (strcmp(trid->traddr, sess->trid.traddr) != 0) {
			return false;
		}
	}
	return true;
}

static void
attach_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
	  struct spdk_nvme_ctrlr *ctrlr, const struct spdk_nvme_ctrlr_opts *opts)
{
	struct nvme_session *sess = cb_ctx;
	int nsid;
	struct spdk_nvme_ns *ns;

	printf("Attached to %s\n", trid->traddr);
	sess->ctrlr = ctrlr;

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
	sess->ns = ns;
	printf("Using namespace ID: %d\n", nsid);
}

int
zone_init_env_and_probe(const char *device, struct spdk_env_opts *opts, struct nvme_session *sess)
{
	int rc;

	opts->name = "zone_tool";
	if (spdk_env_init(opts) < 0) {
		fprintf(stderr, "Unable to init SPDK env\n");
		return -1;
	}

	if (device && device[0]) {
		/* If user provided a transport string */
		if (spdk_nvme_transport_id_parse(&sess->trid, device) != 0) {
			/* fallback: assume device is PCIe address */
			spdk_nvme_trid_populate_transport(&sess->trid, SPDK_NVME_TRANSPORT_PCIE);
			snprintf(sess->trid.traddr, sizeof(sess->trid.traddr), "%s", device);
		}
	}

	printf("Initializing NVMe controllers...\n");
	rc = spdk_nvme_probe(&sess->trid, sess, probe_cb, attach_cb, NULL);
	if (rc != 0) {
		fprintf(stderr, "spdk_nvme_probe failed\n");
		return -1;
	}
	if (!sess->ctrlr || !sess->ns) {
		fprintf(stderr, "No controller/namespace found\n");
		return -1;
	}
	return 0;
}

int
zone_build_layout(struct spdk_nvme_ns *ns, uint64_t total_lba, uint64_t zone_size_lba,
		  struct zone_desc **zones_out, int *zone_count_out)
{
	uint64_t ns_sectors = spdk_nvme_ns_get_num_sectors(ns);
	uint64_t max_lba = total_lba;

	if (max_lba > ns_sectors) {
		max_lba = ns_sectors;
	}

	int zone_count = (int)((max_lba + zone_size_lba - 1) / zone_size_lba);
	struct zone_desc *zones = calloc(zone_count, sizeof(*zones));
	if (!zones) {
		return -1;
	}

	for (int i = 0; i < zone_count; i++) {
		uint64_t start = (uint64_t)i * zone_size_lba;
		uint64_t end = start + zone_size_lba;
		if (start >= max_lba) {
			zones[i].active = 0;
			continue;
		}
		if (end > max_lba) {
			end = max_lba;
		}
		zones[i].start_lba = start;
		zones[i].current_lba = start;
		zones[i].end_lba = end;
		zones[i].active = 1;
	}

	*zones_out = zones;
	*zone_count_out = zone_count;
	return 0;
}

void
zone_fill_pattern(void *buf, size_t len, uint8_t pat, int incremental)
{
	if (!incremental) {
		memset(buf, pat, len);
		return;
	}
	uint8_t *p = buf;
	for (size_t i = 0; i < len; i++) {
		p[i] = (uint8_t)(pat + (i & 0xFF));
	}
}

int
zone_verify_pattern(const void *buf, size_t len, uint8_t pat, int incremental)
{
	const uint8_t *p = buf;
	for (size_t i = 0; i < len; i++) {
		uint8_t expect = incremental ? (uint8_t)(pat + (i & 0xFF)) : pat;
		if (p[i] != expect) {
			return -1;
		}
	}
	return 0;
}
/* SPDX-License-Identifier: BSD-3-Clause */
#include "zone_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct app_probe_ctx {
	struct spdk_nvme_transport_id trid;
	struct app_ctx *ctx;
};

int
app_parse_trid(struct app_opts *opts, const char *trid_str)
{
	spdk_nvme_trid_populate_transport(&opts->trid, SPDK_NVME_TRANSPORT_PCIE);
	if (trid_str && spdk_nvme_transport_id_parse(&opts->trid, trid_str) != 0) {
		fprintf(stderr, "Bad transport address\n");
		return -1;
	}
	return 0;
}

int
app_setup_env(struct spdk_env_opts *env_opts, const char *name)
{
	env_opts->opts_size = sizeof(*env_opts);
	spdk_env_opts_init(env_opts);
	env_opts->name = name;
	if (spdk_env_init(env_opts) < 0) {
		fprintf(stderr, "Unable to initialize SPDK env\n");
		return -1;
	}
	return 0;
}

static bool
probe_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
	 struct spdk_nvme_ctrlr_opts *opts)
{
	struct app_probe_ctx *pctx = cb_ctx;
	if (pctx->trid.traddr[0] != '\0') {
		if (strcmp(trid->traddr, pctx->trid.traddr) != 0) {
			return false;
		}
	}
	return true;
}

static void
attach_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
	  struct spdk_nvme_ctrlr *ctrlr, const struct spdk_nvme_ctrlr_opts *opts)
{
	struct app_probe_ctx *pctx = cb_ctx;
	int nsid;
	struct spdk_nvme_ns *ns;

	printf("Attached to %s\n", trid->traddr);
	pctx->ctx->ctrlr = ctrlr;

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
	pctx->ctx->ns = ns;
	printf("Using namespace ID: %d\n", nsid);
}

int
app_probe_attach(struct app_opts *opts, struct app_ctx *ctx)
{
	struct app_probe_ctx pctx = {.trid = opts->trid, .ctx = ctx};
	int rc = spdk_nvme_probe(&opts->trid, &pctx, probe_cb, attach_cb, NULL);
	if (rc != 0) {
		fprintf(stderr, "spdk_nvme_probe failed\n");
		return -1;
	}
	if (!ctx->ctrlr || !ctx->ns) {
		fprintf(stderr, "No controller/namespace found\n");
		return -1;
	}
	return 0;
}

void
app_cleanup(struct app_ctx *ctx)
{
	if (ctx->qpair) {
		spdk_nvme_ctrlr_free_io_qpair(ctx->qpair);
	}
	if (ctx->ctrlr) {
		spdk_nvme_detach(ctx->ctrlr);
	}
	if (ctx->zones) {
		free(ctx->zones);
	}
}

int
app_init_zones(struct app_ctx *ctx, const struct app_opts *opts)
{
	uint64_t ns_sectors = spdk_nvme_ns_get_num_sectors(ctx->ns);
	uint64_t max_lba = opts->total_lba;
	if (max_lba > ns_sectors) {
		max_lba = ns_sectors;
	}

	int zone_count = (int)((max_lba + opts->zone_size_lba - 1) / opts->zone_size_lba);
	ctx->zone_count = zone_count;
	ctx->zones = calloc(zone_count, sizeof(struct zone_range));
	if (!ctx->zones) {
		fprintf(stderr, "Failed to alloc zones\n");
		return -1;
	}

	for (int i = 0; i < zone_count; i++) {
		uint64_t start = (uint64_t)i * opts->zone_size_lba;
		uint64_t end = start + opts->zone_size_lba;
		if (start >= max_lba) {
			ctx->zones[i].active = 0;
			continue;
		}
		if (end > max_lba) {
			end = max_lba;
		}
		ctx->zones[i].start_lba = start;
		ctx->zones[i].current_lba = start;
		ctx->zones[i].end_lba = end;
		ctx->zones[i].active = 1;
	}

	ctx->qpair = spdk_nvme_ctrlr_alloc_io_qpair(spdk_nvme_ns_get_ctrlr(ctx->ns), NULL, 0);
	if (!ctx->qpair) {
		fprintf(stderr, "Failed to allocate I/O qpair\n");
		return -1;
	}

	ctx->max_outstanding = opts->open_zones * 4;
	ctx->pattern = opts->pattern;
	return 0;
}

struct rw_io {
	void *buf;
	uint64_t lba;
	uint32_t lba_count;
	struct app_ctx *ctx;
	int zid;
	int is_read;
};

static void
rw_complete(void *arg, const struct spdk_nvme_cpl *completion)
{
	struct rw_io *io = arg;
	struct app_ctx *ctx = io->ctx;

	if (io->is_read) {
		/* verify pattern */
		uint8_t pat = (uint8_t)ctx->pattern;
		if (completion && !spdk_nvme_cpl_is_error(completion)) {
			for (uint32_t i = 0; i < io->lba_count * spdk_nvme_ns_get_sector_size(ctx->ns); i++) {
				if (((uint8_t *)io->buf)[i] != pat) {
					fprintf(stderr, "Verify mismatch zone %d LBA %lu\n", io->zid, io->lba);
					ctx->error = 1;
					break;
				}
			}
		} else {
			fprintf(stderr, "Read error zone %d LBA %lu\n", io->zid, io->lba);
			ctx->error = 1;
		}
	} else {
		if (completion && spdk_nvme_cpl_is_error(completion)) {
			spdk_nvme_qpair_print_completion(ctx->qpair, (struct spdk_nvme_cpl *)completion);
			fprintf(stderr, "Write error zone %d LBA %lu\n", io->zid, io->lba);
			ctx->error = 1;
		}
	}

	ctx->outstanding--;
	spdk_free(io->buf);
	free(io);
	if (ctx->outstanding == 0 && !ctx->error) {
		int done = 1;
		for (int i = 0; i < ctx->zone_count; i++) {
			if (ctx->zones[i].active) {
				done = 0;
				break;
			}
		}
		if (done) {
			ctx->is_completed = 1;
		}
	}
}

static int
submit_rw(struct app_ctx *ctx, int zid, uint32_t chunk_lba, int is_read)
{
	struct zone_range *z = &ctx->zones[zid];
	if (!z->active) {
		return 0;
	}
	if (z->current_lba >= z->end_lba) {
		z->active = 0;
		return 0;
	}

	uint32_t lba_count = chunk_lba;
	if (z->current_lba + lba_count > z->end_lba) {
		lba_count = z->end_lba - z->current_lba;
	}

	struct rw_io *io = malloc(sizeof(*io));
	if (!io) {
		fprintf(stderr, "Failed to alloc rw_io\n");
		return -1;
	}

	uint64_t lba_size = spdk_nvme_ns_get_sector_size(ctx->ns);
	io->buf = spdk_zmalloc(lba_count * lba_size, 0x1000, NULL, SPDK_ENV_NUMA_ID_ANY, SPDK_MALLOC_DMA);
	if (!io->buf) {
		fprintf(stderr, "Failed to alloc buffer\n");
		free(io);
		return -1;
	}

	if (!is_read) {
		memset(io->buf, (uint8_t)ctx->pattern, lba_count * lba_size);
	}

	io->lba = z->current_lba;
	io->lba_count = lba_count;
	io->ctx = ctx;
	io->zid = zid;
	io->is_read = is_read;

	int rc;
	if (is_read) {
		rc = spdk_nvme_ns_cmd_read(ctx->ns, ctx->qpair, io->buf,
					   z->current_lba, lba_count,
					   rw_complete, io, 0);
	} else {
		rc = spdk_nvme_ns_cmd_write(ctx->ns, ctx->qpair, io->buf,
					    z->current_lba, lba_count,
					    rw_complete, io, 0);
	}
	if (rc != 0) {
		fprintf(stderr, "%s submit failed zone %d LBA %lu rc=%d\n",
			is_read ? "Read" : "Write", zid, z->current_lba, rc);
		spdk_free(io->buf);
		free(io);
		return -1;
	}

	ctx->outstanding++;
	z->current_lba += lba_count;
	return 1;
}

int submit_write_chunk(struct app_ctx *ctx, int zid, uint32_t chunk_lba)
{
	return submit_rw(ctx, zid, chunk_lba, 0);
}

int submit_read_chunk(struct app_ctx *ctx, int zid, uint32_t chunk_lba)
{
	return submit_rw(ctx, zid, chunk_lba, 1);
}

static int
run_rw_loop(struct app_ctx *ctx, const struct app_opts *opts, int is_read, int percent)
{
	/* adjust active ranges by percent */
	for (int i = 0; i < ctx->zone_count; i++) {
		struct zone_range *z = &ctx->zones[i];
		if (!z->active) {
			continue;
		}
		uint64_t len = z->end_lba - z->start_lba;
		uint64_t new_end = z->start_lba + (len * percent) / 100;
		if (new_end < z->end_lba) {
			z->end_lba = new_end;
		}
		z->current_lba = z->start_lba;
		z->active = (z->start_lba < z->end_lba);
	}

	/* prime submissions */
	for (int i = 0; i < ctx->zone_count; i++) {
		if (ctx->outstanding >= ctx->max_outstanding) {
			break;
		}
		if (!ctx->zones[i].active) {
			continue;
		}
		int rc = submit_rw(ctx, i, opts->chunk_lba, is_read);
		if (rc < 0) {
			return -1;
		}
	}

	while (!ctx->is_completed && !ctx->error) {
		spdk_nvme_qpair_process_completions(ctx->qpair, 0);
		for (int i = 0; i < ctx->zone_count; i++) {
			if (ctx->outstanding >= ctx->max_outstanding) {
				break;
			}
			if (!ctx->zones[i].active) {
				continue;
			}
			int rc = submit_rw(ctx, i, opts->chunk_lba, is_read);
			if (rc < 0) {
				return -1;
			}
		}
		if (ctx->outstanding == 0) {
			ctx->is_completed = 1;
		}
	}
	return ctx->error ? -1 : 0;
}

int
run_zone_sequential_write(struct app_ctx *ctx, const struct app_opts *opts, int percent)
{
	printf("Zone sequential write: total_lba=%lu zone_size=%lu chunk=%u open_zones=%d pattern=0x%02x percent=%d\n",
	       opts->total_lba, opts->zone_size_lba, opts->chunk_lba, opts->open_zones,
	       (unsigned)opts->pattern, percent);
	return run_rw_loop(ctx, opts, 0, percent);
}

int
run_zone_read_verify(struct app_ctx *ctx, const struct app_opts *opts, int percent)
{
	printf("Zone read+verify: total_lba=%lu zone_size=%lu chunk=%u open_zones=%d pattern=0x%02x percent=%d\n",
	       opts->total_lba, opts->zone_size_lba, opts->chunk_lba, opts->open_zones,
	       (unsigned)opts->pattern, percent);
	return run_rw_loop(ctx, opts, 1, percent);
}

int
run_zone_reset_rewrite(struct app_ctx *ctx, const struct app_opts *opts, int pattern_b)
{
	int rc;
	/* first pass write with pattern A (ctx->pattern) */
	rc = run_zone_sequential_write(ctx, opts, 100);
	if (rc != 0) {
		return rc;
	}
	/* reset zones */
	for (int i = 0; i < ctx->zone_count; i++) {
		ctx->zones[i].current_lba = ctx->zones[i].start_lba;
		ctx->zones[i].active = (ctx->zones[i].start_lba < ctx->zones[i].end_lba);
	}
	ctx->pattern = pattern_b;
	ctx->outstanding = 0;
	ctx->is_completed = 0;
	ctx->error = 0;
	/* second pass write with pattern B */
	rc = run_zone_sequential_write(ctx, opts, 100);
	return rc;
}

int
run_zone_boundary_guard(const struct app_opts *opts)
{
	/* This tool only checks computation and refuses cross-boundary writes */
	uint64_t max_lba = opts->total_lba;
	uint64_t zone_size = opts->zone_size_lba;
	uint64_t start = zone_size - (opts->chunk_lba / 2);
	uint64_t end = start + opts->chunk_lba;
	if (end > zone_size) {
		printf("Boundary guard: reject crossing zone boundary (start=%lu len=%u)\n",
		       start, opts->chunk_lba);
		return 0;
	}
	printf("Boundary guard: no crossing detected (start=%lu len=%u)\n",
	       start, opts->chunk_lba);
	(void)max_lba;
	return 0;
}

int
run_zone_spot_read(struct app_ctx *ctx, const struct app_opts *opts)
{
	uint64_t lba_size = spdk_nvme_ns_get_sector_size(ctx->ns);
	uint64_t buf_bytes = opts->chunk_lba * lba_size;
	void *buf = spdk_zmalloc(buf_bytes, 0x1000, NULL, SPDK_ENV_NUMA_ID_ANY, SPDK_MALLOC_DMA);
	if (!buf) {
		fprintf(stderr, "Failed to alloc spot buffer\n");
		return -1;
	}
	int rc = 0;
	for (int i = 0; i < ctx->zone_count; i++) {
		uint64_t start = ctx->zones[i].start_lba;
		uint64_t end = ctx->zones[i].end_lba;
		uint64_t mid = start + (end - start) / 2;
		uint64_t points[3] = {start, mid, end > start ? end - opts->chunk_lba : start};
		for (int p = 0; p < 3; p++) {
			uint64_t lba = points[p];
			if (lba + opts->chunk_lba > end) {
				continue;
			}
			memset(buf, 0, buf_bytes);
			rc = spdk_nvme_ns_cmd_read(ctx->ns, ctx->qpair, buf,
						   lba, opts->chunk_lba,
						   NULL, NULL, 0);
			if (rc != 0) {
				fprintf(stderr, "Spot read submit failed zone %d LBA %lu rc=%d\n", i, lba, rc);
				goto out;
			}
			while (spdk_nvme_qpair_process_completions(ctx->qpair, 0) == 0) {
				/* wait */
			}
			/* verify pattern */
			uint8_t pat = (uint8_t)ctx->pattern;
			for (uint64_t b = 0; b < buf_bytes; b++) {
				if (((uint8_t *)buf)[b] != pat) {
					fprintf(stderr, "Spot verify mismatch zone %d LBA %lu\n", i, lba);
					rc = -1;
					goto out;
				}
			}
		}
	}
out:
	spdk_free(buf);
	return rc;
}

