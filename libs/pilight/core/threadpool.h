/*
	Copyright (C) 2015 CurlyMo

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

#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#include <semaphore.h>
#include "eventpool.h"

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

	pthread_t pth;
	pthread_mutex_t lock;
	pthread_mutexattr_t attr;
	pthread_cond_t signal;

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
void *threadpool_delegate(void *param);

#endif
