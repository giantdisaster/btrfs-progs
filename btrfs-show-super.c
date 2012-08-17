/*
 * Copyright (C) 2012 STRATO AG.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#define _XOPEN_SOURCE 500
#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "kerncompat.h"
#include "ctree.h"
#include "disk-io.h"
#include "print-tree.h"
#include "transaction.h"
#include "list.h"
#include "version.h"
#include "utils.h"


static void print_usage(void);
static void dump_superblock(struct btrfs_super_block *sb);
int main(int argc, char **argv);
static void btrfs_print_id(char *prefix, u8 *id, size_t size);


static void print_usage(void)
{
	fprintf(stderr, "usage: btrfs-show-super [-i super_mirror] dev\n");
	fprintf(stderr, "\tThe super_mirror number is between 0 and %d.\n",
		BTRFS_SUPER_MIRROR_MAX - 1);
	fprintf(stderr, "%s\n", BTRFS_BUILD_VERSION);
}

int main(int argc, char **argv)
{
	int opt;
	int ret;
	const char *filename;
	struct btrfs_super_block sb;
	int fd = -1;
	u64 sb_bytenr = btrfs_sb_offset(0);

	while ((opt = getopt(argc, argv, "i:")) != -1) {
		switch(opt) {
		case 'i': {
				int arg = atoi(optarg);

				if (arg < 0 || arg >= BTRFS_SUPER_MIRROR_MAX) {
					fprintf(stderr,
						"Illegal super_mirror %d\n",
						arg);
					print_usage();
					exit(1);
				}
				sb_bytenr = btrfs_sb_offset(arg);
				break;
			}
		default:
			print_usage();
			exit(1);
		}
	}

	if (argc != optind + 1) {
		print_usage();
		exit(1);
	}

	filename = argv[optind];
	fd = open(filename, O_RDONLY, 0666);
	if (fd < 0) {
		fprintf(stderr, "Could not open %s\n", filename);
		goto error;
	}

	ret = pread64(fd, &sb, sizeof(sb), sb_bytenr);
	if (ret < sizeof(sb)) {
		fprintf(stderr, "Failed to read the superblock on %s at %llu\n",
			filename, (unsigned long long)sb_bytenr);
		goto error;
	}

	dump_superblock(&sb);

error:
	if (fd != -1)
		close(fd);

	exit(1);
}

static void dump_superblock(struct btrfs_super_block *sb)
{
	printf("bytenr\t%llu\n", (unsigned long long)btrfs_super_bytenr(sb));
	printf("flags\t0x%llx\n", (unsigned long long)btrfs_super_flags(sb));
	printf("magic \t0x%llx (little endian)\n",
	       (unsigned long long)sb->magic);
	btrfs_print_id("fsid\t", sb->fsid, BTRFS_FSID_SIZE);
	btrfs_print_id("label\t", (u8 *)sb->label, BTRFS_LABEL_SIZE);
	printf("generation\t%llu\n",
	       (unsigned long long)btrfs_super_generation(sb));
	printf("root\t%llu\n", (unsigned long long)btrfs_super_root(sb));
	printf("sys_array_size\t%llu\n",
	       (unsigned long long)btrfs_super_sys_array_size(sb));
	printf("chunk_root_generation\t%llu\n",
	       (unsigned long long)btrfs_super_chunk_root_generation(sb));
	printf("root_level\t%llu\n",
	       (unsigned long long)btrfs_super_root_level(sb));
	printf("chunk_root\t%llu\n",
	       (unsigned long long)btrfs_super_chunk_root(sb));
	printf("chunk_root_level\t%llu\n",
	       (unsigned long long)btrfs_super_chunk_root_level(sb));
	printf("log_root\t%llu\n",
	       (unsigned long long)btrfs_super_log_root(sb));
	printf("log_root_transid\t%llu\n",
	       (unsigned long long)btrfs_super_log_root_transid(sb));
	printf("log_root_level\t%llu\n",
	       (unsigned long long)btrfs_super_log_root_level(sb));
	printf("total_bytes\t%llu\n",
	       (unsigned long long)btrfs_super_total_bytes(sb));
	printf("bytes_used\t%llu\n",
	       (unsigned long long)btrfs_super_bytes_used(sb));
	printf("sectorsize\t%llu\n",
	       (unsigned long long)btrfs_super_sectorsize(sb));
	printf("nodesize\t%llu\n",
	       (unsigned long long)btrfs_super_nodesize(sb));
	printf("leafsize\t%llu\n",
	       (unsigned long long)btrfs_super_leafsize(sb));
	printf("stripesize\t%llu\n",
	       (unsigned long long)btrfs_super_stripesize(sb));
	printf("root_dir\t%llu\n",
	       (unsigned long long)btrfs_super_root_dir(sb));
	printf("num_devices\t%llu\n",
	       (unsigned long long)btrfs_super_num_devices(sb));
	printf("compat_flags\t0x%llx\n",
	       (unsigned long long)btrfs_super_compat_flags(sb));
	printf("compat_ro_flags\t0x%llx\n",
	       (unsigned long long)btrfs_super_compat_ro_flags(sb));
	printf("incompat_flags\t0x%llx\n",
	       (unsigned long long)btrfs_super_incompat_flags(sb));
	printf("csum_type\t%llu\n",
	       (unsigned long long)btrfs_super_csum_type(sb));
	printf("cache_generation\t%llu\n",
	       (unsigned long long)btrfs_super_cache_generation(sb));

	printf("dev_item.type\t%llu\n", (unsigned long long)
	       btrfs_stack_device_type(&sb->dev_item));
	printf("dev_item.total_bytes\t%llu\n", (unsigned long long)
	       btrfs_stack_device_total_bytes(&sb->dev_item));
	printf("dev_item.bytes_used\t%llu\n", (unsigned long long)
	       btrfs_stack_device_bytes_used(&sb->dev_item));
	printf("dev_item.io_align\t%u\n", (unsigned int)
	       btrfs_stack_device_io_align(&sb->dev_item));
	printf("dev_item.io_width\t%u\n", (unsigned int)
	       btrfs_stack_device_io_width(&sb->dev_item));
	printf("dev_item.sector_size\t%u\n", (unsigned int)
	       btrfs_stack_device_sector_size(&sb->dev_item));
	printf("dev_item.devid\t%llu\n",
	       btrfs_stack_device_id(&sb->dev_item));
	printf("dev_item.dev_group\t%u\n", (unsigned int)
	       btrfs_stack_device_group(&sb->dev_item));
	printf("dev_item.seek_speed\t%u\n", (unsigned int)
	       btrfs_stack_device_seek_speed(&sb->dev_item));
	printf("dev_item.bandwidth\t%u\n", (unsigned int)
	       btrfs_stack_device_bandwidth(&sb->dev_item));
	printf("dev_item.generation\t%llu\n", (unsigned long long)
	       btrfs_stack_device_generation(&sb->dev_item));
}

static void btrfs_print_id(char *prefix, u8 *id, size_t size)
{
	int i;
	char buf[3 * size];

	for (i = 0; i < size; i++) {
		buf[3 * i] = ((id[i] >> 4) < 10) ?
			      (id[i] >> 4) + '0' :
			      (id[i] >> 4) + 'A' - 10;
		buf[3 * i + 1] = (id[i] & 0xf) < 10 ?
				  (id[i] & 0xf) + '0' :
				  (id[i] & 0xf) + 'A' - 10;
		buf[3 * i + 2] = (i == size - 1) ? '\0' : ':';
	}
	printf("%s%s\n", prefix, buf);
}
