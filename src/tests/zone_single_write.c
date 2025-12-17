/* SPDX-License-Identifier: BSD-3-Clause */
#include "zone_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void usage(const char *prog)
{
	printf("Usage: %s [options] <device> <zone_size_lba> <zone_index> <chunk_lba>\n", prog);
	printf("Options:\n");
	printf("  -i <id>  shm id\n");
	printf("  -r <trid> NVMe transport (e.g., 0000:01:00.0)\n");
	printf("  -h       help\n");
	printf("\n");
	printf("This tool writes a single zone to full capacity.\n");
	printf("zone_index: the index of zone to write (0-based)\n");
	printf("lba = zone_index * zone_size_lba\n");
	printf("Data written: LBA address as data (no pattern)\n");
	printf("DPDK mem size: fixed at 2048MB (for up to 8 open zones, 200MB each)\n");
}

int main(int argc, char **argv)
{
	// Fixed DPDK mem size: 2048MB (for up to 8 open zones, 200MB each)
	struct env_opts eopts = { .mem_size = 2048, .shm_id = -1 };
	struct zone_opts zopts = { .open_zones = 1 };
	struct spdk_env_opts env;
	struct spdk_nvme_ctrlr *ctrlr = NULL;
	struct spdk_nvme_ns *ns = NULL;
	struct spdk_nvme_qpair *qpair = NULL;
	struct zone_desc *zones = NULL;
	int zone_count = 0;
	uint64_t zone_index = 0;
	uint64_t target_lba = 0;
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
	zone_index = strtoull(argv[optind + 2], NULL, 10);
	zopts.chunk_lba = strtoul(argv[optind + 3], NULL, 10);

	// Calculate target LBA: zone_index * zone_size_lba
	target_lba = zone_index * zopts.zone_size_lba;
	
	// Calculate total_lba to cover at least the target zone
	zopts.total_lba = target_lba + zopts.zone_size_lba;

	rc = zone_init_env(&eopts, &env, &ctrlr, &ns);
	if (rc != 0) goto out;

	rc = zone_build_table(&zones, &zone_count, zopts.total_lba, zopts.zone_size_lba, zopts.open_zones);
	if (rc != 0) goto out;

	// Find the target zone
	int target_zone_idx = -1;
	for (int i = 0; i < zone_count; i++) {
		if (zones[i].zslba == target_lba) {
			target_zone_idx = i;
			break;
		}
	}

	if (target_zone_idx == -1) {
		fprintf(stderr, "Zone with LBA %lu not found (zone_index=%lu, zone_size=%lu)\n",
			target_lba, zone_index, zopts.zone_size_lba);
		rc = -1;
		goto out;
	}

	qpair = zone_alloc_qpair(ns);
	if (!qpair) {
		rc = -1;
		goto out;
	}

	struct io_stat stat = {0};
	printf("zone_single_write: zone_index=%lu zone_size_lba=%lu target_lba=%lu chunk_lba=%u\n",
	       zone_index, zopts.zone_size_lba, target_lba, zopts.chunk_lba);
	printf("DPDK mem size: %d MB (fixed)\n", eopts.mem_size);
	printf("Data: LBA address as data\n");

	// Write only the target zone to full capacity
	// Pass pointer to the target zone in the array and count=1
	rc = zone_seq_write(ns, qpair, &zones[target_zone_idx], 1, zopts.chunk_lba, 1, &stat);
	if (rc != 0) {
		fprintf(stderr, "seq_write failed for zone_index=%lu\n", zone_index);
	} else {
		printf("Successfully wrote zone_index=%lu (LBA %lu) to full capacity\n", zone_index, target_lba);
	}

out:
	if (qpair) spdk_nvme_ctrlr_free_io_qpair(qpair);
	free(zones);
	zone_cleanup(ctrlr);
	return rc;
}
