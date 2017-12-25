/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>

#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/proc.h"

#include "alltests.h"

pthread_t pth[2];
double cpu[12];
int done[2];

static void do_primes(int maxprimes, int sleep) {
	unsigned long i, num, primes = 0;
	for(num = 1; num <= maxprimes; ++num) {
		for(i = 2; (i <= num) && (num % i != 0); ++i);
		if(i == num) {
			++primes;
		}
		if(sleep == 1) {
			usleep(1);
		}
	}
}

static void *thread1(void *param) {
	struct cpu_usage_t cpu_usage;
	memset(&cpu_usage, 0,  sizeof(struct cpu_usage_t));
	getThreadCPUUsage(pthread_self(), &cpu_usage);
	cpu[0] = cpu_usage.cpu_per;
	do_primes(35000, 1);
	getThreadCPUUsage(pthread_self(), &cpu_usage);
	cpu[1] = cpu_usage.cpu_per;
	done[0] = 1;
	return NULL;
}

static void *thread2(void *param) {
	struct cpu_usage_t cpu_usage;
	memset(&cpu_usage, 0,  sizeof(struct cpu_usage_t));
	getThreadCPUUsage(pthread_self(), &cpu_usage);
	cpu[2] = cpu_usage.cpu_per;
	do_primes(25000, 1);
	getThreadCPUUsage(pthread_self(), &cpu_usage);
	cpu[3] = cpu_usage.cpu_per;
	done[1] = 1;
	return NULL;
}

static void test_proc(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();

	memset(&cpu, 0, 12 * sizeof(double));
	memset(&done, 0, 2  * sizeof(int));
	
	{
		printf("[ - %-46s ]\n", "calculating 50000 primes");
		fflush(stdout);

		struct cpu_usage_t cpu_usage;
		memset(&cpu_usage, 0, sizeof(struct cpu_usage_t));
		getThreadCPUUsage(pthread_self(), &cpu_usage);
		cpu[0] = getCPUUsage();
		do_primes(50000, 0);
		cpu[1] = getCPUUsage();
		CuAssertTrue(tc, cpu[0] < cpu[1]);
	}

	{
		printf("[ - %-46s ]\n", "now 35000 and 25000 primes in two threads");
		fflush(stdout);

		memset(&cpu, 0, 12 * sizeof(double));
		pthread_create(&pth[0], NULL, thread1, NULL);
		pthread_create(&pth[1], NULL, thread2, NULL);
		while(done[0] == 0 || done[1] == 0) {
			usleep(1000);
		}
		pthread_join(pth[0], NULL);
		pthread_join(pth[1], NULL);
		CuAssertTrue(tc,
			(cpu[0] < cpu[1]) &&
			(cpu[2] < cpu[3]) &&
			(cpu[3] < cpu[1])
		);
	}
	
	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_proc(void) {	
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_proc);

	return suite;
}