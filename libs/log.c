#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include "config.h"
#include "gc.h"
#include "log.h"

FILE *lf=NULL;

/* Enable log */
int filelog = 1;
int shelllog = 1;
int loglevel = LOG_INFO;
char logfile[1024] = LOG_FILE;

int log_gc(void) {
	if(lf != NULL) {
		if(fclose(lf) != 0)
			return 0;
	}
	return 1;
}

void logprintf(int prio, const char *format_str, ...) {
	int save_errno = errno;
	va_list ap;

	if(filelog == 0 && shelllog == 0)
		return;

	if(loglevel >= prio) {
		if(lf == NULL) {
			if((lf = fopen(logfile, "a+")) == NULL) {
				logprintf(LOG_WARNING, "could not open logfile %s", logfile);
			} else {
				gc_attach(log_gc);
			}
		}
		
		time_t current;
		char *currents;

		current=time(&current);
		currents=ctime(&current);		

		if(filelog == 0 && lf != NULL && loglevel < LOG_DEBUG) {
			fprintf(lf,"[%15.15s] %s: ",currents+4, progname);
			va_start(ap, format_str);
			if(prio==LOG_WARNING)
				fprintf(lf,"WARNING: ");
			if(prio==LOG_ERR)
				fprintf(lf,"ERROR: ");
			if(prio==LOG_INFO)
				fprintf(lf, "INFO: ");
			if(prio==LOG_NOTICE)
				fprintf(lf, "NOTICE: ");
			vfprintf(lf, format_str, ap);
			fputc('\n',lf);
			fflush(lf);
			va_end(ap);
		}

		if(shelllog == 1) {

			fprintf(stderr, "[%15.15s] %s: ",currents+4, progname);
			va_start(ap, format_str);

			if(prio==LOG_WARNING)
				fprintf(stderr, "WARNING: ");
			if(prio==LOG_ERR)
				fprintf(stderr, "ERROR: ");
			if(prio==LOG_INFO)
				fprintf(stderr, "INFO: ");
			if(prio==LOG_NOTICE)
				fprintf(stderr, "NOTICE: ");
			if(prio==LOG_DEBUG)
				fprintf(stderr, "DEBUG: ");
			vfprintf(stderr, format_str, ap);
			fputc('\n',stderr);
			fflush(stderr);
			va_end(ap);
		}
	}
	errno = save_errno;
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

void enable_file_log(void) {
	filelog = 1;
}

void disable_file_log(void) {
	filelog = 0;
}

void enable_shell_log(void) {
	shelllog = 1;
}

void disable_shell_log(void) {
	shelllog = 0;
}

void set_logfile(char *log) {
	struct stat s;
	char *filename = basename(log);
	char path[1024];
	size_t i = (strlen(log)-strlen(filename));
	
	memset(path, '\0', sizeof(path));
	memcpy(path, log, i);

	if(strcmp(basename(log), log) != 0) {
		int err = stat(path, &s);
		if(err == -1) {
			if(ENOENT == errno) {
				logprintf(LOG_ERR, "the log file folder does not exist", optarg);
				exit(EXIT_FAILURE);
			} else {
				logprintf(LOG_ERR, "failed to run stat on log folder", optarg);
				exit(EXIT_FAILURE);
			}
		} else {
			if(S_ISDIR(s.st_mode)) {
				strcpy(logfile,log);
			} else {
				logprintf(LOG_ERR, "the log file folder does not exist", optarg);
				exit(EXIT_FAILURE);
			}
		}
	} else {
		strcpy(logfile,log);
	}
}

void set_loglevel(int level) {
	loglevel = level;
}

int get_loglevel(void) {
	return loglevel;
}