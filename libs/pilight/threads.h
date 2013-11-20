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

#ifndef _THREADS_H_
#define _THREADS_H_

#include <pthread.h>

typedef struct threads_t {
	pthread_t pth;
	char *id;
	void *param;
	unsigned int running;
	void *(*function)(void *param);
	struct threads_t *next;
} threads_t;

struct threads_t *threads;

void threads_register(const char *id, void *(*function)(void* param), void *param);
void *threads_start(void *param);
int threads_gc(void);

#endif
