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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <syslog.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <pthread.h>

#include "../../pilight.h"
#include "common.h"
#include "gc.h"
#include "log.h"

struct logqueue_t {
	char *line;
	struct logqueue_t *next;
} logqueue_t;

static pthread_mutex_t logqueue_lock;
static pthread_cond_t logqueue_signal;
static pthread_mutexattr_t logqueue_attr;

static struct logqueue_t *logqueue;
static struct logqueue_t *logqueue_head;
static unsigned int logqueue_number = 0;
static unsigned int loop = 1;
static unsigned int stop = 0;
static unsigned int pthinitialized = 0;

static char *logfile = NULL;
static char *logpath = NULL;
static int filelog = 1;
static int shelllog = 0;
static int loglevel = LOG_DEBUG;

int log_gc(void) {
	if(shelllog == 1) {
		fprintf(stderr, "DEBUG: garbage collected log library\n");
	}

	loop = 0;
	stop = 1;

	pthread_mutex_unlock(&logqueue_lock);
	pthread_cond_signal(&logqueue_signal);

	while(logqueue_number > 0) {
		usleep(10);
	}

	if(logfile != NULL) {
		FREE(logfile);
	}
	if(logpath != NULL) {
		FREE(logpath);
	}
	return 1;
}

void logprintf(int prio, const char *format_str, ...) {
	if(stop == 0) {	
		struct timeval tv;
		struct tm *tm = NULL;
		va_list ap, apcpy;
		char fmt[64], buf[64], *line = MALLOC(128), nul;
		int save_errno = -1;
		size_t bytes = 0;

		if(line == NULL) {
			fprintf(stderr, "out of memory");
			exit(EXIT_FAILURE);
		}
		
		pthread_mutex_lock(&logqueue_lock);
		save_errno = errno;

		memset(line, '\0', 128);
		if(loglevel >= prio) {
			gettimeofday(&tv, NULL);
			if((tm = localtime(&tv.tv_sec)) != NULL) {
				strftime(fmt, sizeof(fmt), "%b %d %H:%M:%S", tm);
				snprintf(buf, sizeof(buf), "%s:%03u", fmt, (unsigned int)tv.tv_usec);
			}
			sprintf(line, "[%22.22s] %s: ", buf, progname);

			switch(prio) {
				case LOG_WARNING:
					strcat(line, "WARNING: ");
					break;
				case LOG_ERR:
					strcat(line, "ERROR: ");
					break;
				case LOG_INFO:
					strcat(line, "INFO: ");
					break;
				case LOG_NOTICE:
					strcat(line, "NOTICE: ");
					break;
				case LOG_DEBUG:
					strcat(line, "DEBUG: ");
					break;
				case LOG_STACK:
					strcat(line, "STACK: ");
					break;
				default:
				break;
			}
			va_copy(apcpy, ap);
			va_start(apcpy, format_str);
			bytes = vsnprintf(&nul, 1, format_str, apcpy);
			va_end(apcpy);
			
			if((line = REALLOC(line, bytes+strlen(line)+3)) == NULL) {
				fprintf(stderr, "out of memory");
				exit(EXIT_FAILURE);
			}
			va_start(ap, format_str);
			vsprintf(&line[strlen(line)], format_str, ap);
			va_end(ap);

			strcat(line, "\n");
		}
		if(shelllog == 1) {
			fprintf(stderr, line);
		}

		if(logqueue_number < 1024) {
			if(prio < LOG_DEBUG) {
				struct logqueue_t *node = MALLOC(sizeof(logqueue_t));
				if(node == NULL) {
					fprintf(stderr, "out of memory");
					exit(EXIT_FAILURE);
				}
				if((node->line = MALLOC(strlen(line)+1)) == NULL) {
					fprintf(stderr, "out of memory");
					exit(EXIT_FAILURE);
				}
				memset(node->line, '\0', strlen(line)+1);
				strcpy(node->line, line);

				if(logqueue_number == 0) {
					logqueue = node;
					logqueue_head = node;
				} else {
					logqueue_head->next = node;
					logqueue_head = node;
				}

				logqueue_number++;
			}
		} else {
			logprintf(LOG_ERR, "log queue full");
		}
		FREE(line);
		errno = save_errno;
		pthread_mutex_unlock(&logqueue_lock);
		pthread_cond_signal(&logqueue_signal);
	}
}

void *logloop(void *param) {
	struct stat sb;
	FILE *lf = NULL;

	pthread_mutex_lock(&logqueue_lock);
	while(loop) {
		if(logqueue_number > 0) {
			pthread_mutex_lock(&logqueue_lock);
			if(logfile != NULL) {
				if((stat(logfile, &sb)) >= 0) {
					if((lf = fopen(logfile, "a")) == NULL) {
						filelog = 0;
					}
				} else {
					if(sb.st_nlink == 0) {
						if((lf = fopen(logfile, "a")) == NULL) {
							filelog = 0;
						}
					}
					if(sb.st_size > LOG_MAX_SIZE) {
						if(lf != NULL) {
							fclose(lf);
						}
						char tmp[strlen(logfile)+5];
						strcpy(tmp, logfile);
						strcat(tmp, ".old");
						rename(logfile, tmp);
						if((lf = fopen(logfile, "a")) == NULL) {
							filelog = 0;
						}
					}
				}
				if(lf != NULL) {
					fwrite(logqueue->line, sizeof(char), strlen(logqueue->line), lf);
					fflush(lf);
					fclose(lf);
					lf = NULL;
				}
			}				
			struct logqueue_t *tmp = logqueue;
			logqueue = logqueue->next;
			FREE(tmp->line);
			FREE(tmp);
			logqueue_number--;
			pthread_mutex_unlock(&logqueue_lock);
		} else {
			pthread_cond_wait(&logqueue_signal, &logqueue_lock);
		}
	}
	pthread_join(pthread_self(), NULL);
	return (void *)NULL;	
}

void logperror(int prio, const char *s) {
	// int save_errno = errno;
	// if(logging == 0)
		// return;

	// if(s != NULL) {
		// logprintf(prio, "%s: %s", s, strerror(errno));
	// } else {
		// logprintf(prio, "%s", strerror(errno));
	// }
	// errno = save_errno;
}

void log_file_enable(void) {
	filelog = 1;
	if(pthinitialized == 0) {
		pthread_mutexattr_init(&logqueue_attr);
		pthread_mutexattr_settype(&logqueue_attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&logqueue_lock, &logqueue_attr);
		pthinitialized = 1;
	}
}

void log_file_disable(void) {
	filelog = 0;
}

void log_shell_enable(void) {
	shelllog = 1;
}

void log_shell_disable(void) {
	shelllog = 0;
}

void log_file_set(char *log) {
	struct stat s;
	struct stat sb;
	FILE *lf = NULL;
	char *filename = basename(log);
	size_t i = (strlen(log)-strlen(filename));
	logpath = REALLOC(logpath, i+1);
	memset(logpath, '\0', i+1);
	strncpy(logpath, log, i);

	if(strcmp(filename, log) != 0) {
		int err = stat(logpath, &s);
		if(err == -1) {
			if(ENOENT == errno) {
				logprintf(LOG_ERR, "the log folder %s does not exist", logpath);
				FREE(logpath);
				exit(EXIT_FAILURE);
			} else {
				logprintf(LOG_ERR, "failed to run stat on log folder %s", logpath);
				FREE(logpath);
				exit(EXIT_FAILURE);
			}
		} else {
			if(S_ISDIR(s.st_mode)) {
				logfile = REALLOC(logfile, strlen(log)+1);
				strcpy(logfile, log);
			} else {
				logprintf(LOG_ERR, "the log folder %s does not exist", logpath);
				FREE(logpath);
				exit(EXIT_FAILURE);
			}
		}
	} else {
		logfile = REALLOC(logfile, strlen(log)+1);
		strcpy(logfile, log);
	}

	char tmp[strlen(logfile)+5];
	strcpy(tmp, logfile);
	strcat(tmp, ".old");

	if((stat(tmp, &sb)) == 0) {
		if(sb.st_nlink > 0) {
			if((stat(logfile, &sb)) == 0) {
				if(sb.st_nlink > 0) {
					remove(tmp);
					rename(logfile, tmp);
				}
			}
		}
	}

	if(lf == NULL && filelog == 1) {
		if((lf = fopen(logfile, "a")) == NULL) {
			filelog = 0;
			shelllog = 1;
			logprintf(LOG_ERR, "could not open logfile %s", logfile);
			FREE(logpath);
			FREE(logfile);
			exit(EXIT_FAILURE);
		} else {
			fclose(lf);
			lf = NULL;
		}
	}

	FREE(logpath);
}

void log_level_set(int level) {
	loglevel = level;
}

int log_level_get(void) {
	return loglevel;
}

void logerror(const char *format_str, ...) {
	char line[1024];
	va_list ap;
	struct stat sb;
	FILE *f = NULL;
	char fmt[64], buf[64];
	struct timeval tv;
	struct tm *tm;
	char date[128];

	memset(line, '\0', 1024);
	gettimeofday(&tv, NULL);
	if((tm = localtime(&tv.tv_sec)) != NULL) {
		strftime(fmt, sizeof(fmt), "%b %d %H:%M:%S", tm);
		snprintf(buf, sizeof(buf), "%s:%03u", fmt, (unsigned int)tv.tv_usec);
	}

	sprintf(date, "[%22.22s] %s: ", buf, progname);
	strcat(line, date);
	va_start(ap, format_str);
	vsprintf(&line[strlen(line)], format_str, ap);
	strcat(line, "\n");

	if((stat("/var/log/pilight.err", &sb)) >= 0) {
		if((f = fopen("/var/log/pilight.err", "a")) == NULL) {
			return;
		}
	} else {
		if(sb.st_nlink == 0) {
			if(!(f = fopen("/var/log/pilight.err", "a"))) {
				return;
			}
		}
		if(sb.st_size > LOG_MAX_SIZE) {
			if(f != NULL) {
				fclose(f);
			}
			char tmp[strlen("/var/log/pilight.err")+5];
			strcpy(tmp, "/var/log/pilight.err");
			strcat(tmp, ".old");
			rename("/var/log/pilight.err", tmp);
			if((f = fopen("/var/log/pilight.err", "a")) == NULL) {
				return;
			}
		}
	}
	if(f != NULL) {
		fwrite(line, sizeof(char), strlen(line), f);
		fflush(f);
		fclose(f);
		f = NULL;
	}
	va_end(ap);
}
