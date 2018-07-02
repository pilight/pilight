/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#ifndef _WIN32
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
	#include <sys/time.h>
	#include <unistd.h>
	#include <libgen.h>
#endif

#include "pilight.h"
#include "common.h"
#include "log.h"
#include "../../libuv/uv.h"

#if defined(WIN32) || defined(WIN64)
	// Copied from linux libc sys/stat.h:
	#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
	#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

struct logqueue_t {
	char *buffer;
	struct logqueue_t *next;
} logqueue_t;

static uv_mutex_t logqueue_lock;
static int pthinitialized = 0;

static struct logqueue_t *logqueue = NULL;

static char *logfile = NULL;
static int filelog = 1;
static int shelllog = 0;
static int loglevel = LOG_DEBUG;
static int init = 0;
static int stop = 0;

static void *reason_log_free(void *param) {
	struct reason_log_t *data = param;
	FREE(data->buffer);
	FREE(data);
	return NULL;
}

void logwrite(char *line) {
	struct stat sb;
	FILE *lf = NULL;
	if(logfile != NULL) {
		if((stat(logfile, &sb)) == 0) {
			if(sb.st_nlink != 0 && sb.st_size > LOG_MAX_SIZE) {
				char *tmp = MALLOC(strlen(logfile)+5);
				if(tmp == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				strcpy(tmp, logfile);
				strcat(tmp, ".old");
				rename(logfile, tmp);
				FREE(tmp);
			}
		}
		if((lf = fopen(logfile, "a")) == NULL) {
			filelog = 0;
		} else {
			fwrite(line, sizeof(char), strlen(line), lf);
			fflush(lf);
			fclose(lf);
			lf = NULL;
		}
	}
}

int log_gc(void) {
	if(shelllog == 1) {
		fprintf(stderr, "DEBUG: garbage collected log library\n");
	}
	
	stop = 1;
	init = 0;
	shelllog = 0;
	filelog = 0;

	if(pthinitialized == 1) {
		/* Flush log queue to pilight.err file */
		uv_mutex_lock(&logqueue_lock);
	}
	while(logqueue) {
		struct logqueue_t *tmp = logqueue;

		logwrite(tmp->buffer);

		logqueue = logqueue->next;

		FREE(tmp->buffer);
		FREE(tmp);
	}

	if(pthinitialized == 1) {
		uv_mutex_unlock(&logqueue_lock);
	}

	if(logfile != NULL) {
		FREE(logfile);
	}
	
	return 1;
}

static void loginitlock(void) {
	if(pthinitialized == 0) {
		uv_mutex_init(&logqueue_lock);
		pthinitialized = 1;
	}	
}

void _logprintf(int prio, char *file, int line, const char *str, ...) {
	struct timeval tv;
	struct tm tm;
	va_list ap, apcpy;
	char fmt[64], buffer[1024];
	int errcpy = -1, len = 0, pos = 0, bufsize = 1024;

	if(loglevel >= prio) {	
		errcpy = errno;
		memset(&tm, '\0', sizeof(struct tm));
		gettimeofday(&tv, NULL);
#ifdef _WIN32
		struct tm *tm1 = NULL;
		time_t long_time = tv.tv_sec;
		if((tm1 = gmtime(&long_time)) != 0) {
			memcpy(&tm, tm1, sizeof(struct tm));
			strftime(fmt, sizeof(fmt), "%b %d %H:%M:%S", &tm);
#else
		if((gmtime_r(&tv.tv_sec, &tm)) != 0) {
			strftime(fmt, sizeof(fmt), "%b %d %H:%M:%S", &tm);
#endif
		}
		len = snprintf(NULL, 0, "(%s #%d) [%s:%03u]", file, line, fmt, (unsigned int)tv.tv_usec);
		
		/* len + loglevel */
		// if(len+9 > bufsize) {
			// if((buffer = REALLOC(buffer, len+10)) == NULL) {
				// printf("out of memory\n");
				// exit(EXIT_FAILURE);
			// }
			// bufsize = len+10+1;
		// }
		pos += snprintf(buffer, bufsize, "(%s #%d) [%s:%03u] ", file, line, fmt, (unsigned int)tv.tv_usec);

		switch(prio) {
			case LOG_WARNING:
				pos += snprintf(&buffer[pos], bufsize-pos, "WARNING: ");
			break;
			case LOG_ERR:
				pos += snprintf(&buffer[pos], bufsize-pos, "ERROR: ");
			break;
			case LOG_INFO:
				pos += snprintf(&buffer[pos], bufsize-pos, "INFO: ");
			break;
			case LOG_NOTICE:
				pos += snprintf(&buffer[pos], bufsize-pos, "NOTICE: ");
			break;
			case LOG_DEBUG:
				pos += snprintf(&buffer[pos], bufsize-pos, "DEBUG: ");
			break;
			default:
			break;
		}

// #ifdef _WIN32
		// apcpy = ap;
// #else
		// va_copy(apcpy, ap);
// #endif
		va_start(apcpy, str);
#ifdef _WIN32
		len = _vscprintf(str, apcpy);
#else
		len = vsnprintf(NULL, 0, str, apcpy);
#endif
		if(len == -1) {
			fprintf(stderr, "ERROR: improperly formatted logprintf message %s\n", str);
		} else {
			/*
			 * Truncate with ellipses when logstring
			 * is larger than allowed size.
			 */
			if(len > (bufsize-pos-3)) {
				va_end(apcpy);
				char *tmp = MALLOC(len);
				if(tmp == NULL) {
					printf("out of memory\n");
					exit(EXIT_FAILURE);
				}
				va_start(ap, str);
				vsnprintf(tmp, len, str, ap);
				va_end(ap);
				tmp[bufsize-pos-6] = '.';
				tmp[bufsize-pos-5] = '.';
				tmp[bufsize-pos-4] = '.';
				tmp[bufsize-pos-3] = '\0';
				strcpy(&buffer[pos], tmp);
				pos += bufsize-pos-3;
				FREE(tmp);
			} else {
				va_end(apcpy);
				va_start(ap, str);
				pos += vsnprintf(&buffer[pos], bufsize-pos, str, ap);
				va_end(ap);
			}
		}
		buffer[pos++]='\n';
		buffer[pos++]='\0';
		if(shelllog == 1) {
			fprintf(stderr, "%s", buffer);
		}

#ifdef _WIN32
		// if(prio == LOG_ERR && strstr(progname, "daemon") != NULL && pilight.running == 0) {
			// MessageBox(NULL, buffer, "pilight :: error", MB_OK);
		// }
#endif
		if(prio < LOG_DEBUG && stop == 0 && filelog == 1) {
			if(init == 0) {
				uv_mutex_lock(&logqueue_lock);
				struct logqueue_t *node = MALLOC(sizeof(struct logqueue_t));
				if(node == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				if((node->buffer = MALLOC(pos+1)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}

				strcpy(node->buffer, buffer);
				node->next = NULL;

				loginitlock();
				struct logqueue_t *tmp = logqueue;
				if(tmp != NULL) {
					while(tmp->next != NULL) {
						tmp = tmp->next;
					}
					tmp->next = node;
					node = tmp;
				} else {
					node->next = logqueue;
					logqueue = node;
				}
				uv_mutex_unlock(&logqueue_lock);
			} else {
				struct reason_log_t *node = MALLOC(sizeof(struct reason_log_t));
				if(node == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				if((node->buffer = MALLOC(pos+1)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				strcpy(node->buffer, buffer);
				eventpool_trigger(REASON_LOG, reason_log_free, node);
			}
		}
		// if(buffer != NULL) {
			// FREE(buffer);
		// }
		
		errno = errcpy;
	}
}

void *logprocess(int reason, void *param) {
	struct reason_log_t *data = param;

	logwrite(data->buffer);

	return NULL;
}

void log_init(void) {
	init = 1;
	stop = 0;

	loginitlock();
	eventpool_callback(REASON_LOG, logprocess);
	uv_mutex_lock(&logqueue_lock);
	while(logqueue) {
		struct logqueue_t *tmp = logqueue;
		struct reason_log_t *node = MALLOC(sizeof(struct reason_log_t));
		if(node == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		if((node->buffer = MALLOC(strlen(tmp->buffer)+1)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		strcpy(node->buffer, tmp->buffer);
		eventpool_trigger(REASON_LOG, reason_log_free, node);
		logqueue = logqueue->next;
		FREE(tmp->buffer);
		FREE(tmp);
	}
	uv_mutex_unlock(&logqueue_lock);
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

int log_file_set(char *log) {
	struct stat s;
	struct stat sb;
	char *filename = NULL;
	char *logpath = NULL;
	FILE *lf = NULL;

#ifdef _WIN32
	char drive[255];
	char directory[255];
	char filebase[255];
	char extension[255];

	if(_splitpath_s(log, drive, 255, directory, 255,  filebase, 255, extension, 255) != 0) {
		logprintf(LOG_ERR, "could not open logfile %s", log);
		return -1;
	}
	if((filename = MALLOC(strlen(filebase)+strlen(extension)+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	sprintf(filename, "%s%s", filebase, extension);

#else
	if((filename = basename(log)) == NULL) {
		logprintf(LOG_ERR, "could not open logfile %s", log);
		return -1;
	}
#endif

	size_t i = (strlen(log)-strlen(filename));
	if((logpath = REALLOC(logpath, i+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(logpath, '\0', i+1);
	strncpy(logpath, log, i);

/*
 * dir stat doens't work on windows if path has a trailing slash
 */
#ifdef _WIN32
	if(logpath[i-1] == '\\' || logpath[i-1] == '/') {
		logpath[i-1] = '\0';
	}
#endif

	if(strcmp(filename, log) != 0) {
		int err = stat(logpath, &s);
		if(err == -1) {
			if(ENOENT == errno) {
				logprintf(LOG_ERR, "the log folder %s does not exist", logpath);
				FREE(logpath);
				return EXIT_FAILURE;
			} else {
				logprintf(LOG_ERR, "failed to run stat on log folder %s", logpath);
				FREE(logpath);
				return EXIT_FAILURE;
			}
		} else {
			if(S_ISDIR(s.st_mode)) {
				if((logfile = REALLOC(logfile, strlen(log)+1)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				strcpy(logfile, log);
			} else {
				logprintf(LOG_ERR, "the log folder %s does not exist", logpath);
				FREE(logpath);
				return EXIT_FAILURE;
			}
		}
	} else {
		if((logfile = REALLOC(logfile, strlen(log)+1)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		strcpy(logfile, log);
	}
#ifdef _WIN32
	FREE(filename);
#endif

	char *tmp = MALLOC(strlen(logfile)+5);
	if(tmp == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
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
	FREE(tmp);

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
	return EXIT_SUCCESS;
}

void log_level_set(int level) {
	loglevel = level;
}

int log_level_get(void) {
	return loglevel;
}

/*
 * We don't want a formatted string here
 * because that will crash on strings like
 * DATE_FORMAT(dt, "%H%M%S") due to the
 * unescaped percentages.
 */
void logerror(char *str) {
	char line[1024];
	// va_list ap;
	struct stat sb;
	FILE *f = NULL;
	char fmt[64], buf[64];
	struct timeval tv;
	struct tm tm;
	char date[128];
#ifdef _WIN32
	const char *errpath = "c:/ProgramData/pilight/pilight.err";
#else
	const char *errpath = "/var/log/pilight.err";
#endif
	memset(line, '\0', 1024);
	// memset(&ap, '\0', sizeof(va_list));
	memset(&sb, '\0', sizeof(struct stat));
	memset(&tv, '\0', sizeof(struct timeval));
	memset(date, '\0', 128);
	memset(&tm, '\0', sizeof(struct tm));

	gettimeofday(&tv, NULL);
#ifdef _WIN32
		if((localtime(&tv.tv_sec)) != 0) {
#else
		if((localtime_r(&tv.tv_sec, &tm)) != 0) {
#endif
		strftime(fmt, sizeof(fmt), "%b %d %H:%M:%S", &tm);
		snprintf(buf, sizeof(buf), "%s:%03u", fmt, (unsigned int)tv.tv_usec);
	}

	sprintf(date, "[%22.22s] %s: ", buf, progname);
	strcat(line, date);
	// va_start(ap, format_str);
	// vsprintf(&line[strlen(line)], format_str, ap);
	memcpy(&line[strlen(line)], str, strlen(str));
	strcat(line, "\n");

	if((stat(errpath, &sb)) >= 0) {
		if((f = fopen(errpath, "a")) == NULL) {
			return;
		}
	} else {
		if(sb.st_nlink == 0) {
			if(!(f = fopen(errpath, "a"))) {
				return;
			}
		}
		if(sb.st_size > LOG_MAX_SIZE) {
			if(f != NULL) {
				fclose(f);
			}
			char tmp[strlen(errpath)+5];
			strcpy(tmp, errpath);
			strcat(tmp, ".old");
			rename(errpath, tmp);
			if((f = fopen(errpath, "a")) == NULL) {
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
	// va_end(ap);
}
