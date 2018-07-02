/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _LOG_H_
#define _LOG_H_

#ifdef _WIN32
	#define	LOG_EMERG	0
	#define	LOG_ALERT	1
	#define	LOG_CRIT	2
	#define	LOG_ERR		3
	#define	LOG_WARNING	4
	#define	LOG_NOTICE	5
	#define	LOG_INFO	6
	#define	LOG_DEBUG	7
#else
	#include <syslog.h>
#endif

#define logprintf(a, b, ...) _logprintf(a, __FILE__, __LINE__, b, ##__VA_ARGS__)

void _logprintf(int, char *, int, const char *, ...);
void logperror(int, const char *)	;
void *logloop(void *);
void log_file_enable(void);
void log_file_disable(void);
void log_shell_enable(void);
void log_shell_disable(void);
int log_file_set(char *);
void log_level_set(int);
int log_level_get(void);
int log_gc(void);
void log_init(void);
// void logerror(const char *, ...);
void logerror(char *);

#endif
