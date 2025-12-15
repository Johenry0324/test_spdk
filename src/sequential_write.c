/* SPDX-License-Identifier: BSD-3-Clause
 * Sequential write tool for NVMe devices using SPDK
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

struct write_io {
	void *buf;
	uint64_t lba;
	uint32_t lba_count;
	struct write_context *ctx;
};

struct write_context {
	struct spdk_nvme_ns *ns;
	struct spdk_nvme_qpair *qpair;
	uint64_t total_lba;
	uint32_t chunk_lba;
	uint64_t current_lba;
	uint32_t outstanding_ios;
	uint32_t max_outstanding;
	int is_completed;
	int error_occurred;
};

static struct spdk_nvme_ctrlr *g_ctrlr = NULL;
static struct spdk_nvme_ns *g_ns = NULL;
static struct spdk_nvme_transport_id g_trid = {};

static void
write_complete(void *arg, const struct spdk_nvme_cpl *completion)
{
	struct write_io *io = (struct write_io *)arg;
	struct write_context *ctx = io->ctx;

	// Free the buffer
	spdk_free(io->buf);

	if (spdk_nvme_cpl_is_error(completion)) {
		spdk_nvme_qpair_print_completion(ctx->qpair, (struct spdk_nvme_cpl *)completion);
		fprintf(stderr, "Write I/O error at LBA %lu: %s\n",
			io->lba, spdk_nvme_cpl_get_status_string(&completion->status));
		ctx->error_occurred = 1;
		ctx->is_completed = 1;
		free(io);
		return;
	}

	ctx->outstanding_ios--;

	// Check if all writes are completed
	if (ctx->current_lba >= ctx->total_lba && ctx->outstanding_ios == 0) {
		ctx->is_completed = 1;
	}

	// Free the I/O context
	free(io);
}

static int
submit_write(struct write_context *ctx)
{
	struct write_io *io;
	uint32_t lba_count;
	int rc;
	uint64_t lba_size;
	uint64_t current_lba;

	if (ctx->current_lba >= ctx->total_lba) {
		return 0;
	}

	// Allocate I/O context
	io = malloc(sizeof(struct write_io));
	if (io == NULL) {
		fprintf(stderr, "Failed to allocate I/O context\n");
		return -1;
	}

	// Calculate how many LBAs to write in this chunk
	current_lba = ctx->current_lba;
	lba_count = ctx->chunk_lba;
	if (current_lba + lba_count > ctx->total_lba) {
		lba_count = ctx->total_lba - current_lba;
	}

	lba_size = spdk_nvme_ns_get_sector_size(ctx->ns);
	io->buf = spdk_zmalloc(lba_count * lba_size, 0x1000, NULL,
			       SPDK_ENV_NUMA_ID_ANY, SPDK_MALLOC_DMA);
	if (io->buf == NULL) {
		fprintf(stderr, "Failed to allocate write buffer\n");
		free(io);
		return -1;
	}

	// Fill buffer with pattern (optional: can be modified)
	memset(io->buf, 0xAA, lba_count * lba_size);

	io->lba = current_lba;
	io->lba_count = lba_count;
	io->ctx = ctx;

	rc = spdk_nvme_ns_cmd_write(ctx->ns, ctx->qpair, io->buf,
				    current_lba, lba_count,
				    write_complete, io, 0);
	if (rc != 0) {
		fprintf(stderr, "Failed to submit write I/O at LBA %lu\n",
			current_lba);
		spdk_free(io->buf);
		free(io);
		return -1;
	}

	ctx->outstanding_ios++;
	ctx->current_lba += lba_count;

	return 1;
}

static int
sequential_write(struct spdk_nvme_ns *ns, uint64_t total_lba, uint32_t chunk_lba)
{
	struct write_context ctx;
	int rc;

	memset(&ctx, 0, sizeof(ctx));
	ctx.ns = ns;
	ctx.total_lba = total_lba;
	ctx.chunk_lba = chunk_lba;
	ctx.current_lba = 0;
	ctx.max_outstanding = 32; // Maximum outstanding I/Os
	ctx.outstanding_ios = 0;
	ctx.is_completed = 0;
	ctx.error_occurred = 0;

	// Allocate I/O qpair
	ctx.qpair = spdk_nvme_ctrlr_alloc_io_qpair(spdk_nvme_ns_get_ctrlr(ns), NULL, 0);
	if (ctx.qpair == NULL) {
		fprintf(stderr, "Failed to allocate I/O qpair\n");
		return -1;
	}

	printf("Starting sequential write:\n");
	printf("  Total LBAs: %lu\n", total_lba);
	printf("  Chunk size: %u LBAs\n", chunk_lba);
	printf("  LBA size: %u bytes\n", spdk_nvme_ns_get_sector_size(ns));

	// Submit initial batch of I/Os
	while (ctx.outstanding_ios < ctx.max_outstanding && ctx.current_lba < ctx.total_lba) {
		rc = submit_write(&ctx);
		if (rc < 0) {
			fprintf(stderr, "Failed to submit write\n");
			goto cleanup;
		}
		if (rc == 0) {
			break;
		}
	}

	// Process completions and submit more I/Os
	while (!ctx.is_completed) {
		// Process completions
		spdk_nvme_qpair_process_completions(ctx.qpair, 0);

		// Submit more I/Os if we have capacity
		while (ctx.outstanding_ios < ctx.max_outstanding && ctx.current_lba < ctx.total_lba) {
			rc = submit_write(&ctx);
			if (rc < 0) {
				fprintf(stderr, "Failed to submit write\n");
				goto cleanup;
			}
			if (rc == 0) {
				break;
			}
		}

		// Small delay to avoid busy waiting
		if (ctx.outstanding_ios == 0 && ctx.current_lba < ctx.total_lba) {
			usleep(100);
		}
	}

	if (ctx.error_occurred) {
		fprintf(stderr, "Write operation failed with error\n");
		rc = -1;
	} else {
		printf("Sequential write completed successfully\n");
		rc = 0;
	}

cleanup:
	spdk_nvme_ctrlr_free_io_qpair(ctx.qpair);
	return rc;
}

static bool
probe_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
	 struct spdk_nvme_ctrlr_opts *opts)
{
	// Check if this matches our target device
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
	struct spdk_nvme_ns *ns;
	int nsid;

	printf("Attached to NVMe controller at %s\n", trid->traddr);

	g_ctrlr = ctrlr;

	// Get the first active namespace
	nsid = spdk_nvme_ctrlr_get_first_active_ns(ctrlr);
	if (nsid == 0) {
		fprintf(stderr, "No active namespaces found\n");
		return;
	}

	ns = spdk_nvme_ctrlr_get_ns(ctrlr, nsid);
	if (ns == NULL || !spdk_nvme_ns_is_active(ns)) {
		fprintf(stderr, "Invalid namespace\n");
		return;
	}

	g_ns = ns;
	printf("Using namespace ID: %d\n", nsid);
}

static void
usage(const char *program_name)
{
	printf("Usage: %s [options] <device> <size> <chunk_size>\n", program_name);
	printf("\n");
	printf("Arguments:\n");
	printf("  device      Device name (e.g., /dev/nvme0n1 or PCIe address)\n");
	printf("  size        Total LBA count to write\n");
	printf("  chunk_size  LBA count per write operation\n");
	printf("\n");
	printf("Options:\n");
	printf("  -d <size>   DPDK huge memory size in MB\n");
	printf("  -i <id>     Shared memory group ID\n");
	printf("  -r <addr>   NVMe transport address\n");
	printf("  -h          Show this help\n");
}

static int
parse_args(int argc, char **argv, struct spdk_env_opts *env_opts,
	   char **device, uint64_t *size, uint32_t *chunk_size)
{
	int op;
	int arg_index = 0;

	spdk_nvme_trid_populate_transport(&g_trid, SPDK_NVME_TRANSPORT_PCIE);

	while ((op = getopt(argc, argv, "d:hi:r:")) != -1) {
		switch (op) {
		case 'd':
			env_opts->mem_size = spdk_strtol(optarg, 10);
			if (env_opts->mem_size < 0) {
				fprintf(stderr, "Invalid DPDK memory size\n");
				return -1;
			}
			break;
		case 'i':
			env_opts->shm_id = spdk_strtol(optarg, 10);
			if (env_opts->shm_id < 0) {
				fprintf(stderr, "Invalid shared memory ID\n");
				return -1;
			}
			break;
		case 'r':
			if (spdk_nvme_transport_id_parse(&g_trid, optarg) != 0) {
				fprintf(stderr, "Error parsing transport address\n");
				return -1;
			}
			break;
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
		default:
			usage(argv[0]);
			return -1;
		}
	}

	arg_index = optind;
	if (arg_index + 3 > argc) {
		fprintf(stderr, "Missing required arguments\n");
		usage(argv[0]);
		return -1;
	}

	*device = argv[arg_index];
	
	// Parse size (LBA count)
	char *endptr;
	*size = strtoull(argv[arg_index + 1], &endptr, 10);
	if (*endptr != '\0' || *size == 0) {
		fprintf(stderr, "Invalid size argument\n");
		return -1;
	}

	// Parse chunk_size
	*chunk_size = strtoul(argv[arg_index + 2], &endptr, 10);
	if (*endptr != '\0' || *chunk_size == 0) {
		fprintf(stderr, "Invalid chunk_size argument\n");
		return -1;
	}

	// Try to parse device as PCIe address if it looks like /dev/nvme*
	// For simplicity, we'll use the device name directly in transport address
	if (strncmp(*device, "/dev/nvme", 9) == 0) {
		// Extract PCIe address from device name if possible
		// This is a simplified approach - in production, you might want to
		// query the system to get the actual PCIe address
		fprintf(stderr, "Note: Device file names need to be converted to PCIe addresses\n");
		fprintf(stderr, "Please use -r option with PCIe address (e.g., -r 0000:01:00.0)\n");
	}

	return 0;
}

int
main(int argc, char **argv)
{
	int rc;
	struct spdk_env_opts opts;
	char *device;
	uint64_t size;
	uint32_t chunk_size;

	opts.opts_size = sizeof(opts);
	spdk_env_opts_init(&opts);

	rc = parse_args(argc, argv, &opts, &device, &size, &chunk_size);
	if (rc != 0) {
		return rc;
	}

	opts.name = "sequential_write";
	if (spdk_env_init(&opts) < 0) {
		fprintf(stderr, "Unable to initialize SPDK env\n");
		return 1;
	}

	printf("Initializing NVMe Controllers\n");

	// Probe for NVMe controllers
	rc = spdk_nvme_probe(&g_trid, NULL, probe_cb, attach_cb, NULL);
	if (rc != 0) {
		fprintf(stderr, "spdk_nvme_probe() failed\n");
		rc = 1;
		goto exit;
	}

	if (g_ctrlr == NULL || g_ns == NULL) {
		fprintf(stderr, "No NVMe controller or namespace found\n");
		rc = 1;
		goto exit;
	}

	printf("Initialization complete.\n");

	// Perform sequential write
	rc = sequential_write(g_ns, size, chunk_size);
	if (rc != 0) {
		fprintf(stderr, "Sequential write failed\n");
		rc = 1;
	}

exit:
	if (g_ctrlr != NULL) {
		spdk_nvme_detach(g_ctrlr);
	}

	spdk_env_fini();
	return rc;
}

