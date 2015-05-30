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
#ifndef _WIN32
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
#endif

#include "json.h"
#include "common.h"
#include "log.h"
#include "mem.h"

#define NRCOUNTRIES 	408
#define PRECISION 		1

#ifndef min
	#define min(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef max
	#define max(a,b) (((a)>(b))?(a):(b))
#endif

static int ***tzcoords;
static unsigned int tznrpolys[NRCOUNTRIES];
static char tznames[NRCOUNTRIES][30];
static int tzdatafilled = 0;
static pthread_mutex_t tzlock;
static pthread_mutexattr_t tzattr;
static int tz_lock_initialized = 0;

/*
	Extra checks for gracefull (early)
  stopping of pilight
*/
static int fillingtzdata = 0;
static int searchingtz = 0;

static int fillTZData(void) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	if(tz_lock_initialized == 0) {
		pthread_mutexattr_init(&tzattr);
		pthread_mutexattr_settype(&tzattr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&tzlock, &tzattr);
		tz_lock_initialized = 1;
	}

	if(tz_lock_initialized == 1) {
		pthread_mutex_lock(&tzlock);
	}
/*
	Extra checks for gracefull (early)
  stopping of pilight
*/
	fillingtzdata = 1;
	if(tzdatafilled == 1) {
		fillingtzdata = 0;
		if(tz_lock_initialized == 1) {
			pthread_mutex_unlock(&tzlock);
		}
		return EXIT_SUCCESS;
	}
	char *content = NULL;
	JsonNode *root = NULL;

	char tzdatafile[] = TZDATA_FILE;
	/* Read JSON tzdata file */
	if(file_get_contents(tzdatafile, &content) == 0) {
		/* Validate JSON and turn into JSON object */
		if(json_validate(content) == false) {
			logprintf(LOG_ERR, "tzdata is not in a valid json format");
			free(content);
			fillingtzdata = 0;
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
				if((tzcoords = realloc(tzcoords, sizeof(int **)*(i+1))) == NULL) {
					fprintf(stderr, "out of memory\n");
					exit(EXIT_FAILURE);
				}
				tzcoords[i] = NULL;
				JsonNode *coords = json_first_child(country);
				x = 0;
				while(coords) {
					y = 0;
					if((tzcoords[i] = realloc(tzcoords[i], sizeof(int *)*(x+1))) == NULL) {
						fprintf(stderr, "out of memory\n");
						exit(EXIT_FAILURE);
					}
					tzcoords[i][x] = NULL;
					if((tzcoords[i][x] = malloc(sizeof(int)*2)) == NULL) {
						fprintf(stderr, "out of memory\n");
						exit(EXIT_FAILURE);
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
	}
	free(content);
	tzdatafilled = 1;
	fillingtzdata = 0;
	if(tz_lock_initialized == 1) {
		pthread_mutex_unlock(&tzlock);
	}
	return EXIT_SUCCESS;
}

int datetime_gc(void) {
	int i = 0, a = 0;
/*
	Extra checks for gracefull (early)
  stopping of pilight
*/
	while(fillingtzdata) {
		usleep(10);
	}
	while(searchingtz) {
		usleep(10);
	}
	if(tzdatafilled == 1) {
		for(i=0;i<NRCOUNTRIES;i++) {
			unsigned int n = tznrpolys[i];
			for(a=0;a<n;a++) {
				free(tzcoords[i][a]);
			}
			free(tzcoords[i]);
		}
		free(tzcoords);
		logprintf(LOG_DEBUG, "garbage collected datetime library");
	}
	return EXIT_SUCCESS;
}

char *coord2tz(double longitude, double latitude) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	if(fillTZData() == EXIT_FAILURE) {
		return NULL;
	}

/*
	Extra checks for gracefull (early)
  stopping of pilight
*/
	if(tz_lock_initialized == 1) {
		pthread_mutex_lock(&tzlock);
	}
	searchingtz = 1;
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
	searchingtz = 0;
	if(tz_lock_initialized == 1) {
		pthread_mutex_unlock(&tzlock);
	}
	return tz;
}

time_t datetime2ts(int year, int month, int day, int hour, int minutes, int seconds, char *tz) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	atomiclock();
 	time_t t;
 	struct tm tm = {0};
	char oritz[64];
	memset(oritz, '\0', 64);

	if(getenv("TZ") != NULL) {
		strcpy(oritz, getenv("TZ"));
	}

	tm.tm_sec = seconds;
	tm.tm_min = minutes;
	tm.tm_hour = hour;
	tm.tm_mday = day;
	tm.tm_mon = month-1;
	tm.tm_year = year-1900;

	if(tz != NULL) {
		setenv("TZ", tz, 1);
	}
	tzset();

	t = mktime(&tm);

	if(strlen(oritz) > 0) {
		setenv("TZ", oritz, 1);
	} else {
		unsetenv("TZ");
	}
	tzset();

	atomicunlock();
	return t;
}

int tzoffset(char *tz1, char *tz2) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	atomiclock();

	time_t utc, tzsearch, now;
	struct tm tm;
	char oritz[64];

	memset(&tm, '\0', sizeof(struct tm));
	memset(oritz, '\0', 64);

	if(getenv("TZ") != NULL) {
		strcpy(oritz, getenv("TZ"));
	}

	now = time(NULL);
#ifdef _WIN32
	localtime(&now);
#else
	localtime_r(&now, &tm);
#endif

	setenv("TZ", tz1, 1);
	tzset();
	utc = mktime(&tm);

	setenv("TZ", tz2, 1);
	tzset();
	tzsearch = mktime(&tm);

	if(strlen(oritz) > 0) {
		setenv("TZ", oritz, 1);
	} else {
		unsetenv("TZ");
	}
	tzset();

	atomicunlock();

	return (int)((utc-tzsearch)/3600);
}

int ctzoffset(void) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	time_t tm1, tm2;
	struct tm t2, tmp;
	memset(&tmp, '\0', sizeof(struct tm));

	tm1 = time(NULL);

	memset(&t2, '\0', sizeof(struct tm));
#ifdef _WIN32
	gmtime(&tm1);
#else
	gmtime_r(&tm1, &t2);
#endif

	tm2 = mktime(&t2);
#ifdef _WIN32
	localtime(&tm1);
#else
	localtime_r(&tm1, &tmp);
#endif
	return (int)((tm1 - tm2)/3600);
}

int isdst(time_t t, char *tz) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	char oritz[64];
	int dst = 0;

	memset(oritz, '\0', 64);
	if(getenv("TZ") != NULL) {
		strcpy(oritz, getenv("TZ"));
	}

	atomiclock();

	setenv("TZ", tz, 1);
	tzset();

#ifdef _WIN32
	struct tm *tm1;
	if((tm1 = localtime(&t)) != 0) {
		dst = tm1->tm_isdst;
#else
	struct tm tm;
	if(localtime_r(&t, &tm) != NULL) {
		dst = tm.tm_isdst;
#endif
	}

	if(strlen(oritz) > 0) {
		setenv("TZ", oritz, 1);
	} else {
		unsetenv("TZ");
	}
	tzset();

	atomicunlock();

	return dst;
}

void datefix(int *year, int *month, int *day, int *hour, int *minute, int *second) {
	int months[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	int i = 0;

	if(((*year % 4) == 0 && (*year % 100) != 0) || (*year % 400) == 0) {
		months[1] = 29;
	}
	while(*second >= 60) {
		*minute += 1;
		*second -= 60;
	}
	while(*second < 0) {
		*minute -= 1;
		*second = 60 + *second;
	}
	while(*minute >= 60) {
		*hour += 1;
		*minute -= 60;
	}
	while(*minute < 0) {
		*hour -= 1;
		*minute = 60 + *minute;
	}
	while(*hour > 23) {
		*day += 1;
		*hour -= 24;
	}
	while(*hour < 0) {
		*day -= 1;
		*hour = 24 + *hour;
	}
	while(*day > months[*month-1] || *day <= 0 || *month > 12 || *month <= 0) {
		if(*month > 0 && *month < 13) {
			if(*day > months[*month-1]) {
				i = months[*month-1];
				*month += 1;
				*day -= i;
			} else if(*day <= 0) {
				*month -= 1;
				if(*month <= 0) {
					*day = months[11 + *month] + *day;
				} else {
					*day = months[*month-1] + *day;
				}
			}
		}
		while(*month > 12 || *month <= 0) {
			if(*month > 12) {
				*year += 1;
				*month -= 12;
			} else if(*month < 0) {
				*year -= 1;
				*month = 12 + *month;
			} else {
				*year -= 1;
				*month = 12;
			}
			if(((*year % 4) == 0 && (*year % 100) != 0) || (*year % 400) == 0) {
				months[1] = 29;
			} else {
				months[1] = 28;
			}
		}
	}
}
