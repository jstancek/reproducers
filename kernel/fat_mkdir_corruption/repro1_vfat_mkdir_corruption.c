#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/loop.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>

/*
[   19.656265] FAT-fs (loop0): error, corrupted directory (invalid entries)
[   19.657073] FAT-fs (loop0): Filesystem has been set read-only
*/

int main(void)
{
	int i, j, ret, fd, loop_fd, ctrl_fd;
	int loop_num;
	char loopdev[256], tmp[256], testfile[256];

	mkdir("/tmp/mntpoint", 0777);
	for (i = 0; ; i++) {
		printf("Iteration: %d\n", i);
		sprintf(testfile, "/tmp/test.img.%d", getpid());

		ctrl_fd = open("/dev/loop-control", O_RDWR);
		loop_num = ioctl(ctrl_fd, LOOP_CTL_GET_FREE);
		close(ctrl_fd);
		sprintf(loopdev, "/dev/loop%d", loop_num);

		fd = open(testfile, O_WRONLY|O_CREAT|O_TRUNC, 0600);
		fallocate(fd, 0, 0, 256*1024*1024);
		close(fd);

		fd = open(testfile, O_RDWR);
		loop_fd = open(loopdev, O_RDWR);
		ioctl(loop_fd, LOOP_SET_FD, fd);
		close(loop_fd);
		close(fd);

		sprintf(tmp, "mkfs.vfat %s", loopdev);
		system(tmp);
		mount(loopdev, "/tmp/mntpoint", "vfat", 0, NULL);

		for (j = 0; j < 200; j++) {
			sprintf(tmp, "/tmp/mntpoint/testdir%d", j);
			ret = mkdir(tmp, 0777);
			if (ret) {
				perror("mkdir");
				break;
			}
		}

		umount("/tmp/mntpoint");
		loop_fd = open(loopdev, O_RDWR);
		ioctl(loop_fd, LOOP_CLR_FD, fd);
		close(loop_fd);
		unlink(testfile);

		if (ret)
			break;
	}

	return 0;
}
