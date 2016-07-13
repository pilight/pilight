/*
	Copyright (C) 2015 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#include <semaphore.h>
#include "eventpool.h"
#include "proc.h"

struct threadpool_tasks_t {
	unsigned long id;
	char *name;
	void *(*func)(void *);
	void *(*free)(void *);
	sem_t *ref;
	int priority;
	int reason;

	struct {
		struct timespec first;
		struct timespec second;
	}	timestamp;

	void *userdata;

	struct threadpool_tasks_t *next;
} threadpool_tasks_t;

struct threadpool_workers_t {
	int nr;
	unsigned long id;
	int loop;
	sem_t running;
	int working;
	int haswork;
	struct threadpool_tasks_t task;
	struct cpu_usage_t cpu_usage;

	pthread_t pth;
	pthread_mutex_t lock;
	pthread_mutexattr_t attr;
#ifdef _WIN32
	HANDLE signal;
#else
	pthread_cond_t signal;
#endif

	struct threadpool_workers_t *next;
} threadpool_workers_t;

int threadpool_free_runs(int);
void threadpool_remove_task(unsigned long);
void threadpool_add_worker(void);
unsigned long threadpool_add_work(int, sem_t *, char *, int, void *(*)(void *), void *(*)(void *), void *);
void threadpool_init(int, int, int);
void threadpool_reinit(void);
void threadpool_gc(void);
unsigned long threadpool_add_scheduled_work(char *, void *(*)(void *), struct timeval, void *);
void threadpool_work_interval(unsigned long, struct timeval);
void threadpool_work_stop(unsigned long);
void *threadpool_delegate(void *);

#endif
