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

#include <stdlib.h>
#include <stdio.h>
#define __USE_XOPEN
#include <sys/time.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <pthread.h>

#include "json.h"
#include "common.h"
#include "log.h"

#define NRCOUNTRIES 	408
#define PRECISION 		1

#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))

static int ***tzcoords;
static unsigned int tznrpolys[NRCOUNTRIES];
static char tznames[NRCOUNTRIES][30];
static int tzdatafilled = 0;
static pthread_mutex_t tzlock;
static pthread_mutexattr_t tzattr;
static int tz_lock_initialized = 0;

static int fillTZData(void) {
	if(tz_lock_initialized == 0) {
		pthread_mutexattr_init(&tzattr);
		pthread_mutexattr_settype(&tzattr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&tzlock, &tzattr);
		tz_lock_initialized = 1;
	}

	pthread_mutex_lock(&tzlock);
	if(tzdatafilled == 1) {
		return EXIT_SUCCESS;
	}
	char *content = NULL;
	JsonNode *root = NULL;
	FILE *fp;
	size_t bytes;
	struct stat st;

	char tzdatafile[] = TZDATA_FILE;
	/* Read JSON tzdata file */
	if(!(fp = fopen(tzdatafile, "rb"))) {
		logprintf(LOG_ERR, "cannot read tzdata file: %s", tzdatafile);
		return EXIT_FAILURE;
	}

	fstat(fileno(fp), &st);
	bytes = (size_t)st.st_size;

	if(!(content = calloc(bytes+1, sizeof(char)))) {
		logprintf(LOG_ERR, "out of memory");
		fclose(fp);
		return EXIT_FAILURE;
	}

	if(fread(content, sizeof(char), bytes, fp) == -1) {
		logprintf(LOG_ERR, "cannot read tzdata file: %s", tzdatafile);
	}
	fclose(fp);

	/* Validate JSON and turn into JSON object */
	if(json_validate(content) == false) {
		logprintf(LOG_ERR, "tzdata is not in a valid json format");
		sfree((void *)&content);
		return EXIT_FAILURE;
	}

	logprintf(LOG_DEBUG, "loading timezone database...");
	root = json_decode(content);

	JsonNode *alist = json_first_child(root);
	unsigned int i = 0, x = 0, y = 0;
	while(alist) {
		JsonNode *country = json_first_child(alist);
		while(country) {
			strcpy(tznames[i], country->key);
			if(!(tzcoords = realloc(tzcoords, sizeof(int **)*(i+1)))) {
				logprintf(LOG_ERR, "out of memory");
			}
			tzcoords[i] = NULL;
			JsonNode *coords = json_first_child(country);
			x = 0;
			while(coords) {
				y = 0;
				if(!(tzcoords[i] = realloc(tzcoords[i], sizeof(int *)*(x+1)))) {
					logprintf(LOG_ERR, "out of memory");
				}
				tzcoords[i][x] = NULL;
				if(!(tzcoords[i][x] = malloc(sizeof(int)*2))) {
					logprintf(LOG_ERR, "out of memory");
				}
				JsonNode *lonlat = json_first_child(coords);
				while(lonlat) {
					tzcoords[i][x][y] = (int)lonlat->number_;
					y++;
					lonlat = lonlat->next;
				}
				x++;
				coords = coords->next;
			}
			tznrpolys[i] = x;
			i++;
			country = country->next;
		}
		alist = alist->next;
	}
	json_delete(root);
	sfree((void *)&content);
	tzdatafilled = 1;
	pthread_mutex_unlock(&tzlock);
	return EXIT_SUCCESS;
}

int datetime_gc(void) {
	pthread_mutex_unlock(&tzlock);
	int i = 0, a = 0;
	if(tzdatafilled) {
		for(i=0;i<NRCOUNTRIES;i++) {
			unsigned int n = tznrpolys[i];
			for(a=0;a<n;a++) {
				sfree((void *)&tzcoords[i][a]);
			}
			sfree((void *)&tzcoords[i]);
		}
		sfree((void *)&tzcoords);
		logprintf(LOG_DEBUG, "garbage collected datetime library");
	}
	return EXIT_SUCCESS;
}

char *coord2tz(double longitude, double latitude) {
	if(fillTZData() == EXIT_FAILURE) {
		return NULL;
	}

	int i = 0, a = 0, margin = 1, inside = 0;
	char *tz = NULL;

	margin *= (int)pow(10, PRECISION);
	int y = (int)round(latitude*(int)pow(10, PRECISION));
	int x = (int)round(longitude*(int)pow(10, PRECISION));

	while(!inside && margin < (5*(int)pow(10, PRECISION))) {
		for(i=0;i<NRCOUNTRIES;i++) {
			unsigned int n = tznrpolys[i];
			if(n > 0) {
				int p1x = 0;
				int p1y = 0;
				for(a=0;a<n;a++) {
					if(tzcoords[i][a][0] < p1x || ((int)p1x == 0)) {
						p1x = tzcoords[i][a][0];
					}
					if(tzcoords[i][a][1] < p1y && ((int)p1y == 0)) {
						p1y = tzcoords[i][a][1];
					}
				}
				for(a=0;a<n+1;a++) {
					int p2x = tzcoords[i][a % (int)n][0];
					int p2y = tzcoords[i][a % (int)n][1];
					if((round(p2x)-margin < round(x) && round(p2x)+margin > round(x))
					   &&(round(p2y)-margin < round(y) && round(p2y)+margin > round(y))) {
						int xinters = 0;
						if(y > min(p1y, p2y)) {
							if(y <= max(p1y, p2y)) {
								if(x <= max(p1x, p2x)) {
									if(p1y != p2y) {
										xinters = (y-p1y)*(p2x-p1x)/(p2y-p1y)+p1x;
									}
									if(p1x == p2x || x <= xinters) {
										tz = tznames[i];
										inside = 1;
										break;
									}
								}
							}
						}
						p1x = p2x;
						p1y = p2y;
					}
				}
				if(inside == 1) {
					break;
				}
			}
		}
		margin /= (int)pow(10, PRECISION);
		margin++;
		margin *= (int)pow(10, PRECISION);
	}
	return tz;
}

time_t datetime2ts(int year, int month, int day, int hour, int minutes, int seconds, char *tz) {
	char date[20];
	time_t t;
	struct tm tm = {0};
	sprintf(date, "%d-%d-%d %d:%d:%d", year, month, day, hour, minutes, seconds);
	strptime(date, "%Y-%m-%d %T", &tm);
	if(tz) {
		setenv("TZ", tz, 1);
	}
	t = mktime(&tm);
	if(tz) {
		unsetenv("TZ");
	}
	return t;
}

struct tm *localtztime(char *tz, time_t t) {
	struct tm *tm = NULL;
	setenv("TZ", tz, 1);
	tm = localtime(&t);
	unsetenv("TZ");
	return tm;
}

int tzoffset(char *tz1, char *tz2) {
    time_t utc, tzsearch, now;
	struct tm *tm = NULL;
	now = time(NULL);
	tm = localtime(&now);

	setenv("TZ", tz1, 1);
	utc = mktime(tm);
	setenv("TZ", tz2, 1);
	tzsearch = mktime(tm);
	unsetenv("TZ");
	return (int)((utc-tzsearch)/3600);
}

int ctzoffset(void) {
    time_t tm1, tm2;
	struct tm *t2;
    tm1 = time(NULL);
    t2 = gmtime(&tm1);
    tm2 = mktime(t2);
	localtime(&tm1);
	return (int)((tm1 - tm2)/3600);
}

int isdst(char *tz) {
	char UTC[] = "Europe/London";
	time_t now = 0;
	struct tm *tm = NULL;
	now = time(NULL);
	tm = gmtime(&now);
	tm->tm_hour += tzoffset(UTC, tz);

	setenv("TZ", tz, 1);
	mktime(tm);
	unsetenv("TZ");

	return tm->tm_isdst;
}
