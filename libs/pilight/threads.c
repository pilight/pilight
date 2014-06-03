/*
	Copyright (C) 2013 CurlyMo

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

#include "threads.h"
#include "common.h"
#include "log.h"

static unsigned short thread_loop = 1;
static unsigned short thread_running = 0;

static pthread_mutex_t threadqueue_lock;
static pthread_cond_t threadqueue_signal;
static pthread_mutexattr_t threadqueue_attr;

static int threadqueue_number = 0;
static struct threadqueue_t *threadqueue = NULL;

struct threadqueue_t *threads_register(const char *id, void *(*function)(void *param), void *param, int force) {
	pthread_mutex_lock(&threadqueue_lock);

	struct threadqueue_t *tnode = malloc(sizeof(struct threadqueue_t));
	if(!tnode) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}

	struct timeval tcurrent;
	gettimeofday(&tcurrent, NULL);

	tnode->ts = 1000000 * (unsigned int)tcurrent.tv_sec + (unsigned int)tcurrent.tv_usec;
	tnode->function = function;
	tnode->running = 0;
	tnode->force = force;
	tnode->id = malloc(strlen(id)+1);
	if(!tnode->id) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(tnode->id, id);
	tnode->param = param;
	tnode->next = NULL;

	struct threadqueue_t *tmp = threadqueue;
	if(tmp) {
		while(tmp->next != NULL) {
			tmp = tmp->next;
		}
		tmp->next = tnode;
	} else {
		tnode->next = tmp;
		threadqueue = tnode;
	}
	threadqueue_number++;

	pthread_mutex_unlock(&threadqueue_lock);
	pthread_cond_signal(&threadqueue_signal);

	return tnode;
}

void threads_create(pthread_t *pth, const pthread_attr_t *attr,  void *(*start_routine) (void *), void *arg) {
	sigset_t new, old;
	sigemptyset(&new);
	sigaddset(&new, SIGINT);
	sigaddset(&new, SIGQUIT);
	sigaddset(&new, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &new, &old);
	pthread_create(pth, attr, start_routine, arg);
	pthread_sigmask(SIG_SETMASK, &old, NULL);
}

void *threads_start(void *param) {
	pthread_mutexattr_init(&threadqueue_attr);
	pthread_mutexattr_settype(&threadqueue_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&threadqueue_lock, &threadqueue_attr);
	pthread_cond_init(&threadqueue_signal, NULL);

	struct threadqueue_t *tmp_threads = NULL;

	pthread_mutex_lock(&threadqueue_lock);
	while(thread_loop) {
		if(threadqueue_number > 0) {
			pthread_mutex_lock(&threadqueue_lock);
			tmp_threads = threadqueue;
			while(tmp_threads) {
				if(tmp_threads->running == 0) {
					break;
				}
				tmp_threads = tmp_threads->next;
			}
			threads_create(&tmp_threads->pth, NULL, tmp_threads->function, (void *)tmp_threads->param);
			thread_running++;
			tmp_threads->running = 1;
			if(thread_running == 1) {
				logprintf(LOG_DEBUG, "new thread %s, %d thread running", tmp_threads->id, thread_running);
			} else {
				logprintf(LOG_DEBUG, "new thread %s, %d threads running", tmp_threads->id, thread_running);
			}

			threadqueue_number--;
			pthread_mutex_unlock(&threadqueue_lock);
		} else {
			pthread_cond_wait(&threadqueue_signal, &threadqueue_lock);
		}
	}
	return (void *)NULL;
}

void thread_stop(struct threadqueue_t *node) {
	struct threadqueue_t *currP, *prevP;

	prevP = NULL;

	for(currP = threadqueue; currP != NULL; prevP = currP, currP = currP->next) {

		if(currP->ts == node->ts) {
			if(prevP == NULL) {
				threadqueue = currP->next;
			} else {
				prevP->next = currP->next;
			}

			if(currP->running == 1) {
				thread_running--;
				logprintf(LOG_DEBUG, "stopping thread %s", currP->id);
				if(currP->force == 1) {
					pthread_cancel(currP->pth);
				}
				pthread_join(currP->pth, NULL);
				if(thread_running == 1) {
					logprintf(LOG_DEBUG, "stopped thread %s, %d thread running", currP->id, thread_running);
				} else {
					logprintf(LOG_DEBUG, "stopped thread %s, %d threads running", currP->id, thread_running);
				}
			}

			sfree((void *)&currP->id);
			sfree((void *)&currP);

			break;
		}
	}
}

void threads_cpu_usage(void) {
	logprintf(LOG_ERR, "----- Thread Profiling -----");
	struct threadqueue_t *tmp_threads = threadqueue;
	while(tmp_threads) {
		getThreadCPUUsage(&tmp_threads->pth, &tmp_threads->cpu_usage);
		if(tmp_threads->cpu_usage.cpu_per > 0) {
			if(tmp_threads->cpu_usage.cpu_per > 100) {
				logprintf(LOG_ERR, "- thread %s: 99%%", tmp_threads->id);
			} else {
				logprintf(LOG_ERR, "- thread %s: %f%%", tmp_threads->id, tmp_threads->cpu_usage.cpu_per);
			}
		} else {
			logprintf(LOG_ERR, "- thread %s: 0.000000%%", tmp_threads->id);
		}
		tmp_threads = tmp_threads->next;
	}
	logprintf(LOG_ERR, "----- Thread Profiling -----");
}

int threads_gc(void) {
	thread_loop = 0;

	pthread_mutex_unlock(&threadqueue_lock);
	pthread_cond_signal(&threadqueue_signal);

	struct threadqueue_t *tmp_threads = threadqueue;
	while(tmp_threads) {
		if(tmp_threads->running == 1) {
			tmp_threads->running = 0;
			thread_running--;

			logprintf(LOG_DEBUG, "stopping %s thread", tmp_threads->id);
			if(tmp_threads->force == 1) {
				pthread_cancel(tmp_threads->pth);
			}
			pthread_join(tmp_threads->pth, NULL);
			if(thread_running == 1) {
				logprintf(LOG_DEBUG, "stopped thread %s, %d thread running", tmp_threads->id, thread_running);
			} else {
				logprintf(LOG_DEBUG, "stopped thread %s, %d threads running", tmp_threads->id, thread_running);
			}
		}
		usleep(10000);
		tmp_threads = tmp_threads->next;
	}

	struct threadqueue_t *ttmp = NULL;
	while(threadqueue) {
		ttmp = threadqueue;
		sfree((void *)&ttmp->id);
		threadqueue = threadqueue->next;
		sfree((void *)&ttmp);
	}
	sfree((void *)&threadqueue);

	logprintf(LOG_DEBUG, "garbage collected threads library");
	return EXIT_SUCCESS;
}
