/*
	Copyright (C) 2014 CurlyMo

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
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#ifndef __USE_XOPEN
	#define __USE_XOPEN
#endif
#include <time.h>
#include <math.h>

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "../pilight/datetime.h" // Full path because we also have a datetime protocol
#include "log.h"
#include "threads.h"
#include "protocol.h"
#include "hardware.h"
#include "json.h"
#include "gc.h"
#include "sunriseset.h"

#define PI 3.1415926
#define PIX 57.29578049044297 // 180 / PI
#define ZENITH 90.83333333333333

static unsigned short sunriseset_loop = 1;
static unsigned short sunriseset_threads = 0;

static double sunRiseSetCalculate(int year, int month, int day, double lon, double lat, int rising, int tz) {
	int N = (int)((floor(275 * month / 9)) - ((floor((month + 9) / 12)) *
			((1 + floor((year - 4 * floor(year / 4) + 2) / 3)))) + (int)day - 30);

	double lngHour = lon / 15.0;
	double T = 0;

	if(rising) {
		T = N + ((6 - lngHour) / 24);
	} else {
		T = N + ((18 - lngHour) / 24);
	}

	double M = (0.9856 * T) - 3.289;
	double M1 = M * PI / 180;
	double L = fmod((M + (1.916 * sin(M1)) + (0.020 * sin(2 * M1)) + 282.634), 360.0);
	double L1 = L * PI / 180;
	double L2 = lat * PI / 180;
	double SD = 0.39782 * sin(L1);
	double CH = (cos(ZENITH * PI / 180)-(SD * sin(L2))) / (cos((PIX * asin((SD))) * PI / 180) * cos(L2));

	if(CH > 1) {
		return -1;
	} else if(CH < -1) {
		return -1;
	}

	double RA = fmod((PIX * atan((0.91764 * tan(L1)))), 360);
	double MQ = (RA + (((floor(L / 90)) * 90) - ((floor(RA / 90)) * 90))) / 15;
	double A;
	double B;
	if(!rising) {
		A = 0.06571;
		B = 6.595;
	} else {
		A = 0.06571;
		B = 6.618;
	}

	double t = ((rising ? 360 - PIX * acos(CH) : PIX * acos(CH)) / 15) + MQ - (A * T) - B;
	double UT = fmod((t - lngHour) + 24.0, 24.0);
	double min = (round(60*fmod(UT, 1))/100);

	if(min >= 0.60) {
		min -= 0.60;
	}

	double hour = UT-min;

	return ((round(hour)+min)+tz)*100;
}

static void *sunRiseSetParse(void *param) {
	struct protocol_threads_t *thread = (struct protocol_threads_t *)param;
	struct JsonNode *json = (struct JsonNode *)thread->param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct JsonNode *jchild1 = NULL;
	char *slongitude = NULL, *slatitude = NULL, *tz = NULL;
	double longitude = 0, latitude = 0;
	char UTC[] = "Europe/London";

	time_t timenow = 0;
	struct tm *current = NULL;
	int month = 0, mday = 0, year = 0, offset = 0, nrloops = 0;
	int hour = 0, min = 0, sec = 0, risetime = 0, settime = 0;

	sunriseset_threads++;

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			jchild1 = json_first_child(jchild);
			while(jchild1) {
				if(strcmp(jchild1->key, "longitude") == 0) {
					if(!(slongitude = malloc(strlen(jchild1->string_)+1))) {
						logprintf(LOG_ERR, "out of memory");
						exit(EXIT_FAILURE);
					}
					strcpy(slongitude, jchild1->string_);
					longitude = atof(jchild1->string_);
				}
				if(strcmp(jchild1->key, "latitude") == 0) {
					if(!(slatitude = malloc(strlen(jchild1->string_)+1))) {
						logprintf(LOG_ERR, "out of memory");
						exit(EXIT_FAILURE);
					}
					strcpy(slatitude, jchild1->string_);
					latitude = atof(jchild1->string_);
				}
				jchild1 = jchild1->next;
			}
			jchild = jchild->next;
		}
	}

	if((tz = coord2tz(longitude, latitude)) == NULL) {
		logprintf(LOG_DEBUG, "could not determine timezone");
		tz = UTC;
	} else {
		logprintf(LOG_DEBUG, "%s:%s seems to be in timezone: %s", slongitude, slatitude, tz);
	}

	while(sunriseset_loop) {
		protocol_thread_wait(thread, 1, &nrloops);
		timenow = time(NULL);
		current = localtztime(tz, timenow);

		sec = current->tm_sec;
		min = current->tm_min;
		hour = current->tm_hour;
		month = current->tm_mon+1;
		mday = current->tm_mday;
		year = current->tm_year+1900;

		offset = tzoffset(UTC, tz);

		int hournow = (hour*100)+min;

		if(((hournow == 0 || hournow == risetime || hournow == settime) && sec == 0)
		   || (settime == 0 && risetime == 0)) {
			sunriseset->message = json_mkobject();
			JsonNode *code = json_mkobject();
			risetime = (int)sunRiseSetCalculate(year, month, mday, longitude, latitude, 1, offset);
			settime = (int)sunRiseSetCalculate(year, month, mday, longitude, latitude, 0, offset);

			if(isdst(tz)) {
				risetime += 100;
				settime += 100;
				if(risetime > 2400) risetime -= 2400;
				if(settime > 2400) settime -= 2400;
			}

			json_append_member(code, "longitude", json_mkstring(slongitude));
			json_append_member(code, "latitude", json_mkstring(slatitude));

			/* Only communicate the sun state change when they actually occur,
			   and only communicate the new times when the day changes */
			if(hournow != 0) {
				if(hournow >= risetime && hournow < settime) {
					json_append_member(code, "sun", json_mkstring("rise"));
				} else {
					json_append_member(code, "sun", json_mkstring("set"));
				}
			} else {
				json_append_member(code, "sunrise", json_mknumber(risetime));
				json_append_member(code, "sunset", json_mknumber(settime));
			}

			json_append_member(sunriseset->message, "message", code);
			json_append_member(sunriseset->message, "origin", json_mkstring("receiver"));
			json_append_member(sunriseset->message, "protocol", json_mkstring(sunriseset->id));

			pilight.broadcast(sunriseset->id, sunriseset->message);
			json_delete(sunriseset->message);
			sunriseset->message = NULL;
		}
	}

	sfree((void *)&slatitude);
	sfree((void *)&slongitude);

	sunriseset_threads--;
	return (void *)NULL;
}

struct threadqueue_t *sunRiseSetInitDev(JsonNode *jdevice) {
	sunriseset_loop = 1;
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	sfree((void *)&output);

	struct protocol_threads_t *node = protocol_thread_init(sunriseset, json);
	return threads_register("sunriseset", &sunRiseSetParse, (void *)node, 0);
}

static void sunRiseSetThreadGC(void) {
	sunriseset_loop = 0;
	protocol_thread_stop(sunriseset);
	while(sunriseset_threads > 0) {
		usleep(10);
	}
	protocol_thread_free(sunriseset);
}

static int sunRiseSetCheckValues(JsonNode *code) {
	char *sun = NULL;

	if(json_find_string(code, "sun", &sun) == 0) {
		if(strcmp(sun, "rise") != 0 && strcmp(sun, "set") != 0) {
			return 1;
		}
	}
	return 0;
}

#ifndef MODULE
__attribute__((weak))
#endif
void sunRiseSetInit(void) {

	protocol_register(&sunriseset);
	protocol_set_id(sunriseset, "sunriseset");
	protocol_device_add(sunriseset, "sunriseset", "Sunrise / Sunset Calculator");
	sunriseset->devtype = WEATHER;
	sunriseset->hwtype = API;
	sunriseset->multipleId = 0;

	options_add(&sunriseset->options, 'o', "longitude", OPTION_HAS_VALUE, CONFIG_ID, JSON_STRING, NULL, NULL);
	options_add(&sunriseset->options, 'a', "latitude", OPTION_HAS_VALUE, CONFIG_ID, JSON_STRING, NULL, NULL);
	options_add(&sunriseset->options, 'u', "sunrise", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{3,4}$");
	options_add(&sunriseset->options, 'd', "sunset", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{3,4}$");
	options_add(&sunriseset->options, 's', "sun", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_STRING, NULL, NULL);

	options_add(&sunriseset->options, 0, "device-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&sunriseset->options, 0, "gui-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&sunriseset->options, 0, "gui-show-sunriseset", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	sunriseset->initDev=&sunRiseSetInitDev;
	sunriseset->threadGC=&sunRiseSetThreadGC;
	sunriseset->checkValues=&sunRiseSetCheckValues;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "sunriseset";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	sunRiseSetInit();
}
#endif
