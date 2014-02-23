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
#include <unistd.h>
#include <pthread.h>

#include "threads.h"
#include "common.h"
#include "log.h"

unsigned short thread_loop = 1;
unsigned short thread_running = 0;

pthread_mutex_t threadqueue_lock;
pthread_cond_t threadqueue_signal;
pthread_mutexattr_t threadqueue_attr;

int threadqueue_number = 0;

struct threadqueue_t {
	pthread_t pth;
	int force;
	char *id;
	void *param;
	unsigned int running;
	void *(*function)(void *param);
	struct threadqueue_t *next;
} threadqueue_t;

struct threadqueue_t *threadqueue = NULL;
struct threadqueue_t *threadqueue_head = NULL;

void threads_register(const char *id, void *(*function)(void *param), void *param, int force) {
	pthread_mutex_lock(&threadqueue_lock);

	struct threadqueue_t *tnode = malloc(sizeof(struct threadqueue_t));
	if(!tnode) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
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
}

void *threads_start(void *param) {
	pthread_mutexattr_init(&threadqueue_attr);
	pthread_mutexattr_settype(&threadqueue_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&threadqueue_lock, &threadqueue_attr);

	struct threadqueue_t *tmp_threads = NULL;
	
	pthread_mutex_lock(&threadqueue_lock);	
	while(thread_loop) {
#ifdef __FreeBSD__
		pthread_mutex_lock(&threadqueue_lock);
#endif
		if(threadqueue_number > 0) {
			pthread_mutex_lock(&threadqueue_lock);			
			tmp_threads = threadqueue;
			while(tmp_threads) {
				if(tmp_threads->running == 0) {
					break;
				}
				tmp_threads = tmp_threads->next;
			}
			pthread_create(&tmp_threads->pth, NULL, tmp_threads->function, (void *)tmp_threads->param);
			tmp_threads->running = 1;		
			if(thread_running == 1) {
				logprintf(LOG_DEBUG, "new thread %s, %d thread running", tmp_threads->id, thread_running);
			} else {
				logprintf(LOG_DEBUG, "new thread %s, %d threads running", tmp_threads->id, thread_running);
			}
			thread_running++;

			threadqueue_number--;
			pthread_mutex_unlock(&threadqueue_lock);
		} else {
			pthread_cond_wait(&threadqueue_signal, &threadqueue_lock);
		}
	}
	return (void *)NULL;
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
