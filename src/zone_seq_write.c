/* SPDX-License-Identifier: BSD-3-Clause */
#include "zone_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void usage(const char *prog)
{
	printf("Usage: %s [options] <device> <total_lba> <zone_size_lba> <chunk_lba>\n", prog);
	printf("Options:\n");
	printf("  -o <n>   open zones (default 8)\n");
	printf("  -p <id>  pattern id (0:0xAB default, 1:0x55, 2:0x11)\n");
	printf("  -d <MB>  DPDK mem size\n");
	printf("  -i <id>  shm id\n");
	printf("  -r <trid> NVMe transport (e.g., 0000:01:00.0)\n");
	printf("  -h       help\n");
}

int main(int argc, char **argv)
{
	struct env_opts eopts = { .mem_size = -1, .shm_id = -1 };
	struct zone_opts zopts = { .open_zones = 8, .pattern_id = 0 };
	struct spdk_env_opts env;
	struct spdk_nvme_ctrlr *ctrlr = NULL;
	struct spdk_nvme_ns *ns = NULL;
	struct spdk_nvme_qpair *qpair = NULL;
	struct zone_desc *zones = NULL;
	int zone_count = 0;
	int op;
	int rc = 0;

	spdk_nvme_trid_populate_transport(&eopts.trid, SPDK_NVME_TRANSPORT_PCIE);

	while ((op = getopt(argc, argv, "o:p:d:i:r:h")) != -1) {
		switch (op) {
		case 'o':
			zopts.open_zones = spdk_strtol(optarg, 10);
			break;
		case 'p':
			zopts.pattern_id = spdk_strtol(optarg, 10);
			break;
		case 'd':
			eopts.mem_size = spdk_strtol(optarg, 10);
			break;
		case 'i':
			eopts.shm_id = spdk_strtol(optarg, 10);
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
	zopts.total_lba = strtoull(argv[optind + 1], NULL, 10);
	zopts.zone_size_lba = strtoull(argv[optind + 2], NULL, 10);
	zopts.chunk_lba = strtoul(argv[optind + 3], NULL, 10);

	rc = zone_init_env(&eopts, &env, &ctrlr, &ns);
	if (rc != 0) goto out;

	rc = zone_build_table(&zones, &zone_count, zopts.total_lba, zopts.zone_size_lba, zopts.open_zones);
	if (rc != 0) goto out;

	qpair = zone_alloc_qpair(ns);
	if (!qpair) {
		rc = -1;
		goto out;
	}

	struct io_stat stat = {0};
	printf("zone_seq_write: total_lba=%lu zone_size_lba=%lu chunk_lba=%u open_zones=%d pattern=%d\n",
	       zopts.total_lba, zopts.zone_size_lba, zopts.chunk_lba, zopts.open_zones, zopts.pattern_id);

	rc = zone_seq_write(ns, qpair, zones, zone_count, zopts.chunk_lba, zopts.open_zones, zopts.pattern_id, &stat);
	if (rc != 0) {
		fprintf(stderr, "seq_write failed\n");
	}

out:
	if (qpair) spdk_nvme_ctrlr_free_io_qpair(qpair);
	free(zones);
	zone_cleanup(ctrlr);
	return rc;
}

