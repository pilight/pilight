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

#ifndef _TIMERPOOL_H_
#define _TIMERPOOL_H_

#include <netinet/in.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>

#include "eventpool.h"

struct timer_tasks_t {
	char *name;
	void *userdata;
	void *(*func)(void *);
	unsigned long sec;
	unsigned long nsec;
	int repeats;
	int runs;
	struct timespec wait;
} timer_tasks_t;

struct timers_t {
	int init;
	timer_t timer;
	unsigned long sec;
	unsigned long nsec;
	pthread_mutex_t lock;
	pthread_mutexattr_t attr;
	struct timer_tasks_t **tasks;
	int nrtasks;
	int hsize;
	void *(*func)(struct timer_tasks_t *);
} timers_t;

void timer_thread_start(void);
void timer_thread_gc(void);
void timer_add_task(struct timers_t *, char *, struct timeval, void *(*)(void *), void *);
int timer_tasks_top(struct timers_t *, struct timer_tasks_t *);
int timer_tasks_pop(struct timers_t *, struct timer_tasks_t *);
int timer_init(struct timers_t *, int, void *(*)(struct timer_tasks_t *), int, struct timeval, struct timeval);
void timer_gc(struct timers_t *timer);

#endif
