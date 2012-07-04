/*
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/stat.h>

#include "kerncompat.h"
#include "ctree.h"
#include "ioctl.h"
#include "utils.h"

#include "commands.h"

/* FIXME - imported cruft, fix sparse errors and warnings */
#ifdef __CHECKER__
#define BLKGETSIZE64 0
#define BTRFS_IOC_SNAP_CREATE_V2 0
#define BTRFS_VOL_NAME_MAX 255
struct btrfs_ioctl_vol_args { char name[BTRFS_VOL_NAME_MAX]; };
static inline int ioctl(int fd, int define, void *arg) { return 0; }
#endif

static const char * const device_cmd_group_usage[] = {
	"btrfs device <command> [<args>]",
	NULL
};

static const char * const cmd_add_dev_usage[] = {
	"btrfs device add <device> [<device>...] <path>",
	"Add a device to a filesystem",
	NULL
};

static int cmd_add_dev(int argc, char **argv)
{
	char	*mntpnt;
	int	i, fdmnt, ret=0, e;

	if (check_argc_min(argc, 3))
		usage(cmd_add_dev_usage);

	mntpnt = argv[argc - 1];

	fdmnt = open_file_or_dir(mntpnt);
	if (fdmnt < 0) {
		fprintf(stderr, "ERROR: can't access to '%s'\n", mntpnt);
		return 12;
	}

	for (i = 1; i < argc - 1; i++ ){
		struct btrfs_ioctl_vol_args ioctl_args;
		int	devfd, res;
		u64 dev_block_count = 0;
		struct stat st;
		int mixed = 0;

		res = check_mounted(argv[i]);
		if (res < 0) {
			fprintf(stderr, "error checking %s mount status\n",
				argv[i]);
			ret++;
			continue;
		}
		if (res == 1) {
			fprintf(stderr, "%s is mounted\n", argv[i]);
			ret++;
			continue;
		}

		devfd = open(argv[i], O_RDWR);
		if (!devfd) {
			fprintf(stderr, "ERROR: Unable to open device '%s'\n", argv[i]);
			close(devfd);
			ret++;
			continue;
		}
		res = fstat(devfd, &st);
		if (res) {
			fprintf(stderr, "ERROR: Unable to stat '%s'\n", argv[i]);
			close(devfd);
			ret++;
			continue;
		}
		if (!S_ISBLK(st.st_mode)) {
			fprintf(stderr, "ERROR: '%s' is not a block device\n", argv[i]);
			close(devfd);
			ret++;
			continue;
		}

		res = btrfs_prepare_device(devfd, argv[i], 1, &dev_block_count, &mixed);
		if (res) {
			fprintf(stderr, "ERROR: Unable to init '%s'\n", argv[i]);
			close(devfd);
			ret++;
			continue;
		}
		close(devfd);

		strncpy(ioctl_args.name, argv[i], BTRFS_PATH_NAME_MAX);
		res = ioctl(fdmnt, BTRFS_IOC_ADD_DEV, &ioctl_args);
		e = errno;
		if(res<0){
			fprintf(stderr, "ERROR: error adding the device '%s' - %s\n",
				argv[i], strerror(e));
			ret++;
		}

	}

	close(fdmnt);
	if (ret)
		return ret+20;
	else
		return 0;
}

static const char * const cmd_rm_dev_usage[] = {
	"btrfs device delete <device> [<device>...] <path>",
	"Remove a device from a filesystem",
	NULL
};

static int cmd_rm_dev(int argc, char **argv)
{
	char	*mntpnt;
	int	i, fdmnt, ret=0, e;

	if (check_argc_min(argc, 3))
		usage(cmd_rm_dev_usage);

	mntpnt = argv[argc - 1];

	fdmnt = open_file_or_dir(mntpnt);
	if (fdmnt < 0) {
		fprintf(stderr, "ERROR: can't access to '%s'\n", mntpnt);
		return 12;
	}

	for(i=1 ; i < argc - 1; i++ ){
		struct	btrfs_ioctl_vol_args arg;
		int	res;

		strncpy(arg.name, argv[i], BTRFS_PATH_NAME_MAX);
		res = ioctl(fdmnt, BTRFS_IOC_RM_DEV, &arg);
		e = errno;
		if(res<0){
			fprintf(stderr, "ERROR: error removing the device '%s' - %s\n",
				argv[i], strerror(e));
			ret++;
		}
	}

	close(fdmnt);
	if( ret)
		return ret+20;
	else
		return 0;
}

static const char * const cmd_scan_dev_usage[] = {
	"btrfs device scan [<device>...]",
	"Scan devices for a btrfs filesystem",
	NULL
};

static int cmd_scan_dev(int argc, char **argv)
{
	int	i, fd, e;
	int	checklist = 1;
	int	devstart = 1;

	if( argc > 1 && !strcmp(argv[1],"--all-devices")){
		if (check_argc_max(argc, 2))
			usage(cmd_scan_dev_usage);

		checklist = 0;
		devstart += 1;
	}

	if(argc<=devstart){

		int ret;

		printf("Scanning for Btrfs filesystems\n");
		if(checklist)
			ret = btrfs_scan_block_devices(1);
		else
			ret = btrfs_scan_one_dir("/dev", 1);
		if (ret){
			fprintf(stderr, "ERROR: error %d while scanning\n", ret);
			return 18;
		}
		return 0;
	}

	fd = open("/dev/btrfs-control", O_RDWR);
	if (fd < 0) {
		perror("failed to open /dev/btrfs-control");
		return 10;
	}

	for( i = devstart ; i < argc ; i++ ){
		struct btrfs_ioctl_vol_args args;
		int ret;

		printf("Scanning for Btrfs filesystems in '%s'\n", argv[i]);

		strncpy(args.name, argv[i], BTRFS_PATH_NAME_MAX);
		/*
		 * FIXME: which are the error code returned by this ioctl ?
		 * it seems that is impossible to understand if there no is
		 * a btrfs filesystem from an I/O error !!!
		 */
		ret = ioctl(fd, BTRFS_IOC_SCAN_DEV, &args);
		e = errno;

		if( ret < 0 ){
			close(fd);
			fprintf(stderr, "ERROR: unable to scan the device '%s' - %s\n",
				argv[i], strerror(e));
			return 11;
		}
	}

	close(fd);
	return 0;
}

static const char * const cmd_dev_stats_usage[] = {
	"btrfs device stats [-z] <path>|<device>",
	"Show current device IO stats. -z to reset stats afterwards.",
	NULL
};

static int cmd_dev_stats(int argc, char **argv)
{
	char *path;
	struct btrfs_ioctl_fs_info_args fi_args;
	struct btrfs_ioctl_dev_info_args *di_args = NULL;
	int ret;
	int fdmnt;
	int i;
	char c;
	int fdres = -1;
	int err = 0;
	__u64 flags = 0;

	optind = 1;
	while ((c = getopt(argc, argv, "z")) != -1) {
		switch (c) {
		case 'z':
			flags = BTRFS_DEV_STATS_RESET;
			break;
		case '?':
		default:
			fprintf(stderr, "ERROR: device stat args invalid.\n"
					" device stat [-z] <path>|<device>\n"
					" -z  to reset stats after reading.\n");
			return 1;
		}
	}

	if (optind + 1 != argc) {
		fprintf(stderr, "ERROR: device stat needs path|device as single"
			" argument\n");
		return 1;
	}

	path = argv[optind];

	fdmnt = open_file_or_dir(path);
	if (fdmnt < 0) {
		fprintf(stderr, "ERROR: can't access '%s'\n", path);
		return 12;
	}

	ret = get_fs_info(fdmnt, path, &fi_args, &di_args);
	if (ret) {
		fprintf(stderr, "ERROR: getting dev info for devstats failed: "
				"%s\n", strerror(-ret));
		err = 1;
		goto out;
	}
	if (!fi_args.num_devices) {
		fprintf(stderr, "ERROR: no devices found\n");
		err = 1;
		goto out;
	}

	for (i = 0; i < fi_args.num_devices; i++) {
		struct btrfs_ioctl_get_dev_stats args = {0};
		__u8 path[BTRFS_DEVICE_PATH_NAME_MAX + 1];

		strncpy((char *)path, (char *)di_args[i].path,
			BTRFS_DEVICE_PATH_NAME_MAX);
		path[BTRFS_DEVICE_PATH_NAME_MAX] = '\0';

		args.devid = di_args[i].devid;
		args.nr_items = BTRFS_DEV_STAT_VALUES_MAX;
		args.flags = flags;

		if (ioctl(fdmnt, BTRFS_IOC_GET_DEV_STATS, &args) < 0) {
			fprintf(stderr,
				"ERROR: ioctl(BTRFS_IOC_GET_DEV_STATS) on %s failed: %s\n",
				path, strerror(errno));
			err = 1;
		} else {
			if (args.nr_items >= BTRFS_DEV_STAT_WRITE_ERRS + 1)
				printf("[%s].write_io_errs   %llu\n",
				       path,
				       (unsigned long long) args.values[
					BTRFS_DEV_STAT_WRITE_ERRS]);
			if (args.nr_items >= BTRFS_DEV_STAT_READ_ERRS + 1)
				printf("[%s].read_io_errs    %llu\n",
				       path,
				       (unsigned long long) args.values[
					BTRFS_DEV_STAT_READ_ERRS]);
			if (args.nr_items >= BTRFS_DEV_STAT_FLUSH_ERRS + 1)
				printf("[%s].flush_io_errs   %llu\n",
				       path,
				       (unsigned long long) args.values[
					BTRFS_DEV_STAT_FLUSH_ERRS]);
			if (args.nr_items >= BTRFS_DEV_STAT_CORRUPTION_ERRS + 1)
				printf("[%s].corruption_errs %llu\n",
				       path,
				       (unsigned long long) args.values[
					BTRFS_DEV_STAT_CORRUPTION_ERRS]);
			if (args.nr_items >= BTRFS_DEV_STAT_GENERATION_ERRS + 1)
				printf("[%s].generation_errs %llu\n",
				       path,
				       (unsigned long long) args.values[
					BTRFS_DEV_STAT_GENERATION_ERRS]);
		}
	}

out:
	free(di_args);
	close(fdmnt);
	if (fdres > -1)
		close(fdres);

	return err;
}

const struct cmd_group device_cmd_group = {
	device_cmd_group_usage, NULL, {
		{ "add", cmd_add_dev, cmd_add_dev_usage, NULL, 0 },
		{ "delete", cmd_rm_dev, cmd_rm_dev_usage, NULL, 0 },
		{ "scan", cmd_scan_dev, cmd_scan_dev_usage, NULL, 0 },
		{ "stats", cmd_dev_stats, cmd_dev_stats_usage, NULL, 0 },
		{ 0, 0, 0, 0, 0 }
	}
};

int cmd_device(int argc, char **argv)
{
	return handle_command_group(&device_cmd_group, argc, argv);
}
