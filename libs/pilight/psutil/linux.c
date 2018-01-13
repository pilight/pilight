#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <libgen.h>

#include "psutil.h"

static int maxpid = -1;

unsigned long psutil_max_pid(void) {
	if(maxpid == -1) {
		int fd = open("/proc/sys/kernel/pid_max", O_RDONLY, 0), ptr = 0;
		if(fd == -1) {
			return maxpid;
		}
		char cmdline[17];
		if((ptr = (int)read(fd, cmdline, 16)) > -1) {
			maxpid = atoi(cmdline);
		}
		close(fd);
	}
	return maxpid;
}

int psutil_proc_exe(const pid_t pid, char **name) {
	int ptr = 0;
	char path[512], *p = path, cmdline[1024];

	snprintf(p, 511, "/proc/%d/exe", pid);
	if((ptr = readlink(path, cmdline, 1023)) != -1) {
		cmdline[ptr] = '\0';
		strcpy(*name, cmdline);
		return 0;
	}

	return -1;
}

int psutil_proc_name(const pid_t pid, char **name, size_t bufsize) {
	int ptr = 0, fd = 0;
	char path[512], *p = path, cmdline[bufsize];

	snprintf(p, 511, "/proc/%d/cmdline", pid);
	fd = open(path, O_RDONLY, 0);
	if(fd == -1) {
		return -1;
	}

	if((ptr = (int)read(fd, cmdline, bufsize-1)) > 0) {
		strcpy(*name, basename(cmdline));
		close(fd);
		return 0;
	}
	close(fd);
	return -1;
}

size_t psutil_get_cmd_args(pid_t pid, char **cmdline, size_t size) {
	int ptr = 0, fd = 0;
	char path[512], *p = path;

	memset(*cmdline, 0,  size-1);

	snprintf(p, 511, "/proc/%d/cmdline", pid);

	fd = open(path, O_RDONLY, 0);
	if(fd == -1) {
		return 0;
	}

	if((ptr = (int)read(fd, *cmdline, size)) > 0) {
		int i = 0;
		/* Replace all NULL terminators for newlines */
		for(i=0;i<ptr-1;i++) {
			if(i < ptr && (*cmdline)[i] == '\0') {
				(*cmdline)[i] = ' ';
			}
		}
	}
	close(fd);
	if(ptr > 0) {
		return ptr;
	}
	return -1;
}

int psutil_cpu_count_phys(void) {
	int nprocs = sysconf(_SC_NPROCESSORS_ONLN);
  if(nprocs < 1) {
    return -1;
  }

  int nprocs_max = sysconf(_SC_NPROCESSORS_CONF);
  if(nprocs_max < 1) {
		return -1;
  }
  return nprocs;
}