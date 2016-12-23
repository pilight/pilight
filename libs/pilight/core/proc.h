/*
	Copyright (C) 2014 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _PROC_H_
#define _PROC_H_

#include <time.h>

/* CPU usage */
typedef struct cpu_usage_t {
	double sec_start;
	double sec_stop;
	double sec_diff;
	double cpu_old;
	double cpu_new;
	double cpu_per;
	struct timespec ts;
	clock_t starts;
} cpu_usage_t;

int proc_gc(void);
double getCPUUsage(void);
double getRAMUsage(void);
// void getThreadCPUUsage(pthread_t pth, struct cpu_usage_t *cpu_usage);

#endif
