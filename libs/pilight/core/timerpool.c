/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#ifndef _WIN32
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
#endif
#define __USE_UNIX98
#include <pthread.h>
#include <sys/time.h>

#include "pilight.h"
#include "log.h"
#include "mem.h"
#include "eventpool.h"
#include "timerpool.h"
#include "threadpool.h"
#include "semaphore.h"

static pthread_t pth;
static pthread_mutex_t pthlock;
static pthread_cond_t pthsignal;
static pthread_mutexattr_t pthattr;
static struct timers_t *gtimer = NULL;
static volatile int loop = 0;
static sem_t pthrunning;
static volatile int seminited = 0;
static volatile int pthinit = 0;

int timer_tasks_top(struct timers_t *node, struct timer_tasks_t *el) {
	pthread_mutex_lock(&node->lock);
	if(node->nrtasks == 0) {
		pthread_mutex_unlock(&node->lock);
		return -1;
	} else {
		memcpy(el, node->tasks[1], sizeof(struct timer_tasks_t));
		if((el->name = MALLOC(strlen(node->tasks[1]->name)+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(el->name, node->tasks[1]->name);
		pthread_mutex_unlock(&node->lock);
		return 0;
	}
}

int timer_tasks_pop(struct timers_t *node, struct timer_tasks_t *el) {
	pthread_mutex_lock(&node->lock);
	if(node->nrtasks == 0) {
		pthread_mutex_unlock(&node->lock);
		return -1;
	} else {
		memcpy(el, node->tasks[1], sizeof(struct timer_tasks_t));
		if((el->name = MALLOC(strlen(node->tasks[1]->name)+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(el->name, node->tasks[1]->name);
		FREE(node->tasks[1]->name);
		FREE(node->tasks[1]);
		node->tasks[1] = node->tasks[node->nrtasks];
		node->tasks[node->nrtasks] = NULL;

		int a = 1; // parent;
		int b = a*2; // left child;

		node->nrtasks--;
		while(b <= node->nrtasks) {
			if(b < node->nrtasks &&
				((node->tasks[b]->sec > node->tasks[b+1]->sec) ||
				(node->tasks[b]->sec == node->tasks[b+1]->sec && node->tasks[b]->nsec > node->tasks[b+1]->nsec))) {
				b++;
			}
			if((node->tasks[a]->sec > node->tasks[b]->sec) ||
				(node->tasks[a]->sec == node->tasks[b]->sec && node->tasks[a]->nsec > node->tasks[b]->nsec)) {
				struct timer_tasks_t *tmp = node->tasks[a];
				node->tasks[a] = node->tasks[b];
				node->tasks[b] = tmp;
			} else {
				break;
			}
			a = b;
			b = a*2;
		}
	}
	pthread_mutex_unlock(&node->lock);
	return 0;
}

static void timer_update(struct timers_t *node, struct timer_tasks_t *e) {
	struct itimerspec timerspec;
	struct timespec ts;

	memset(&ts, 0, sizeof(struct timespec));

	int a = node->sec;
	int b = node->nsec;

	if((a == e->sec && b > e->nsec) || (a > e->sec) || (a == 0 && b == 0)) {
		node->sec = e->sec;
		node->nsec = e->nsec;
		timerspec.it_interval.tv_sec = 0 / 1000000000;
		timerspec.it_interval.tv_nsec = 0 % 1000000000;
		timerspec.it_value.tv_sec = e->sec;
		timerspec.it_value.tv_nsec = e->nsec;

		timer_settime(node->timer, TIMER_ABSTIME, &timerspec, NULL);
	}
}

void *timer_thread(void *param) {
	struct timer_tasks_t e;

	sem_wait(&pthrunning);

	while(loop == 1) {
		pthread_mutex_lock(&pthlock);
		while(loop == 1 && gtimer == NULL) {
			pthread_cond_wait(&pthsignal, &pthlock);
		}
		if(loop == 0) {
			pthread_mutex_unlock(&pthlock);
			break;
		}
		if(timer_tasks_pop(gtimer, &e) == 0) {
			// double check if we are executing the correct task
			if(e.sec == gtimer->sec && e.nsec == gtimer->nsec) {
				gtimer->func(&e);
			}
			FREE(e.name);
		}

		gtimer->sec = 0;
		gtimer->nsec = 0;

		if(timer_tasks_top(gtimer, &e) == 0) {
			timer_update(gtimer, &e);
			FREE(e.name);
		}
		gtimer = NULL;
		pthread_mutex_unlock(&pthlock);
	}

	sem_post(&pthrunning);
	return NULL;
}

void timer_handler(int a, siginfo_t *sig, void *c) {
	struct timers_t *timer = sig->si_value.sival_ptr;

	pthread_mutex_lock(&pthlock);
	gtimer = timer;
	pthread_cond_signal(&pthsignal);
	pthread_mutex_unlock(&pthlock);
}

int timer_init(struct timers_t *node, int signo, void *(*func)(struct timer_tasks_t *), int type, struct timeval expire, struct timeval interval) {
	struct sigevent event;
	struct itimerspec timerspec;
	struct sigaction sa;
	sigset_t mask;

	node->hsize = 64;
	if((node->tasks = MALLOC(sizeof(struct timer_tasks_t *)*node->hsize)) == 0) {
		OUT_OF_MEMORY
	}
	memset(node->tasks, 0, node->hsize*sizeof(struct timer_tasks_t *));
	node->nrtasks = 0;

	pthread_mutexattr_init(&node->attr);
	pthread_mutexattr_settype(&node->attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&node->lock, &node->attr);

	memset(&node->timer, 0, sizeof(timer_t));
	memset(&timerspec, 0, sizeof(struct itimerspec));
	memset(&mask, 0, sizeof(sigset_t));
	memset(&sa, 0, sizeof(struct sigaction));

	node->nsec = 0;
	node->sec = 0;

	sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = timer_handler;
  sigemptyset(&sa.sa_mask);
	if(sigaction(signo, &sa, NULL) == -1) {
		return -1;
	}

	sigemptyset(&mask);
	sigaddset(&mask, signo);
	if(sigprocmask(SIG_SETMASK, &mask, NULL) == -1) {
		return -1;
	}
	event.sigev_notify = SIGEV_SIGNAL;
	event.sigev_signo = signo;
	event.sigev_value.sival_ptr = node;

	if(type == TIMER_ABSTIME) {
		timer_create(CLOCK_REALTIME, &event, &node->timer);
	} else {
		timer_create(CLOCK_MONOTONIC, &event, &node->timer);
	}
	timerspec.it_interval.tv_sec = interval.tv_sec;
	timerspec.it_interval.tv_nsec = (interval.tv_usec*1000);
	timerspec.it_value.tv_sec = expire.tv_sec;
	timerspec.it_value.tv_nsec = (expire.tv_usec * 1000);
	timer_settime(node->timer, type, &timerspec, NULL);

	if(sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) {
		timer_delete(node->timer);
		return -1;
	}
	node->func = func;
	node->init = 1;
	return 0;
}

void timer_add_task(struct timers_t *timer, char *name, struct timeval wait, void *(*func)(void *), void *userdata) {
	struct timespec ts;

	wait.tv_sec += (wait.tv_usec / 1000000);
	wait.tv_usec = wait.tv_usec % 1000000;

	clock_gettime(CLOCK_REALTIME, &ts);

	ts.tv_sec += wait.tv_sec;
	ts.tv_nsec += wait.tv_usec*1000;

	ts.tv_sec += (ts.tv_nsec / 1000000000);
	ts.tv_nsec = ts.tv_nsec % 1000000000;

	struct timer_tasks_t *node = MALLOC(sizeof(struct timer_tasks_t));
	if(node == NULL) {
		OUT_OF_MEMORY
	}
	if((node->name = MALLOC(strlen(name)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(node->name, name);
	node->sec = ts.tv_sec;
	node->nsec = ts.tv_nsec;
	node->func = func;
	node->userdata = userdata;
	node->runs = 0;
	node->wait.tv_sec = wait.tv_sec;
	node->wait.tv_nsec = (wait.tv_usec*1000);

	pthread_mutex_lock(&timer->lock);
	if(timer->nrtasks >= timer->hsize) {
		if((timer->tasks = REALLOC(timer->tasks, sizeof(struct timer_tasks_t *)*(timer->hsize*2))) == NULL) {
			OUT_OF_MEMORY
		}
		memset(timer->tasks+timer->hsize, 0, timer->hsize*sizeof(struct timer_tasks_t *));
		timer->hsize *= 2;
	}

	timer->tasks[timer->nrtasks+1] = node;
	int i = (timer->nrtasks+1)/2; // parent
	int x = timer->nrtasks+1; // child
	while(i > 0) {
		struct timer_tasks_t *tmp = timer->tasks[i];
		if((timer->tasks[x]->sec < tmp->sec) ||
			(timer->tasks[x]->sec == tmp->sec && timer->tasks[x]->nsec < tmp->nsec)) {
			timer->tasks[i] = timer->tasks[x];
			timer->tasks[x] = tmp;
		}
		x = i; // parent becomes child
		i /= 2; // new parent
	}

	timer->nrtasks++;

	if(pilight.debuglevel >= 2) {
		for(i=1;i<=timer->nrtasks;i++) {
			fprintf(stderr, "%s %lu %lu\n", timer->tasks[i]->name, timer->tasks[i]->sec, timer->tasks[i]->nsec);
		}
	}
	pthread_mutex_unlock(&timer->lock);

	int a = timer->sec;
	int b = timer->nsec;

	/* Only update timer if the latest tasks should be executed first */
	if((a == ts.tv_sec && b > ts.tv_nsec) || (a > ts.tv_sec) || (a == 0 && b == 0)) {
		struct timer_tasks_t e;
		if(timer_tasks_top(timer, &e) == 0) {
			timer_update(timer, &e);
			FREE(e.name);
		}
	}
}

void timer_thread_start(void) {
	sem_init(&pthrunning, 0, 1);

	seminited = 1;
	loop = 1;

	pthread_mutexattr_init(&pthattr);
	pthread_mutexattr_settype(&pthattr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&pthlock, &pthattr);
	pthread_cond_init(&pthsignal, NULL);

#ifndef _WIN32
	sigset_t new, old;
	sigemptyset(&new);
	sigaddset(&new, SIGINT);
	sigaddset(&new, SIGQUIT);
	sigaddset(&new, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &new, &old);
#endif
	pthread_create(&pth, NULL, timer_thread, NULL);
	pthinit = 1;
#ifndef _WIN32
	pthread_sigmask(SIG_SETMASK, &old, NULL);
#endif
}

void timer_thread_gc(void) {
	loop = 0;
	pthread_mutex_lock(&pthlock);
	gtimer = NULL;
	pthread_cond_signal(&pthsignal);
	pthread_mutex_unlock(&pthlock);
	if(seminited == 1) {
		sem_wait(&pthrunning);
		sem_destroy(&pthrunning);
		seminited = 0;
	}
	if(pthinit == 1) {
		pthread_join(pth, NULL);
	}
}

void timer_gc(struct timers_t *timer) {
	pthread_mutex_lock(&timer->lock);
	int i = 0;
	for(i=1;i<=timer->nrtasks;i++) {
		FREE(timer->tasks[i]->name);
		FREE(timer->tasks[i]);
	}
	timer->nrtasks = 0;
	FREE(timer->tasks);
	if(timer->init == 1) {
		timer->init = 0;
		timer_delete(timer->timer);
	}
	pthread_mutex_unlock(&timer->lock);
	return;
}
