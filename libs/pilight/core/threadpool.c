/*
	Copyright (C) 2015 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#ifdef _WIN32
	#include <winsock2.h>
	#include <windows.h>
	#define SIGRTMIN 34
#else
	#define __USE_UNIX98
	#include <pthread.h>
	#include <sys/time.h>
	#include <unistd.h>
#endif	

#include "gc.h"
#include "log.h"
#include "proc.h"
#include "common.h"
#include "errno.h"
#include "threadpool.h"
// #include "timerpool.h"

static struct threadpool_tasks_t *threadpool_tasks = NULL;
static struct threadpool_workers_t *threadpool_workers = NULL;

static uv_mutex_t tasks_lock;
// static pthread_mutexattr_t tasks_attr;
static uv_mutex_t workers_lock;
// static pthread_mutexattr_t workers_attr;

static int nrtasks = 0;
static int nrworkers = 0;
static int init = 0;

static int acceptwork = 1;
static int maxworkers = 0;
static int minworkers = 0;
static int maxlinger = 0;

static int nrruns[REASON_END+1][10];

// static struct timers_t timer;

typedef struct timer_list_t {
	unsigned long id;
	char name[255];
	void *(*task)(void *);
	struct timeval tv;
	void *userdata;

	struct timer_list_t *next;
} timer_list_t;

struct timer_list_t *ttasks = NULL;

static void threadpool_remove_worker(unsigned long id) {
	struct threadpool_workers_t *currP = NULL, *prevP = NULL;

	prevP = NULL;

	for(currP = threadpool_workers; currP != NULL; prevP = currP, currP = currP->next) {
		if(currP->id == id) {
			if(prevP == NULL) {
				threadpool_workers = currP->next;
			} else {
				prevP->next = currP->next;
			}
			sem_destroy(&currP->running);
			logprintf(LOG_DEBUG, "removed worker: %d", currP->nr);
				__sync_sub_and_fetch(&nrworkers, 1);
			FREE(currP);
			break;
		}
	}
}

static void *worker(void *param) {
	struct threadpool_workers_t *node = param;
	int linger = 0;

	sem_wait(&node->running);
	while(node->loop == 1) {
		uv_mutex_lock(&node->lock);

		while(node->loop == 1 && __sync_add_and_fetch(&nrtasks, 0) == 0) {
#ifdef _WIN32
			/*
			 * Somehow the pthread_cond_timedwait doesn't work well on windows
			 * when we also use the SetWaitableTimer elsewhere.
			 */
			WaitForSingleObjectEx(node->signal, 1000L, TRUE);
#else
			struct timeval now;
			struct timespec timeToWait;
			gettimeofday(&now, NULL);
			timeToWait.tv_sec = now.tv_sec+1;
			timeToWait.tv_nsec = 0;	
			pthread_cond_timedwait(&node->signal, &node->lock, &timeToWait);
#endif
			if(node->loop == 0) {
				break;
			}
			linger++;
			int l = maxlinger;

			if(linger > l) {
				uv_mutex_lock(&workers_lock);
				int mw = minworkers;
				if(nrworkers > mw) {
					pthread_detach(node->pth);
					threadpool_remove_worker(node->id);
				} else {
					linger = 0;
				}
				uv_mutex_unlock(&workers_lock);
				if(linger > 0) {
					return NULL;
				}
			}
		}
		linger = 0;
		uv_mutex_unlock(&node->lock);
		if(node->loop == 0) {
			break;
		}

		uv_mutex_lock(&tasks_lock);
		if(threadpool_tasks != NULL) {
			struct threadpool_tasks_t copy;
			memcpy(&copy, threadpool_tasks, sizeof(struct threadpool_tasks_t));
			if((copy.name = MALLOC(strlen(threadpool_tasks->name)+1)) == NULL) {
				OUT_OF_MEMORY
			}
			strcpy(copy.name, threadpool_tasks->name);
			struct threadpool_tasks_t *tmp = threadpool_tasks;
			threadpool_tasks = threadpool_tasks->next;
			__sync_add_and_fetch(&nrtasks, -1);
			FREE(tmp->name);
			FREE(tmp);
			uv_mutex_unlock(&tasks_lock);

			// if(pilight.debuglevel >= 2) {
				// fprintf(stderr, "activated worker %d, executing %s\n", node->nr, copy.name);
			// }
			// if(pilight.debuglevel >= 1) {
				// clock_gettime(CLOCK_MONOTONIC, &copy.timestamp.first);
			// }
			// if(pilight.debuglevel >= 2) {
				// // getThreadCPUUsage(node->pth, &node->cpu_usage);
			// }
			copy.func(copy.reason, &copy);

			// if(pilight.debuglevel >= 1) {
				// if(pilight.debuglevel >= 2) {
					// // getThreadCPUUsage(node->pth, &node->cpu_usage);
					// fprintf(stderr, "worker %d: %f%% CPU", node->nr, node->cpu_usage.cpu_per);
				// }
				// clock_gettime(CLOCK_MONOTONIC, &copy.timestamp.second);
				// logprintf(LOG_DEBUG, "task %s executed in %.6f seconds", copy.name,
					// ((double)copy.timestamp.second.tv_sec + 1.0e-9*copy.timestamp.second.tv_nsec) -
					// ((double)copy.timestamp.first.tv_sec + 1.0e-9*copy.timestamp.first.tv_nsec));
			// }

			if(copy.ref == NULL ||
				 (sem_trywait(copy.ref) == -1 && errno == EAGAIN) ||
				 node->loop == 0) {
				if(copy.done != NULL && copy.userdata != NULL && copy.reason != REASON_END) {
					copy.done(copy.userdata);
				}
				if(copy.ref != NULL) {
					FREE(copy.ref);
				}
			}
			FREE(copy.name);
		} else {
			uv_mutex_unlock(&tasks_lock);
		}
	}

	sem_post(&node->running);
	return NULL;
}

static int findMissingNr(int arr[], int size) {
  int i = 0;

  for(i = 0; i < size; i++) {
		if(abs(arr[i]) - 1 < size && arr[abs(arr[i]) - 1] > 0) {
			arr[abs(arr[i]) - 1] = -arr[abs(arr[i]) - 1];
		}
	}

  for(i = 0; i < size; i++) {
    if(arr[i] > 0) {
			return i+1;
		}
	}

  return size+1;
}

void threadpool_add_worker(void) {
	struct timeval tv;
	struct threadpool_workers_t *node = MALLOC(sizeof(struct threadpool_workers_t));
	if(node == NULL) {
		OUT_OF_MEMORY
	}
	memset(node, 0, sizeof(struct threadpool_workers_t));

#ifdef _WIN32
	SleepEx(1, TRUE);
#else
	usleep(1);
#endif
	gettimeofday(&tv, NULL);

	// pthread_mutexattr_init(&node->attr);
	// pthread_mutexattr_settype(&node->attr, PTHREAD_MUTEX_RECURSIVE);
	// pthread_mutex_init(&node->lock, &node->attr);
	uv_mutex_init(&node->lock);
#ifdef _WIN32
	node->signal = CreateEvent(NULL, FALSE, FALSE, NULL);
#else
	pthread_cond_init(&node->signal, NULL);
#endif

#ifndef _WIN32
	sigset_t new, old;
	sigemptyset(&new);
	sigaddset(&new, SIGINT);
	sigaddset(&new, SIGQUIT);
	sigaddset(&new, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &new, &old);
#endif
	pthread_create(&node->pth, NULL, worker, (void *)node);
#ifndef _WIN32
	pthread_sigmask(SIG_SETMASK, &old, NULL);
#endif

	node->nr = 0;
	node->id = (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;
	node->loop = 1;
	sem_init(&node->running, 0, 1);
	node->next = NULL;

	uv_mutex_lock(&workers_lock);
	struct threadpool_workers_t *tmp = threadpool_workers;
	if(tmp) {
		int mw = maxworkers;
		int *list = MALLOC(mw+1), i = 0, x = 0;
		if(list == NULL) {
			OUT_OF_MEMORY
		}
		memset(&list, 0, mw+1);
		while(tmp) {
			list[i] = tmp->nr;
			tmp = tmp->next;
			i++;
		}
		x = findMissingNr(list, i);
		FREE(list);
		tmp = threadpool_workers;
		while(tmp->next != NULL) {
			tmp = tmp->next;
		}
		if(x == -1) {
			node->nr = tmp->nr + 1;
		} else {
			node->nr = x;
		}
		tmp->next = node;
		logprintf(LOG_DEBUG, "added worker: %d", node->nr);
		node = tmp;
	} else {
		node->nr = 1;
		logprintf(LOG_DEBUG, "added worker: %d", node->nr);
		node->next = threadpool_workers;
		threadpool_workers = node;
	}

	__sync_add_and_fetch(&nrworkers, 1);
	uv_mutex_unlock(&workers_lock);
}

int threadpool_free_runs(int reason) {
	int i = 0;
	for(i=0;i<10;i++) {
		if(__sync_add_and_fetch(&nrruns[reason][i], 0) == -1) {
			return i;
		}
	}
	return -1;
}

unsigned long threadpool_add_work(int reason, uv_sem_t *ref, char *name, int priority, void *(*func)(int, void *), void *(*free)(void *), void *userdata) {
	if(acceptwork == 0 && init == 0) {
		// Adding work to uninitialized threadpool
		return -1;
	}
	if(pilight.debuglevel >= 1) {
		logprintf(LOG_DEBUG, "new threadpool task %s", name);
	}
	unsigned long id = 0;
	struct timeval tv;
	struct threadpool_tasks_t *node = MALLOC(sizeof(struct threadpool_tasks_t));
	if(node == NULL) {
		OUT_OF_MEMORY
	}

#ifdef _WIN32
	SleepEx(1, TRUE);
#else
	usleep(1);
#endif
	gettimeofday(&tv, NULL);

	if((node->name = MALLOC(strlen(name)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(node->name, name);
	node->func = func;
	node->ref = ref;
	node->done = free;
	id = node->id = (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;
	node->priority = priority;
	node->userdata = userdata;
	node->reason = reason;
	node->next = NULL;

	uv_mutex_lock(&tasks_lock);
	struct threadpool_tasks_t *tmp = threadpool_tasks;
	if(tmp != NULL) {
		while(tmp->next != NULL) {
			tmp = tmp->next;
		}
		tmp->next = node;
		node = tmp;
	} else {
		node->next = threadpool_tasks;
		threadpool_tasks = node;
	}
	__sync_add_and_fetch(&nrtasks, 1);
	uv_mutex_unlock(&tasks_lock);

	if(((double)__sync_add_and_fetch(&nrtasks, 0) / (double)__sync_add_and_fetch(&nrworkers, 0)) >= 10 && maxworkers > nrworkers) {
		threadpool_add_worker();
	}
	uv_mutex_lock(&workers_lock);
	struct threadpool_workers_t *workers = threadpool_workers;
	while(workers) {
		uv_mutex_unlock(&workers->lock);
#ifdef _WIN32
		SetEvent(workers->signal);
#else
		pthread_cond_signal(&workers->signal);
#endif
		workers = workers->next;
	}
	uv_mutex_unlock(&workers_lock);

	return id;
}

// static void *threadpool_timer_handler(struct timer_tasks_t *task) {
	// threadpool_add_work(REASON_END, NULL, task->name, 0, task->func, NULL, task->userdata);
	// return NULL;
// }

void threadpool_init(int min, int max, int linger) {
	int i = 0;
	struct timeval tv1, tv2;
	tv1.tv_sec = tv2.tv_sec = 0;
	tv1.tv_usec = tv2.tv_usec = 0;

	for(i=0;i<=REASON_END;i++) {
		int x = 0;
		for(x=0;x<10;x++) {
			nrruns[i][x] = -1;
		}
	}

	// timer_init(&timer, SIGRTMIN, threadpool_timer_handler, TIMER_ABSTIME, tv1, tv2);

	maxworkers = max;
	minworkers = min;
	maxlinger = linger;
	// __sync_add_and_fetch(&acceptwork, 1);

	// pthread_mutexattr_init(&tasks_attr);
	// pthread_mutexattr_settype(&tasks_attr, PTHREAD_MUTEX_RECURSIVE);
	// pthread_mutex_init(&tasks_lock, &tasks_attr);
	uv_mutex_init(&tasks_lock);

	// pthread_mutexattr_init(&workers_attr);
	// pthread_mutexattr_settype(&workers_attr, PTHREAD_MUTEX_RECURSIVE);
	// pthread_mutex_init(&workers_lock, &workers_attr);
	uv_mutex_init(&workers_lock);

	for(i=0;i<min;i++) {
		threadpool_add_worker();
	}
	init = 1;
	// while(ttasks) {
		// struct timer_list_t *tmp = ttasks;
		// timer_add_task(&timer, tmp->name, tmp->tv, tmp->task, tmp->userdata);
		// ttasks = ttasks->next;
		// FREE(tmp);
	// }
}

unsigned long threadpool_add_scheduled_work(char *name, void *(*task)(void *), struct timeval tv, void *userdata) {
	if(acceptwork == 0) {
		// Adding work to uninitialized threadpool
		return -1;
	}

	if(init == 0) {
		/*
		 * We need to buffer any scheduled task until we are forked
		 * and threadpool_init is run.
		 */
		// struct timer_list_t *node = MALLOC(sizeof(struct timer_list_t));
		// if(node == NULL) {
			// OUT_OF_MEMORY
		// }

		// node->id = 0;
		// strncpy(node->name, name, 255);
		// node->name[254] = '\0';
		// node->task = task;
		// memcpy(&node->tv, &tv, sizeof(struct timeval));
		// node->userdata = userdata;

		// node->next = ttasks;
		// ttasks = node;
		return 0;
	} else {
		// timer_add_task(&timer, name, tv, task, userdata);
		return 0;
	}
}

static void threadpool_workers_gc(void) {
	uv_mutex_lock(&workers_lock);
	struct threadpool_workers_t *tmp = threadpool_workers;
	while(threadpool_workers != NULL) {
		tmp = threadpool_workers;
		threadpool_workers->loop = 0;
		uv_mutex_unlock(&threadpool_workers->lock);
#ifdef _WIN32
		SetEvent(threadpool_workers->signal);
#else
		pthread_cond_signal(&threadpool_workers->signal);
#endif
		sem_wait(&threadpool_workers->running);
		pthread_join(threadpool_workers->pth, NULL);
		sem_destroy(&threadpool_workers->running);
		threadpool_workers = threadpool_workers->next;
		FREE(tmp);
	}
	nrworkers = 0;
	uv_mutex_unlock(&workers_lock);
}

static void threadpool_tasks_gc(void) {
	uv_mutex_lock(&tasks_lock);
	struct threadpool_tasks_t *tmp1 = threadpool_tasks;
	while(threadpool_tasks != NULL) {
		tmp1 = threadpool_tasks;
		if(tmp1->done != NULL && tmp1->userdata != NULL && tmp1->reason != REASON_END) {
			tmp1->done(tmp1->userdata);
		}
		if(tmp1->ref != NULL) {
			sem_destroy(tmp1->ref);
		}
		FREE(tmp1->name);
		threadpool_tasks = threadpool_tasks->next;
		FREE(tmp1);
	}
	nrtasks = 0;
	uv_mutex_unlock(&tasks_lock);
}

void threadpool_gc() {
	acceptwork = 0;
	threadpool_workers_gc();
	threadpool_tasks_gc();

	// timer_gc(&timer);
	init = 0;
}
