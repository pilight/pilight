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

#include "threads.h"
#include "log.h"

unsigned short thread_loop = 1;

void threads_register(void *(*function)(void *param), void *param) {
	struct threads_t *tnode = malloc(sizeof(struct threads_t));

	tnode->function = function;
	tnode->running = 0;
	tnode->param = param;
	tnode->next = threads;
	threads = tnode;
}

void *threads_start(void *param) {
	while(thread_loop) {
		struct threads_t *tmp_threads = threads;
		while(tmp_threads) {
			if(tmp_threads->running == 0) {
				pthread_create(&tmp_threads->pth, NULL, tmp_threads->function, (void *)tmp_threads->param);
				tmp_threads->running = 1;
			}
			tmp_threads = tmp_threads->next;
		}
		usleep(10);
	}
	return (void *)NULL;
}

int threads_gc(void) {
	thread_loop = 0;

	struct threads_t *tmp_threads = threads;
	while(tmp_threads) {
		if(tmp_threads->running == 1) {
			tmp_threads->running = 0;
			pthread_cancel(tmp_threads->pth);
			pthread_join(tmp_threads->pth, NULL);
		}
		tmp_threads = tmp_threads->next;
	}

	struct threads_t *ttmp;
	while(threads) {
		ttmp = threads;
		threads = threads->next;
		free(ttmp);
	}
	free(threads);

	logprintf(LOG_DEBUG, "garbage collected threads library");
	return EXIT_SUCCESS;
}

