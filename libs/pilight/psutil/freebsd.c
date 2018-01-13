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

#include "psutil.h"

unsigned long psutil_max_pid(void) {
	return 99999;
}

int psutil_proc_exe(const pid_t pid, char **name) {
	int mib[4];
	char pathname[PATH_MAX];
	size_t size;
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PATHNAME;
	mib[3] = pid;

	size = sizeof(pathname);
	int error = sysctl(mib, 4, pathname, &size, NULL, 0);
	if(error == -1 || size <= 0) {
		// PyErr_SetFromErrno(PyExc_OSError);
		return -1;
	}
	strcpy(*name, pathname);

	return 0;
}

int psutil_proc_name(const pid_t pid, char **name, size_t bufsize) {
	// Fills a kinfo_proc struct based on process pid.
	struct kinfo_proc proc;
	int mib[4];
	size_t size;
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID;
	mib[3] = pid;

	size = sizeof(struct kinfo_proc);
	if(sysctl((int *)mib, 4, &proc, &size, NULL, 0) == -1) {
		return -1;
	}

	// sysctl stores 0 in the size if we can't find the process information.
	if(size == 0) {
		return -1;
	}

	strcpy(*name, proc.ki_comm);
	return 0;
}

size_t psutil_get_cmd_args(pid_t pid, char **procargs, size_t size) {
	int mib[4];
	int argmax = 0;
	size_t ret = size;

	// Get the maximum process arguments size.
	mib[0] = CTL_KERN;
	mib[1] = KERN_ARGMAX;

	if(sysctl(mib, 2, &argmax, &ret, NULL, 0) == -1) {
		return 0;
	}

	// Make a sysctl() call to get the raw argument space of the process.
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_ARGS;
	mib[3] = pid;

	ret = size;
	if(sysctl(mib, 4, *procargs, &ret, NULL, 0) == -1) {
		return 0;
	}

	return ret;
}

int psutil_cpu_count_phys(void) {
	int ncpu;
	size_t ncpucount = sizeof(ncpu);

	if(sysctlbyname("hw.ncpu", &ncpu, &ncpucount, NULL, 0)) {
		return -1;
	}

	return ncpu;
}