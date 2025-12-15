/* SPDX-License-Identifier: BSD-3-Clause
 * Zone-like deallocate/trim tool.
 *
 * Parameters:
 *   device         : PCIe address or transport string (-r)
 *   total_lba      : Total LBAs to deallocate
 *   zone_size_lba  : LBAs per emulated zone
 *   -d <MB>        : DPDK hugepage size (optional)
 *   -i <shm_id>    : Shared memory group ID (optional)
 *   -r <trid>      : Transport address (e.g., 0000:01:00.0)
 *
 * Behavior:
 *   Splits [0, total_lba) into zones of zone_size_lba (rounded up),
 *   and issues NVMe Dataset Management Deallocate for each zone.
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

static struct spdk_nvme_ctrlr *g_ctrlr = NULL;
static struct spdk_nvme_ns *g_ns = NULL;
static struct spdk_nvme_transport_id g_trid = {};

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
	printf("Usage: %s [options] <device> <total_lba> <zone_size_lba>\n", prog);
	printf("Options:\n");
	printf("  -d <MB>  DPDK hugepage size\n");
	printf("  -i <id>  Shared memory group ID\n");
	printf("  -r <trid> NVMe transport address (e.g., 0000:01:00.0)\n");
	printf("  -h       Help\n");
}

static int
parse_args(int argc, char **argv, struct spdk_env_opts *env_opts,
	   char **device, uint64_t *total_lba, uint64_t *zone_size_lba)
{
	int op;

	spdk_nvme_trid_populate_transport(&g_trid, SPDK_NVME_TRANSPORT_PCIE);

	while ((op = getopt(argc, argv, "d:i:r:h")) != -1) {
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

	if (optind + 3 > argc) {
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

	return 0;
}

static int
do_deallocate(struct spdk_nvme_ns *ns, uint64_t total_lba, uint64_t zone_size_lba)
{
	uint64_t ns_sectors = spdk_nvme_ns_get_num_sectors(ns);
	uint64_t max_lba = total_lba;
	uint32_t desc_max = 32; /* reasonable batch */
	struct spdk_nvme_dsm_range *ranges;
	int rc = 0;

	if (max_lba > ns_sectors) {
		max_lba = ns_sectors;
	}

	uint32_t zone_count = (uint32_t)((max_lba + zone_size_lba - 1) / zone_size_lba);

	ranges = calloc(desc_max, sizeof(*ranges));
	if (!ranges) {
		fprintf(stderr, "Failed to alloc ranges\n");
		return -1;
	}

	printf("Deallocate zones\n");
	printf("  Total LBAs      : %lu\n", max_lba);
	printf("  Zone size (LBA) : %lu\n", zone_size_lba);
	printf("  Zones count     : %u\n", zone_count);

	uint32_t pending = 0;
	for (uint32_t zid = 0; zid < zone_count; zid++) {
		uint64_t start = (uint64_t)zid * zone_size_lba;
		uint64_t end = start + zone_size_lba;
		if (start >= max_lba) {
			break;
		}
		if (end > max_lba) {
			end = max_lba;
		}
		uint64_t length = end - start;

		ranges[pending].starting_lba = start;
		ranges[pending].length = (uint32_t)length;
		ranges[pending].attributes.raw = 0;
		pending++;

		if (pending == desc_max || zid == zone_count - 1) {
			rc = spdk_nvme_ns_cmd_dataset_management(ns, NULL, ranges,
								 pending,
								 SPDK_NVME_DSM_ATTR_DEALLOCATE,
								 NULL, NULL);
			if (rc != 0) {
				fprintf(stderr, "Deallocate submit failed rc=%d (zid=%u)\n", rc, zid);
				goto out;
			}
			/* poll completions */
			while (spdk_nvme_qpair_process_completions(spdk_nvme_ctrlr_get_io_qpair(spdk_nvme_ns_get_ctrlr(ns), 0), 0) > 0) {
				/* no-op */
			}
			pending = 0;
		}
	}

out:
	free(ranges);
	return rc;
}

int
main(int argc, char **argv)
{
	int rc;
	struct spdk_env_opts opts;
	char *device;
	uint64_t total_lba, zone_size_lba;

	opts.opts_size = sizeof(opts);
	spdk_env_opts_init(&opts);

	rc = parse_args(argc, argv, &opts, &device, &total_lba, &zone_size_lba);
	if (rc != 0) {
		return rc;
	}

	opts.name = "zone_deallocate";
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

	rc = do_deallocate(g_ns, total_lba, zone_size_lba);
	if (rc != 0) {
		fprintf(stderr, "Deallocate failed\n");
		rc = 1;
	}

exit:
	if (g_ctrlr) {
		spdk_nvme_detach(g_ctrlr);
	}
	spdk_env_fini();
	return rc;
}

