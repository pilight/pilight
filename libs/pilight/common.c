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

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/time.h>
#include <time.h>
#include <dirent.h>
#include <sys/types.h>

#include "common.h"
#include "log.h"

void logmarkup(void) {
	char fmt[64], buf[64];
	struct timeval tv;
	struct tm *tm;

	gettimeofday(&tv, NULL);
	if((tm = localtime(&tv.tv_sec)) != NULL) {
		strftime(fmt, sizeof(fmt), "%b %d %H:%M:%S", tm);
		snprintf(buf, sizeof(buf), "%s:%03u", fmt, (unsigned int)tv.tv_usec);
	}
	
	sprintf(debug_log, "[%22.22s] %s: ", buf, progname);
}

#ifdef __FreeBSD__
int proc_find(const char *name) {
#else
pid_t proc_find(const char *name) {
#endif
	DIR* dir;
	struct dirent* ent;
	char* endptr;
	char buf[512];
	FILE* fp = NULL;

	if(!(dir = opendir("/proc"))) {
        logprintf(LOG_ERR, "can't open /proc");
        return -1;
    }

    while((ent = readdir(dir)) != NULL) {
        long lpid = strtol(ent->d_name, &endptr, 10);
        if(*endptr != '\0') {
			continue;
        }

        snprintf(buf, sizeof(buf), "/proc/%ld/cmdline", lpid);

        if((fp = fopen(buf, "r"))) {
			if(fgets(buf, sizeof(buf), fp) != NULL) {
                char* first = strtok(buf, " ");
                if(!strcmp(first, name)) {
					fclose(fp);
					closedir(dir);
					return (pid_t)lpid;
				}
			}
			fclose(fp);
		}
	}
	closedir(dir);
	return -1;
}

int isNumeric(char * s) {
    if(s == NULL || *s == '\0' || *s == ' ')
		return EXIT_FAILURE;
    char *p;
    strtod(s, &p);
    return (*p == '\0') ? EXIT_SUCCESS : EXIT_FAILURE;
}

#ifdef DEBUG

const char *debug_filename(const char *file) {
	return strrchr(file, '/') ? strrchr(file, '/') + 1 : file;
}

void *debug_malloc(size_t len, const char *file, int line) {
	void *(*libc_malloc)(size_t) = dlsym(RTLD_NEXT, "malloc");
	if(shelllog == 1 && loglevel == LOG_DEBUG) {
		logmarkup();
		printf("%s", debug_log);
		printf("DEBUG: malloc from %s #%d\n", debug_filename(file), line);
	}
	return libc_malloc(len);
}

void *debug_realloc(void *addr, size_t len, const char *file, int line) {
	void *(*libc_realloc)(void *, size_t) = dlsym(RTLD_NEXT, "realloc");
	if(addr == NULL) {
		if(shelllog == 1 && loglevel == LOG_DEBUG) {
				logmarkup();
				printf("%s", debug_log);			
				printf("DEBUG: realloc from %s #%d\n", debug_filename(file), line);
			}
			return debug_malloc(len, file, line);
		} else {
			return libc_realloc(addr, len);
	}
}

void *debug_calloc(size_t nlen, size_t elen) {
	void *(*libc_calloc)(size_t, size_t) = dlsym(RTLD_NEXT, "calloc");
	return libc_calloc(size_t, size_t);
}

void debug_free(void **addr, const char *file, int line) {
	void ** __p = addr;
	if(*(__p) != NULL) {
	if(shelllog == 1 && loglevel == LOG_DEBUG) {
			logmarkup();
			printf("%s", debug_log);			
			printf("DEBUG: free from %s #%d\n", debug_filename(file), line);
		}
		free(*(__p));
		*(__p) = NULL;
	}
}

#else

void sfree(void **addr) {
	void ** __p = addr;
	if(*(__p) != NULL) {
		free(*(__p));
		*(__p) = NULL;
	}
}

#endif
