/*
	Copyright (C) 2013 - 2015 CurlyMo

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

#ifndef _MEM_H_
#define _MEM_H_

void xfree(void);
void memtrack(void);

/*
  We only use these functions for extensive memory debugging

void *_malloc(unsigned long a, const char *file, int line);
void *_realloc(void *a, unsigned long i, const char *file, int line);
void *_calloc(unsigned long a, unsigned long b, const char *file, int line);
void _free(void *a, const char *file, int line);

#define MALLOC(a) _malloc(a, __FILE__, __LINE__)
#define REALLOC(a, b) _realloc(a, b, __FILE__, __LINE__)
#define CALLOC(a, b) _calloc(a, b, __FILE__, __LINE__)
#define FREE(a) _free((void *)(a), __FILE__, __LINE__),(a)=NULL
*/

#define MALLOC(a) malloc(a)
#define REALLOC(a, b) realloc(a, b)
#define CALLOC(a, b) calloc(a, b)
#define FREE(a) free((void *)(a)),(a)=NULL

#endif
