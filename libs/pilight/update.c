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

#include <ctype.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#define __USE_XOPEN
#define _GNU_SOURCE
#include <time.h>
#include <pthread.h>
#include <zlib.h>

#include "http_lib.h"
#include "common.h"
#include "datetime.h"
#include "settings.h"
#include "log.h"
#include "../../pilight.h"

static unsigned short update_loop = 1;
static unsigned short update_needed = 0;
static char *update_latests_ver = NULL;
static char *update_current_ver = NULL;
static time_t update_last_check = 0;
static char **update_mirrors = NULL;
static char *update_filename = NULL, *update_data = NULL;
static char update_typebuf[70];
static int update_lg = 0, update_ret = 0;

static pthread_mutex_t updatelock;
static pthread_mutexattr_t updateattr;
static pthread_cond_t updatesignal;

int update_gc(void) {
	update_loop = 0;

	pthread_mutex_unlock(&updatelock);
	pthread_cond_signal(&updatesignal);

	sfree((void *)&update_latests_ver);
	sfree((void *)&update_current_ver);
	/* clean-up http_lib global */
	if(http_server) sfree((void *)&http_server);

	logprintf(LOG_DEBUG, "garbage collected update library");
	return 1;
}

static int update_mirror_list(void) {
	int i = 0, x = 0, furl = 0;
	char *url = NULL;
	char mirror[255];
	memset(mirror, '\0', 255);

	if(settings_find_string("update-mirror", &url) != 0) {
		url = malloc(strlen(UPDATE_MIRROR)+1);
		if(!url) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		strcpy(url, UPDATE_MIRROR);
		furl = 1;
	}

	http_parse_url(url, &update_filename);
	update_ret = http_get(update_filename, &update_data, &update_lg, update_typebuf);

	if(update_ret == 200 && strcmp(update_typebuf, "text/plain") == 0) {
		char *tmp = update_data;
		while(*tmp != '\0') {
			if(*tmp == '\n' || *tmp == '\0') {
				if(strlen(mirror) > 2) {
					update_mirrors = realloc(update_mirrors, (size_t)((i+1)*(int)sizeof(char *)));
					if(!update_mirrors) {
						logprintf(LOG_ERR, "out of memory");
						exit(EXIT_FAILURE);
					}
					mirror[x-1] = '\0';
					update_mirrors[i] = malloc(strlen(mirror)+1);
					if(!update_mirrors[i]) {
						logprintf(LOG_ERR, "out of memory");
						exit(EXIT_FAILURE);
					}
					strcpy(update_mirrors[i], mirror);
					i++;
				}
				memset(mirror, '\0', 255);
				x=0;
			}
			mirror[x] = *tmp;
			tmp++;
			x++;
		}
	}
	if(update_filename) sfree((void *)&update_filename);
	if(update_data) sfree((void *)&update_data);
	if(furl) sfree((void *)&url);

	return i;
}

static char *update_package_version(char *mirror) {
	int x = 0;
	size_t l = 0;
	char stable[] = "stable";
	char development[] = "development";
	char *nurl = NULL, *output = NULL, *pch = NULL, line[255], *version = malloc(4);
	int devel = 0;
	memset(line, '\0', 255);
	if(!version) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}

	settings_find_number("update-development", &devel);

	if(devel) {
		nurl = malloc(strlen(mirror)+strlen(development)+41);
		if(!nurl) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		sprintf(nurl, "%sdists/%s/main/binary-armhf/Packages.gz", mirror, development);
	} else {
		nurl = malloc(strlen(mirror)+strlen(stable)+41);
		if(!nurl) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		sprintf(nurl, "%sdists/%s/main/binary-armhf/Packages.gz", mirror, stable);
	}
	http_parse_url(nurl, &update_filename);
	update_ret = http_get(update_filename, &update_data, &update_lg, update_typebuf);

	strcpy(version, "0.0");

	if(update_ret == 200 && strcmp(update_typebuf, "application/x-gzip") == 0) {
		z_stream strm = {0};
		unsigned char out[0x4000];
		memset(out, '\0', 0x4000);

		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		strm.next_in = (unsigned char *)update_data;
		strm.avail_in = (uInt)update_lg;
		strm.next_out = out;
		inflateInit2(&strm, 15 | 32);

		do {
			strm.avail_out = 0x4000;
			inflate(&strm, Z_NO_FLUSH);
			l += strlen((char *)out);
			output = realloc(output, l+1);
			if(!output) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			strcpy(&output[l-strlen((char *)out)], (char *)out);
		} while(strm.avail_out == 0);
		inflateEnd(&strm);

		if(output) {
			char *tmp = output;
			x = 0;
			while(*tmp != '\0') {
				if(*tmp == '\n' || *tmp == '\0') {
					if((pch = strstr(line, "Version: ")) > 0) {
						rmsubstr(line, "Version: ");
						rmsubstr(line, "\n");
						if(vercmp(line, version) > 0) {
							version = realloc(version, strlen(line)+1);
							if(!version) {
								logprintf(LOG_ERR, "out of memory");
								exit(EXIT_FAILURE);
							}
							strcpy(version, line);
						}
					}
					memset(line, '\0', 255);
					x=0;
				}
				line[x] = *tmp;
				tmp++;
				x++;
			}
			sfree((void *)&output);
		}
	}
	sfree((void *)&nurl);
	if(update_filename) sfree((void *)&update_filename);
	if(update_data) sfree((void *)&update_data);
	return version;
}

int update_available(void) {
	return update_needed;
}

char *update_latests_version(void) {
	return (char *)update_latests_ver;
}

void *update_poll(void *param) {
	int x = 0;
	int i = 0;
	update_current_ver = malloc(strlen(VERSION)+1);
	update_latests_ver = malloc(strlen(VERSION)+1);
	if(!update_current_ver) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	if(!update_latests_ver) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(update_current_ver, VERSION);
	strcpy(update_latests_ver, VERSION);
	char *n = NULL;
	struct tm *current;
	time_t epoch = 0;
	time_t timenow = 0;

	pthread_mutexattr_init(&updateattr);
	pthread_mutexattr_settype(&updateattr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&updatelock, &updateattr);
	pthread_cond_init(&updatesignal, NULL);

	while(update_loop) {
		time(&timenow);
		if(epoch == timenow) {
			update_last_check = epoch;
			if((i = update_mirror_list()) > 0) {
				for(x = 0; x < i; x++) {
					if((n = update_package_version(update_mirrors[x])) > 0) {
						if(vercmp(n, update_current_ver) > 0) {
							update_needed = 1;
							update_latests_ver = realloc(update_latests_ver, strlen(n)+1);
							if(!update_latests_ver) {
								logprintf(LOG_ERR, "out of memory");
								exit(EXIT_FAILURE);
							}
							strcpy(update_latests_ver, n);
						}
						sfree((void *)&n);
						break;
					}
				}
				for(x = 0; x < i; x++) {
					sfree((void *)&update_mirrors[x]);
				}
				sfree((void *)&update_mirrors);
			}
		}
		if(update_last_check == 0 || (timenow-update_last_check) > 86400 || epoch == 0) {
			current = localtime(&timenow);
			int month = current->tm_mon+1;
			int wday = current->tm_wday;
			int mday = current->tm_mday;
			int year = current->tm_year+1900;

			epoch = (int)datetime2ts(year, month, mday+(7-wday), 0, 0, 0, 0);
		}
		struct timeval tp;
		struct timespec ts;
		pthread_mutex_unlock(&updatelock);
		gettimeofday(&tp, NULL);
		ts.tv_sec = tp.tv_sec;
		ts.tv_nsec = tp.tv_usec * 1000;
		ts.tv_sec += 1;
		pthread_mutex_lock(&updatelock);
		pthread_cond_timedwait(&updatesignal, &updatelock, &ts);
	}
	return 0;
}
