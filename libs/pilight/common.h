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
#include <ifaddrs.h>

#include "../../pilight.h"

char *progname;
int filelog;
int shelllog;
int loglevel;
char debug_log[128];

void logmarkup(void);
int isNumeric(char *str);
uid_t name2uid(char const *name);
int which(const char *program);
int ishex(int x);
const char *rstrstr(const char* haystack, const char* needle);
void alpha_random(char *s, const int len);
int urldecode(const char *s, char *dec);
int base64decode(unsigned char *dest, unsigned char *src, int l);
char *hostname(void);
char *distroname(void);
void rmsubstr(char *s, const char *r);
char *genuuid(char *ifname);
int whitelist_check(char *ip);
void whitelist_free(void);
int path_exists(char *fil);
#ifdef __FreeBSD__
static struct sockaddr *sockaddr_dup(struct sockaddr *sa);
int rep_getifaddrs(struct ifaddrs **ifap);
int proc_find(const char *name);
#else
pid_t proc_find(const char *name);
#endif

#if defined(DEBUG) && !defined(__FreeBSD)

void debug_free(void **addr, const char *file, int line);
const char *debug_filename(const char *file);
void *debug_malloc(size_t len, const char *file, int line);
void *debug_realloc(void *addr, size_t len, const char *file, int line);
void *debug_calloc(size_t nlen, size_t elen, const char *file, int line);

#define sfree(x) debug_free(x, __FILE__, __LINE__);
#define malloc(x) debug_malloc(x, __FILE__, __LINE__);
#define realloc(y, x) debug_realloc(y, x, __FILE__, __LINE__);
#define calloc(y, x) debug_calloc(y, x, __FILE__, __LINE__);

#else

void sfree(void **addr);

#endif

#endif
