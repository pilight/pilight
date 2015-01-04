/*
	Copyright (C) 2013 - 2014 CurlyMo

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

#include <ifaddrs.h>

#include "../../pilight.h"

char *progname;

char *host2ip(char *host);
int isNumeric(char *str);
int nrDecimals(char *str);
int name2uid(char const *name);
int which(const char *program);
int ishex(int x);
const char *rstrstr(const char* haystack, const char* needle);
void alpha_random(char *s, const int len);
int urldecode(const char *s, char *dec);
char *urlencode(char *str);
int base64decode(unsigned char *dest, unsigned char *src, int l);
char *hostname(void);
char *distroname(void);
void rmsubstr(char *s, const char *r);
char *genuuid(char *ifname);
int whitelist_check(char *ip);
void whitelist_free(void);
int file_exists(char *fil);
int path_exists(char *fil);

#ifdef __FreeBSD__
struct sockaddr *sockaddr_dup(struct sockaddr *sa);
int rep_getifaddrs(struct ifaddrs **ifap);
void rep_freeifaddrs(struct ifaddrs *ifap);
int findproc(char *name, char *args, int loosely);
#else
pid_t findproc(char *name, char *args, int loosely);
#endif

int vercmp(char *val, char *ref);
void sfree(void **addr);
int str_replace(char *search, char *replace, char **str);
int strcicmp(char const *a, char const *b);

#endif
