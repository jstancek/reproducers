#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* gcc -O2 mmap5.c -g3 -lpthread */

static int map_size = 4096;
static int num_iter = 500;
static long threads_total;

static void *distant_area;

static long long timespec_diff(struct timespec t1, struct timespec t2)
{
        struct timespec res;

        res.tv_sec = t1.tv_sec - t2.tv_sec;
        if (t1.tv_nsec < t2.tv_nsec) {
                res.tv_sec--;
                res.tv_nsec = 1000000000 - (t2.tv_nsec - t1.tv_nsec);
        } else {
                res.tv_nsec = t1.tv_nsec - t2.tv_nsec;
        }

        return res.tv_sec * 1000 + (res.tv_nsec + 500000) / 1000000;
}

void *map_write_unmap(void *ptr)
{
	int *fd = ptr;
	unsigned char *map_address;
	int i, j = 0;
        struct timespec start, stop;
	long long ms;

	clock_gettime(CLOCK_MONOTONIC, &start);
	for (i = 0; i < num_iter; i++) {
		map_address = mmap(distant_area, (size_t) map_size, PROT_WRITE | PROT_READ,
			MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		if (map_address == MAP_FAILED) {
			perror("mmap");
			exit(1);
		}

		for (j = 0; j < map_size; j++)
			map_address[j] = 'b';

		if (munmap(map_address, map_size) == -1) {
			perror("munmap");
			exit(1);
		}
	}
	clock_gettime(CLOCK_MONOTONIC, &stop);

	ms = timespec_diff(stop, start);
	printf("[%08ld] map_write_unmap took: %lld ms", threads_total++, ms);
	if (ms < 500)
		printf("\r");
	else
		printf("\n");
	return NULL;
}

void *dummy(void *ptr)
{
	return NULL;
}

#define DISTANT_MMAP_SIZE (2L*1024*1024*1024)
int main(void)
{
	pthread_t thid[2];

	/* hint for mmap in map_write_unmap() */
	distant_area = mmap(0, DISTANT_MMAP_SIZE, PROT_WRITE | PROT_READ,
			MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	munmap(distant_area, (size_t)DISTANT_MMAP_SIZE);
	distant_area += DISTANT_MMAP_SIZE / 2;

	while (1) {
		pthread_create(&thid[0], NULL, map_write_unmap, NULL);
		pthread_create(&thid[1], NULL, dummy, NULL);

		pthread_join(thid[0], NULL);
		pthread_join(thid[1], NULL);
	}
}
