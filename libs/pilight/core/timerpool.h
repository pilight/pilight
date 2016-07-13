/*
	Copyright (C) 2015 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _TIMERPOOL_H_
#define _TIMERPOOL_H_

#ifdef _WIN32
	#if _WIN32_WINNT < 0x0501
		#undef _WIN32_WINNT
		#define _WIN32_WINNT 0x0501
	#endif
	#define WIN32_LEAN_AND_MEAN
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#define MSG_NOSIGNAL 0
#else
	#include <netinet/in.h>
#endif
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
#ifdef _WIN32
	HANDLE timer;
	HANDLE queue;
	int active;
#else
	timer_t timer;
#endif
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
