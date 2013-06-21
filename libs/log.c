#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "gc.h"
#include "log.h"

FILE *lf=NULL;

/* Enable log */
int filelog = 1;
int shelllog = 1;
int loglevel = LOG_INFO;
char logfile[1024] = LOG_FILE;

int log_gc() {
	if(lf != NULL) {
		if(fclose(lf) != 0)
			return 0;
	}
	return 1;
}

void logprintf(int prio, char *format_str, ...) {
	int save_errno = errno;
	va_list ap;

	if(filelog == 0 && shelllog == 0)
		return;

	if(loglevel >= prio) {
		if(lf == NULL) {
			if((lf=fopen(logfile,"a")) == NULL) {
				logprintf(LOG_WARNING, "Could not open logfile %s", logfile);
			} else {
				gc_attach(log_gc);
			}
		}

		if(filelog == 0 && lf != NULL && loglevel < LOG_DEBUG) {
			time_t current;
			char *currents;

			current=time(&current);
			currents=ctime(&current);

			fprintf(lf,"%15.15s %s: ",currents+4, progname);
			va_start(ap, format_str);
			if(prio==LOG_WARNING)
				fprintf(lf,"WARNING: ");
			if(prio==LOG_ERR)
				fprintf(lf,"ERROR: ");
			if(prio==LOG_INFO)
				fprintf(lf, "LOG_INFO: ");
			if(prio==LOG_NOTICE)
				fprintf(lf, "LOG_NOTICE: ");
			vfprintf(lf, format_str, ap);
			fputc('\n',lf);
			fflush(lf);
			va_end(ap);
		}

		if(shelllog == 1) {

			fprintf(stderr, "%s: ", progname);
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
				fprintf(stderr, "LOG_DEBUG: ");
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

void enable_file_log() {
	filelog = 1;
}

void disable_file_log() {
	filelog = 0;
}

void enable_shell_log() {
	shelllog = 1;
}

void disable_shell_log() {
	shelllog = 0;
}

void set_logfile(char *file) {
	if(access(file, F_OK) != -1) {
		strcpy(logfile,file);
	} else {
		fprintf(stderr, "%s: the log file %s does not exists\n", progname, optarg);
	}
}

void set_loglevel(int level) {
	loglevel = level;
}