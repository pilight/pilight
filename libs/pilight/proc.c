/*
	Copyright (C) 2014 CurlyMo

	This file is part of pilight.

    pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

    pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <dirent.h>

#include "proc.h"

/* RAM usage */
static unsigned long totalram = 0;

/* Mounting proc subsystem */
#ifdef __FreeBSD__
	static unsigned short mountproc = 1;
#endif

double getCPUUsage(void) {
	static struct cpu_usage_t cpu_usage;

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
}

void getThreadCPUUsage(pthread_t *pth, struct cpu_usage_t *cpu_usage) {
	clockid_t cid;

	clock_gettime(CLOCK_REALTIME, &cpu_usage->ts);
	cpu_usage->sec_stop = cpu_usage->ts.tv_sec + cpu_usage->ts.tv_nsec / 1e9;	

	cpu_usage->sec_diff = cpu_usage->sec_stop - cpu_usage->sec_start;	
	
	pthread_getcpuclockid(*pth, &cid);
	clock_gettime(cid, &cpu_usage->ts);

	cpu_usage->cpu_new = (cpu_usage->ts.tv_sec + (cpu_usage->ts.tv_nsec / 1e9));
	cpu_usage->cpu_per = ((cpu_usage->cpu_new-cpu_usage->cpu_old) / cpu_usage->sec_diff * 100.0);
	if(cpu_usage->cpu_per > 100) {
		cpu_usage->cpu_per = 0;
	}
	cpu_usage->cpu_old = cpu_usage->cpu_new;
	
	clock_gettime(CLOCK_REALTIME, &cpu_usage->ts);
	cpu_usage->sec_start = cpu_usage->ts.tv_sec + cpu_usage->ts.tv_nsec / 1e9;
}

double getRAMUsage(void) {
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
    char title[32] = "";
    char units[32] = "";
    int ret = 0;
    FILE *fp = NULL;

    fp = fopen(statusfile, "r");
    if(fp) {
        while(ret != EOF) {
            ret = fscanf(fp, "%31s %lu %s\n", title, &value, units);
            if(strcmp(title, "VmRSS:") == 0) {
                VmRSS = value * 1024;
				break;
            }
        }
        fclose(fp);
#ifdef __FreeBSD__
		if(mountproc) {
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
    }


    fp = fopen(memfile, "r");
    if(fp) {
        while(ret != EOF) {
            ret = fscanf(fp, "%31s %lu %s\n", title, &value, units);
            if(strcmp(title, "MemTotal:") == 0) {
                total = value * 1024;
                break;
            }
        }
        fclose(fp);
    }
	if(VmRSS > 0 && total > 0) {
		return ((double)VmRSS*100)/(double)total;
	} else {
		return 0.0;
	}
}