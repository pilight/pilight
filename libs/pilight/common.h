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

#ifndef _COMMON_H_
#define _COMMON_H_

#include <syslog.h>

#include "../../pilight.h"

char *progname;
int filelog;
int shelllog;
int loglevel;
char debug_log[128];

void logmarkup(void);

#ifdef DEBUG

void debug_free(void **addr, const char *file, int line);
const char *debug_filename(const char *file);
void *debug_malloc(size_t len, const char *file, int line);
void *debug_realloc(void *addr, size_t len, const char *file, int line);

#define sfree(x) debug_free(x, __FILE__, __LINE__);
#define malloc(x) debug_malloc(x, __FILE__, __LINE__);
#define realloc(y, x) debug_realloc(y, x, __FILE__, __LINE__);

#else

void sfree(void **addr);

#endif

#endif
