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
#ifdef __FreeBSD__
	#include <dirent.h>
#endif
#include <ctype.h>

#include "proc.h"
#include "log.h"
#include "mem.h"

static unsigned short initialized = 0;

#ifndef _WIN32
void getThreadCPUUsage(pthread_t pth, struct cpu_usage_t *cpu_usage) {
#ifdef _WIN32
	FILETIME createTime;
	FILETIME exitTime;
	FILETIME kernelTime;
	FILETIME userTime;
	if(GetThreadTimes(pthread_gethandle(pth), &createTime, &exitTime, &kernelTime, &userTime) != -1) {
		SYSTEMTIME userSystemTime;
		if(FileTimeToSystemTime(&userTime, &userSystemTime) != -1) {
			cpu_usage->cpu_new = ((double)userSystemTime.wHour * 3600.0 + (double)userSystemTime.wMinute * 60.0 +
													 (double)userSystemTime.wSecond + (double)userSystemTime.wMilliseconds / 1000.0);
			if(cpu_usage->cpu_per > 100) {
				cpu_usage->cpu_per = (cpu_usage->cpu_new - cpu_usage->cpu_old);
			}
			cpu_usage->cpu_old = cpu_usage->cpu_new;
		}
	}
	cpu_usage->cpu_per = 0;
#else
	clockid_t cid;
	memset(&cid, '\0', sizeof(cid));

	clock_gettime(CLOCK_REALTIME, &cpu_usage->ts);
	cpu_usage->sec_stop = cpu_usage->ts.tv_sec + cpu_usage->ts.tv_nsec / 1e9;

	cpu_usage->sec_diff = cpu_usage->sec_stop - cpu_usage->sec_start;

	pthread_getcpuclockid(pth, &cid);
	clock_gettime(cid, &cpu_usage->ts);

	cpu_usage->cpu_new = (cpu_usage->ts.tv_sec + (cpu_usage->ts.tv_nsec / 1e9));
	cpu_usage->cpu_per = ((cpu_usage->cpu_new-cpu_usage->cpu_old) / cpu_usage->sec_diff * 100.0);
	if(cpu_usage->cpu_per > 100) {
		cpu_usage->cpu_per = 0;
	}
	cpu_usage->cpu_old = cpu_usage->cpu_new;

	clock_gettime(CLOCK_REALTIME, &cpu_usage->ts);
	cpu_usage->sec_start = cpu_usage->ts.tv_sec + cpu_usage->ts.tv_nsec / 1e9;
#endif
}
#endif

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