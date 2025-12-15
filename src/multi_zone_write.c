/* SPDX-License-Identifier: BSD-3-Clause
 * Multi-zone sequential write tool (emulates zone-style writing on regular namespaces).
 *
 * Parameters:
 *   device         : PCIe address or transport string (same as spdk_nvme_probe -r)
 *   total_lba      : Total LBAs to write across all zones
 *   zone_size_lba  : LBAs per zone (emulated zone size)
 *   chunk_lba      : LBAs per write I/O
 *   -o <open_zones>: Number of concurrently active zones (default 8)
 *   -d <MB>        : DPDK hugepage size (optional)
 *   -i <shm_id>    : Shared memory group ID (optional)
 *
 * Example:
 *   multi_zone_write -r 0000:01:00.0 100000 16384 32 -o 8
 */

#include "spdk/stdinc.h"
#include "spdk/nvme.h"
#include "spdk/env.h"
#include "spdk/string.h"
#include "spdk/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct write_context;

struct write_io {
	void *buf;
	uint64_t lba;
	uint32_t lba_count;
	struct write_context *ctx;
	int zone_id;
};

struct zone_ctx {
	uint64_t start_lba;
	uint64_t current_lba;
	uint64_t end_lba; /* exclusive */
	int active;
};

struct write_context {
	struct spdk_nvme_ns *ns;
	struct spdk_nvme_qpair *qpair;

	struct zone_ctx *zones;
	int zone_count;       /* total zones */
	int open_zones;       /* number of simultaneously active zones */

	uint32_t chunk_lba;
	uint32_t max_outstanding;
	uint32_t outstanding_ios;

	int is_completed;
	int error_occurred;
};

static struct spdk_nvme_ctrlr *g_ctrlr = NULL;
static struct spdk_nvme_ns *g_ns = NULL;
static struct spdk_nvme_transport_id g_trid = {};

/* Forward declarations */
static int submit_one_zone_io(struct write_context *ctx, int zid);

static void
write_complete(void *arg, const struct spdk_nvme_cpl *completion)
{
	struct write_io *io = (struct write_io *)arg;
	struct write_context *ctx = io->ctx;
	int zid = io->zone_id;

	/* Free buffer */
	spdk_free(io->buf);

	if (spdk_nvme_cpl_is_error(completion)) {
		spdk_nvme_qpair_print_completion(ctx->qpair, (struct spdk_nvme_cpl *)completion);
		fprintf(stderr, "Write error at zone %d LBA %lu: %s\n",
			zid, io->lba, spdk_nvme_cpl_get_status_string(&completion->status));
		ctx->error_occurred = 1;
		ctx->is_completed = 1;
		free(io);
		return;
	}

	ctx->outstanding_ios--;

	/* If all zones finished and no outstanding IOs, mark complete */
	if (ctx->outstanding_ios == 0 && !ctx->error_occurred) {
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

	free(io);
}

static int
submit_one_zone_io(struct write_context *ctx, int zid)
{
	struct zone_ctx *z = &ctx->zones[zid];
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
		fprintf(stderr, "Failed to alloc write_io\n");
		return -1;
	}

	lba_count = ctx->chunk_lba;
	if (z->current_lba + lba_count > z->end_lba) {
		lba_count = z->end_lba - z->current_lba;
	}

	lba_size = spdk_nvme_ns_get_sector_size(ctx->ns);
	io->buf = spdk_zmalloc(lba_count * lba_size, 0x1000, NULL,
			       SPDK_ENV_NUMA_ID_ANY, SPDK_MALLOC_DMA);
	if (!io->buf) {
		fprintf(stderr, "Failed to alloc buffer\n");
		free(io);
		return -1;
	}

	memset(io->buf, 0xAB, lba_count * lba_size);

	io->lba = z->current_lba;
	io->lba_count = lba_count;
	io->ctx = ctx;
	io->zone_id = zid;

	rc = spdk_nvme_ns_cmd_write(ctx->ns, ctx->qpair, io->buf,
				    z->current_lba, lba_count,
				    write_complete, io, 0);
	if (rc != 0) {
		fprintf(stderr, "Submit failed zone %d LBA %lu rc=%d\n", zid, z->current_lba, rc);
		spdk_free(io->buf);
		free(io);
		return -1;
	}

	ctx->outstanding_ios++;
	z->current_lba += lba_count;
	return 1;
}

static int
multi_zone_write(struct spdk_nvme_ns *ns, uint64_t total_lba,
		 uint64_t zone_size_lba, uint32_t chunk_lba, int open_zones)
{
	struct write_context ctx = {0};
	uint64_t ns_sectors = spdk_nvme_ns_get_num_sectors(ns);
	uint64_t ns_sector_size = spdk_nvme_ns_get_sector_size(ns);
	uint64_t max_lba = total_lba;
	int rc = 0;

	if (max_lba > ns_sectors) {
		max_lba = ns_sectors;
	}

	ctx.ns = ns;
	ctx.chunk_lba = chunk_lba;
	ctx.max_outstanding = (uint32_t)(open_zones * 4); /* simple heuristic */
	ctx.outstanding_ios = 0;
	ctx.is_completed = 0;
	ctx.error_occurred = 0;
	ctx.open_zones = open_zones;

	/* Compute zone count */
	int zone_count = (int)((max_lba + zone_size_lba - 1) / zone_size_lba);
	ctx.zone_count = zone_count;
	ctx.zones = calloc(zone_count, sizeof(struct zone_ctx));
	if (!ctx.zones) {
		fprintf(stderr, "Failed to alloc zones\n");
		return -1;
	}

	for (int i = 0; i < zone_count; i++) {
		uint64_t start = (uint64_t)i * zone_size_lba;
		uint64_t end = start + zone_size_lba;
		if (start >= max_lba) {
			ctx.zones[i].active = 0;
			continue;
		}
		if (end > max_lba) {
			end = max_lba;
		}
		ctx.zones[i].start_lba = start;
		ctx.zones[i].current_lba = start;
		ctx.zones[i].end_lba = end;
		ctx.zones[i].active = (i < open_zones) ? 1 : 0;
	}

	/* Allocate qpair */
	ctx.qpair = spdk_nvme_ctrlr_alloc_io_qpair(spdk_nvme_ns_get_ctrlr(ns), NULL, 0);
	if (!ctx.qpair) {
		fprintf(stderr, "Failed to alloc qpair\n");
		free(ctx.zones);
		return -1;
	}

	printf("Multi-zone sequential write\n");
	printf("  Total LBAs      : %lu\n", max_lba);
	printf("  Zone size (LBA) : %lu\n", zone_size_lba);
	printf("  Zones count     : %d\n", zone_count);
	printf("  Open zones      : %d\n", open_zones);
	printf("  Chunk size (LBA): %u\n", chunk_lba);
	printf("  LBA size (bytes): %u\n", (uint32_t)ns_sector_size);

	/* Prime initial submissions */
	for (int i = 0; i < zone_count; i++) {
		if (!ctx.zones[i].active) {
			continue;
		}
		if (ctx.outstanding_ios >= ctx.max_outstanding) {
			break;
		}
		rc = submit_one_zone_io(&ctx, i);
		if (rc < 0) {
			goto out;
		}
	}

	/* Main loop */
	while (!ctx.is_completed && !ctx.error_occurred) {
		spdk_nvme_qpair_process_completions(ctx.qpair, 0);

		/* Round-robin submit */
		for (int i = 0; i < zone_count; i++) {
			if (!ctx.zones[i].active) {
				continue;
			}
			if (ctx.outstanding_ios >= ctx.max_outstanding) {
				break;
			}
			rc = submit_one_zone_io(&ctx, i);
			if (rc < 0) {
				goto out;
			}
			if (ctx.zones[i].active == 0 && ctx.outstanding_ios == 0) {
				/* If we just closed last active zone, mark complete */
				int done = 1;
				for (int j = 0; j < zone_count; j++) {
					if (ctx.zones[j].active) {
						done = 0;
						break;
					}
				}
				if (done) {
					ctx.is_completed = 1;
					break;
				}
			}
		}
	}

out:
	if (ctx.error_occurred) {
		rc = -1;
	}
	spdk_nvme_ctrlr_free_io_qpair(ctx.qpair);
	free(ctx.zones);
	return rc;
}

static bool
probe_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
	 struct spdk_nvme_ctrlr_opts *opts)
{
	if (g_trid.traddr[0] != '\0') {
		if (strcmp(trid->traddr, g_trid.traddr) != 0) {
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

	printf("Attached to %s\n", trid->traddr);
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
	printf("Using namespace ID: %d\n", nsid);
}

static void
usage(const char *prog)
{
	printf("Usage: %s [options] <device> <total_lba> <zone_size_lba> <chunk_lba>\n", prog);
	printf("Options:\n");
	printf("  -o <n>   Number of open zones (default 8)\n");
	printf("  -d <MB>  DPDK hugepage size in MB (optional)\n");
	printf("  -i <id>  Shared memory group ID (optional)\n");
	printf("  -r <trid> Transport address (e.g., PCIe address)\n");
	printf("  -h       Help\n");
}

static int
parse_args(int argc, char **argv, struct spdk_env_opts *env_opts,
	   char **device, uint64_t *total_lba,
	   uint64_t *zone_size_lba, uint32_t *chunk_lba,
	   int *open_zones)
{
	int op;

	spdk_nvme_trid_populate_transport(&g_trid, SPDK_NVME_TRANSPORT_PCIE);
	*open_zones = 8;

	while ((op = getopt(argc, argv, "d:i:o:r:h")) != -1) {
		switch (op) {
		case 'd':
			env_opts->mem_size = spdk_strtol(optarg, 10);
			if (env_opts->mem_size < 0) {
				fprintf(stderr, "Invalid mem size\n");
				return -1;
			}
			break;
		case 'i':
			env_opts->shm_id = spdk_strtol(optarg, 10);
			if (env_opts->shm_id < 0) {
				fprintf(stderr, "Invalid shm id\n");
				return -1;
			}
			break;
		case 'o':
			*open_zones = spdk_strtol(optarg, 10);
			if (*open_zones <= 0) {
				fprintf(stderr, "Invalid open zones\n");
				return -1;
			}
			break;
		case 'r':
			if (spdk_nvme_transport_id_parse(&g_trid, optarg) != 0) {
				fprintf(stderr, "Bad transport address\n");
				return -1;
			}
			break;
		case 'h':
		default:
			usage(argv[0]);
			return -1;
		}
	}

	if (optind + 4 > argc) {
		fprintf(stderr, "Missing required arguments\n");
		usage(argv[0]);
		return -1;
	}

	*device = argv[optind];
	char *endptr;
	*total_lba = strtoull(argv[optind + 1], &endptr, 10);
	if (*endptr != '\0' || *total_lba == 0) {
		fprintf(stderr, "Invalid total_lba\n");
		return -1;
	}
	*zone_size_lba = strtoull(argv[optind + 2], &endptr, 10);
	if (*endptr != '\0' || *zone_size_lba == 0) {
		fprintf(stderr, "Invalid zone_size_lba\n");
		return -1;
	}
	*chunk_lba = strtoul(argv[optind + 3], &endptr, 10);
	if (*endptr != '\0' || *chunk_lba == 0) {
		fprintf(stderr, "Invalid chunk_lba\n");
		return -1;
	}

	return 0;
}

int
main(int argc, char **argv)
{
	int rc;
	struct spdk_env_opts opts;
	char *device;
	uint64_t total_lba, zone_size_lba;
	uint32_t chunk_lba;
	int open_zones;

	opts.opts_size = sizeof(opts);
	spdk_env_opts_init(&opts);

	rc = parse_args(argc, argv, &opts, &device, &total_lba, &zone_size_lba, &chunk_lba, &open_zones);
	if (rc != 0) {
		return rc;
	}

	opts.name = "multi_zone_write";
	if (spdk_env_init(&opts) < 0) {
		fprintf(stderr, "Unable to init SPDK env\n");
		return 1;
	}

	printf("Initializing NVMe controllers...\n");
	rc = spdk_nvme_probe(&g_trid, NULL, probe_cb, attach_cb, NULL);
	if (rc != 0) {
		fprintf(stderr, "spdk_nvme_probe failed\n");
		rc = 1;
		goto exit;
	}
	if (!g_ctrlr || !g_ns) {
		fprintf(stderr, "No controller/namespace found\n");
		rc = 1;
		goto exit;
	}

	rc = multi_zone_write(g_ns, total_lba, zone_size_lba, chunk_lba, open_zones);
	if (rc != 0) {
		fprintf(stderr, "multi_zone_write failed\n");
		rc = 1;
	}

exit:
	if (g_ctrlr) {
		spdk_nvme_detach(g_ctrlr);
	}
	spdk_env_fini();
	return rc;
}

