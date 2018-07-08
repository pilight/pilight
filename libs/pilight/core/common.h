/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _COMMON_H_
#define _COMMON_H_

#ifndef _WIN32
	#include <sys/types.h>
	#include <ifaddrs.h>
	#include <sys/socket.h>
	#include <pthread.h>
#endif
#include <stdint.h>

typedef struct varcont_t {
	union {
		char *string_;
		double number_;
		int bool_;
		void *void_;
	};
	int decimals_;
	int type_;
	int free_;
} varcont_t;

#include "pilight.h"

extern char *progname;

#ifdef _WIN32
  #define snprintf _snprintf
  #define vsnprintf _vsnprintf
  #define strcasecmp _stricmp
  #define strncasecmp _strnicmp

	void usleep(unsigned long value);
#define sleep(a) Sleep(a*1000)
int gettimeofday(struct timeval *tp, struct timezone *tzp);
int check_instances(const wchar_t *prog);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
int isrunning(const char *program);
#endif

int getnrcpu(void);
void array_free(char ***array, int len);
int isrunning(const char *program);
void atomicinit(void);
void atomiclock(void);
void atomicunlock(void);
unsigned int explode(char *str, const char *delimiter, char ***output);
int isNumeric(char *str);
int nrDecimals(char *str);
int name2uid(char const *name);
int which(const char *program);
int ishex(int x);
const char *rstrstr(const char* haystack, const char* needle);
void alpha_random(char *s, const int len);
int urldecode(const char *s, char *dec);
char *urlencode(char *str);
char *base64encode(char *src, size_t len);
char *base64decode(char *src, size_t len, size_t *decsize);
char *hostname(void);
char *distroname(void);
void rmsubstr(char *s, const char *r);
char *genuuid(char *ifname);
int file_exists(char *fil);
int path_exists(char *fil);
char *uniq_space(char *str);

#if defined(__FreeBSD__) || defined(_WIN32)
int findproc(char *name, char *args, int loosely);
#else
pid_t findproc(char *name, char *args, int loosely);
#endif

int vercmp(char *val, char *ref);
int str_replace(char *search, char *replace, char **str);
#ifndef _WIN32
int stricmp(char const *a, char const *b);
int strnicmp(char const *a, char const *b, size_t len);
#endif
int file_get_contents(char *file, char **content);
void calc_time_interval(int type, int seconds, int diff, struct timeval *tv);
char *_dirname(char *path);

#endif
