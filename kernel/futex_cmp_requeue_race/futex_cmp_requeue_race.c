#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <linux/futex.h>

static uint32_t *uaddr;

void sigwinch_handler(int sig)
{
	return;
}

void sigalrm_handler(int sig)
{
	const char *errmsg = "child1 timed out\n";

	write(0, errmsg, 17);
	_exit(1);
}

static void child1(void)
{
	struct sigaction act;
	int ret;

	memset(&act, 0, sizeof(act));
	act.sa_flags = SA_RESTART;
	act.sa_handler = sigwinch_handler;
	if (sigaction(SIGWINCH, &act, NULL)) {
		perror("sigaction SIGWINCH");
		exit(1);
	}

	memset(&act, 0, sizeof(act));
	act.sa_handler = sigalrm_handler;
	if (sigaction(SIGALRM, &act, NULL)) {
		perror("sigaction SIGALRM");
		exit(1);
	}

	alarm(10);
	ret = syscall(__NR_futex, &uaddr[0], FUTEX_WAIT, uaddr[0], NULL, NULL, 0);
	printf("child1 exiting, ret: %d (%s)\n", ret, strerror(errno));

	exit(0);
}

int main(void)
{
	pid_t childpid;
	int ret;
	uint32_t curval;

	uaddr = mmap(0, getpagesize(), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	uaddr[0] = 1000;
	uaddr[1] = 2000;

	childpid = fork();
	if (childpid == 0)
		child1();
	sleep(1);

	ret = syscall(__NR_futex, &uaddr[0], FUTEX_CMP_REQUEUE, 0, 1, &uaddr[1], uaddr[0]);
	if (ret != 1) {
		perror("requeue");
		exit(1);
	}

	/*
	 * signal will cause futex_wait() in child1 to restart, but it will
	 * restart with uaddr[0], hence FUTEX_WAKE below will never wake it
	 * at uaddr[1].
	 */
	kill(childpid, SIGWINCH);

	/*
	 * comment sleep below to make it more likely for FUTEX_WAKE to succeed
	 * or FUTEX_WAIT in child hit EAGAIN
	 */
	sleep(1);

	uaddr[0]++;
	uaddr[1]++;

	for (;;) {
		ret = syscall(__NR_futex, &uaddr[1], FUTEX_WAKE, 0, NULL, NULL, 0);
		if (ret == 1)
			break;

		if (waitpid(childpid, NULL, WNOHANG) == childpid)
			break;

		sleep(1);
	};

	return 0;
}
