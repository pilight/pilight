/*
	Copyright (C) 2014 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifdef __FreeBSD__
	#define	_WITH_GETLINE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#ifdef _WIN32
	#include <windows.h>
#else
	#include <pthread.h>
	#include <unistd.h>
	#include <sys/time.h>
#endif
#include <ctype.h>

#include "proc.h"
#include "log.h"
#include "mem.h"

/* RAM usage */
#ifndef _WIN32
	static unsigned long totalram = 0;
#endif
static unsigned short initialized = 0;

/* Mounting proc subsystem */
#ifdef __FreeBSD__
	static unsigned short mountproc = 1;
#endif

static FILE *fstatus = NULL;
static FILE *fmemory = NULL;

int proc_gc(void) {
	if(fstatus != NULL) {
		fclose(fstatus);
	}
	if(fmemory != NULL) {
		fclose(fmemory);
	}
	return 0;
}

double getCPUUsage(void) {
	static struct cpu_usage_t cpu_usage;
	if(!initialized) {
		memset(&cpu_usage, '\0', sizeof(struct cpu_usage_t));
		cpu_usage.cpu_old = 0;
		cpu_usage.cpu_per = 0;
		cpu_usage.cpu_new = 0;
		cpu_usage.sec_start = 0;
		cpu_usage.sec_stop = 0;
		cpu_usage.sec_diff = 0;
		memset(&cpu_usage.ts, '\0', sizeof(struct timespec));
		cpu_usage.starts = 0;
		initialized = 1;
	}
#ifdef _WIN32
	FILETIME createTime;
	FILETIME exitTime;
	FILETIME kernelTime;
	FILETIME userTime;
	if(GetProcessTimes(GetCurrentProcess(), &createTime, &exitTime, &kernelTime, &userTime) != -1) {
		SYSTEMTIME userSystemTime;
		if(FileTimeToSystemTime(&userTime, &userSystemTime) != -1) {
			cpu_usage.cpu_new = ((double)userSystemTime.wHour * 3600.0 + (double)userSystemTime.wMinute * 60.0 +
													 (double)userSystemTime.wSecond + (double)userSystemTime.wMilliseconds / 1000.0);
			double a = (cpu_usage.cpu_new - cpu_usage.cpu_old);
			cpu_usage.cpu_old = cpu_usage.cpu_new;
			return a;
		}
	}
	return 0.0;
#else
	clock_gettime(CLOCK_REALTIME, &cpu_usage.ts);
	cpu_usage.sec_stop = cpu_usage.ts.tv_sec + cpu_usage.ts.tv_nsec / 1e9;

	cpu_usage.sec_diff = cpu_usage.sec_stop - cpu_usage.sec_start;

	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_usage.ts);
	cpu_usage.cpu_new = (cpu_usage.ts.tv_sec + (cpu_usage.ts.tv_nsec / 1e9));
	double a = ((cpu_usage.cpu_new-cpu_usage.cpu_old) / cpu_usage.sec_diff * 100.0);
	cpu_usage.cpu_old = cpu_usage.cpu_new;

	clock_gettime(CLOCK_REALTIME, &cpu_usage.ts);
	cpu_usage.sec_start = cpu_usage.ts.tv_sec + cpu_usage.ts.tv_nsec / 1e9;

	return a;
#endif
}

double getRAMUsage(void) {
#ifndef _WIN32
	#if defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
	if(totalram == 0) {
		totalram = (size_t)sysconf(_SC_PHYS_PAGES)*(size_t)sysconf(_SC_PAGESIZE);
	}
	#endif

	#ifdef __FreeBSD__
		static char statusfile[] = "/compat/proc/self/status";
		static char memfile[] = "/compat/proc/meminfo";
	#else
		static char statusfile[] = "/proc/self/status";
		static char memfile[] = "/proc/meminfo";
	#endif
	unsigned long VmRSS = 0, value = 0, total = 0;
	size_t len = 0;
	ssize_t rd = 0;
	char units[32], title[32], *line = NULL;


	memset(title, '\0', 32);
	memset(units, '\0', 32);

	if(fstatus == NULL) {
		if((fstatus = fopen(statusfile, "r")) == NULL) {
			return 0.0;
		}
	}
	rewind(fstatus);
	while((rd = getline(&line, &len, fstatus)) != -1) {
		if(strstr(line, "VmRSS:") != NULL) {
			if(sscanf(line, "%31s %lu %s\n", title, &value, units) > 0) {
				VmRSS = value * 1024;
			}
			if(line != NULL) {
				free(line);
				line = NULL;
			}
			break;
		}
	}
	if(line != NULL) {
		free(line);
		line = NULL;
	}
	#ifdef __FreeBSD__
		if(mountproc == 1) {
			DIR* dir;
			if(!(dir = opendir("/compat"))) {
				mkdir("/compat", 0755);
			} else {
				closedir(dir);
			}
			if(!(dir = opendir("/compat/proc"))) {
				mkdir("/compat/proc", 0755);
			} else {
				closedir(dir);
			}
			if(!(dir = opendir("/compat/proc/self"))) {
				system("mount -t linprocfs none /compat/proc 2>/dev/null 1>/dev/null");
				mountproc = 0;
			} else {
				closedir(dir);
			}
		}
	#endif

	if(fmemory == NULL) {
		if((fmemory = fopen(memfile, "r")) != NULL) {
			return 0.0;
		}
	}
	rewind(fmemory);
	while((rd = getline(&line, &len, fmemory)) != -1) {
		if(strstr(line, "MemTotal:") != NULL) {
			if(sscanf(line, "%31s %lu %s\n", title, &value, units) > 0) {
				total = value * 1024;
			}
			if(line != NULL) {
				free(line);
				line = NULL;
			}
			break;
		}
	}
	if(line != NULL) {
		free(line);
		line = NULL;
	}
	if(VmRSS > 0 && total > 0) {
		return ((double)VmRSS*100)/(double)total;
	} else {
		return 0.0;
	}
#else
	return 0.0;
#endif
}
