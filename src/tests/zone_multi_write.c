/* SPDX-License-Identifier: BSD-3-Clause */
#include "zone_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define ZONE_MULTI_OPEN_ZONES 8
#define ZONE_MULTI_MEM_MB 2048

static void usage(const char *prog)
{
	printf("Usage: %s [options] <device> <zone_size_lba> <base_zone_index> <chunk_lba>\n", prog);
	printf("Options:\n");
	printf("  -i <id>  shm id\n");
	printf("  -r <trid> NVMe transport (e.g., 0000:01:00.0)\n");
	printf("  -h       help\n");
	printf("\n");
	printf("This tool writes %d zones concurrently starting from base_zone_index.\n", ZONE_MULTI_OPEN_ZONES);
	printf("zone_index = base_zone_index .. base_zone_index + %d - 1 (0-based)\n", ZONE_MULTI_OPEN_ZONES);
	printf("lba = zone_index * zone_size_lba\n");
	printf("Data written: LBA address as data (no pattern)\n");
	printf("DPDK mem size: fixed at %dMB (for up to %d open zones, 200MB each)\n",
	       ZONE_MULTI_MEM_MB, ZONE_MULTI_OPEN_ZONES);
}

struct zone_thread_ctx {
	struct spdk_nvme_ns *ns;
	struct zone_desc *zone;
	uint32_t chunk_lba;
	struct io_stat stat;
	int rc;
};

static void *
zone_thread_fn(void *arg)
{
	struct zone_thread_ctx *ctx = arg;
	struct spdk_nvme_qpair *qpair = NULL;

	qpair = zone_alloc_qpair(ctx->ns);
	if (!qpair) {
		fprintf(stderr, "Failed to alloc qpair for zone at LBA %lu\n", ctx->zone->zslba);
		ctx->rc = -1;
		return NULL;
	}

	ctx->stat.bytes_written = 0;
	ctx->stat.io_count = 0;

	ctx->rc = zone_seq_write(ctx->ns, qpair, ctx->zone, 1,
				    ctx->chunk_lba, 1, &ctx->stat);

	if (ctx->rc != 0) {
		fprintf(stderr, "zone_seq_write failed for zone at LBA %lu\n", ctx->zone->zslba);
	}

	spdk_nvme_ctrlr_free_io_qpair(qpair);
	return NULL;
}

int main(int argc, char **argv)
{
	struct env_opts eopts = { .mem_size = ZONE_MULTI_MEM_MB, .shm_id = -1 };
	struct zone_opts zopts = { .open_zones = ZONE_MULTI_OPEN_ZONES };
	struct spdk_env_opts env;
	struct spdk_nvme_ctrlr *ctrlr = NULL;
	struct spdk_nvme_ns *ns = NULL;
	struct zone_desc *zones = NULL;
	int zone_count = 0;
	uint64_t base_zone_index = 0;
	uint64_t target_lba_first = 0;
	int op;
	int rc = 0;

	spdk_nvme_trid_populate_transport(&eopts.trid, SPDK_NVME_TRANSPORT_PCIE);

	while ((op = getopt(argc, argv, "i:r:h")) != -1) {
		switch (op) {
		case 'i':
			eopts.shm_id = (int)strtol(optarg, NULL, 10);
			break;
		case 'r':
			if (spdk_nvme_transport_id_parse(&eopts.trid, optarg) != 0) {
				fprintf(stderr, "Bad transport address\n");
				return -1;
			}
			break;
		case 'h':
		default:
			usage(argv[0]);
			return 0;
		}
	}

	if (optind + 4 > argc) {
		usage(argv[0]);
		return -1;
	}

	eopts.device = argv[optind];
	zopts.zone_size_lba = strtoull(argv[optind + 1], NULL, 10);
	base_zone_index = strtoull(argv[optind + 2], NULL, 10);
	zopts.chunk_lba = strtoul(argv[optind + 3], NULL, 10);

	// Calculate first target LBA
	target_lba_first = base_zone_index * zopts.zone_size_lba;

	// total_lba must cover all target zones
	zopts.total_lba = (base_zone_index + ZONE_MULTI_OPEN_ZONES) * zopts.zone_size_lba;

	rc = zone_init_env(&eopts, &env, &ctrlr, &ns);
	if (rc != 0) goto out;

	rc = zone_build_table(&zones, &zone_count, zopts.total_lba,
			      zopts.zone_size_lba, zopts.open_zones);
	if (rc != 0) goto out;

	if (zone_count < base_zone_index + ZONE_MULTI_OPEN_ZONES) {
		fprintf(stderr, "Not enough zones: have %d, need at least %lu\n",
			zone_count, base_zone_index + (uint64_t)ZONE_MULTI_OPEN_ZONES);
		rc = -1;
		goto out;
	}

	printf("zone_multi_write: zone_size_lba=%lu base_zone_index=%lu chunk_lba=%u open_zones=%d\n",
	       zopts.zone_size_lba, base_zone_index, zopts.chunk_lba, zopts.open_zones);
	printf("DPDK mem size: %d MB (fixed)\n", eopts.mem_size);
	printf("Data: LBA address as data\n");

	// Create threads for each zone
	pthread_t threads[ZONE_MULTI_OPEN_ZONES];
	struct zone_thread_ctx ctx[ZONE_MULTI_OPEN_ZONES];

	for (int i = 0; i < ZONE_MULTI_OPEN_ZONES; i++) {
		uint64_t zone_lba = (base_zone_index + i) * zopts.zone_size_lba;
		struct zone_desc *target_zone = NULL;

		for (int z = 0; z < zone_count; z++) {
			if (zones[z].zslba == zone_lba) {
				target_zone = &zones[z];
				break;
			}
		}

		if (!target_zone) {
			fprintf(stderr, "Zone with LBA %lu not found (index offset=%d)\n",
				zone_lba, i);
			rc = -1;
			goto join_threads;
		}

		ctx[i].ns = ns;
		ctx[i].zone = target_zone;
		ctx[i].chunk_lba = zopts.chunk_lba;
		ctx[i].stat.bytes_written = 0;
		ctx[i].stat.io_count = 0;
		ctx[i].rc = 0;

		rc = pthread_create(&threads[i], NULL, zone_thread_fn, &ctx[i]);
		if (rc != 0) {
			fprintf(stderr, "Failed to create thread for zone LBA %lu (rc=%d)\n",
				zone_lba, rc);
			goto join_threads;
		}
	}

join_threads:
	for (int i = 0; i < ZONE_MULTI_OPEN_ZONES; i++) {
		pthread_join(threads[i], NULL);
	}

	if (rc == 0) {
		for (int i = 0; i < ZONE_MULTI_OPEN_ZONES; i++) {
			printf("zone[%d] zslba=%lu bytes_written=%lu io_count=%lu rc=%d\n",
			       i,
			       ctx[i].zone ? ctx[i].zone->zslba : 0,
			       ctx[i].stat.bytes_written,
			       ctx[i].stat.io_count,
			       ctx[i].rc);
		}
	}

out:
	free(zones);
	zone_cleanup(ctrlr);
	return rc;
}
