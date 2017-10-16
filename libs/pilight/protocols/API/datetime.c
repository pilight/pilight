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
#include <signal.h>
#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#define MSG_NOSIGNAL 0
#else
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <netdb.h>
	#include <arpa/inet.h>
#endif
#include <pthread.h>
#include <stdint.h>
#include <time.h>

#include "../../core/threads.h"
#include "../../core/pilight.h"
#include "../../core/socket.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/ntp.h"
#include "../../core/json.h"
#include "../../core/gc.h"
#include "../../core/datetime.h"
#include "datetime.h"

static unsigned short loop = 1;
static unsigned short threads = 0;
static char *format = NULL;

static pthread_mutex_t lock;

static void *thread(void *param) {
	char UTC[] = "UTC";
	struct protocol_threads_t *thread = (struct protocol_threads_t *)param;
	struct JsonNode *json = (struct JsonNode *)thread->param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct JsonNode *jchild1 = NULL;
	struct tm tm;
	char *tz = NULL;
	time_t t;
	int target_offset = 0, counter = 0, dst = 0, x = 0;
	double longitude = 0.0, latitude = 0.0;

	threads++;

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			jchild1 = json_first_child(jchild);
			while(jchild1) {
				if(strcmp(jchild1->key, "longitude") == 0) {
					longitude = jchild1->number_;
				}
				if(strcmp(jchild1->key, "latitude") == 0) {
					latitude = jchild1->number_;
				}
				jchild1 = jchild1->next;
			}
			jchild = jchild->next;
		}
	}

	if((tz = coord2tz(longitude, latitude)) == NULL) {
		logprintf(LOG_INFO, "datetime #%d, could not determine timezone", threads);
		tz = UTC;
	} else {
		logprintf(LOG_INFO, "datetime #%d %.6f:%.6f seems to be in timezone: %s", threads, longitude, latitude, tz);
	}

	t = time(NULL);
	t -= getntpdiff();
	dst = isdst(t, tz);
	if(isntpsynced() == 0) {
		x = 1;
	}

	/* Check how many hours we differ from UTC? */
	target_offset = tzoffset(UTC, tz);

	while(loop) {
		pthread_mutex_lock(&lock);
		t = time(NULL);
		t -= getntpdiff();

		/* Get UTC time */
#ifdef _WIN32
		struct tm *tm1;
		if((tm1 = gmtime(&t)) != NULL) {
			memcpy(&tm, tm1, sizeof(struct tm));
#else
		if(gmtime_r(&t, &tm) != NULL) {
#endif
			int year = tm.tm_year+1900;
			int month = tm.tm_mon+1;
			int day = tm.tm_mday;
			/* Add our hour difference to the UTC time */
			tm.tm_hour += target_offset;
			/* Add possible daylist savings time hour */
			tm.tm_hour += dst;
			int hour = tm.tm_hour;
			int minute = tm.tm_min;
			int second = tm.tm_sec;
			int weekday = tm.tm_wday+1;

			datefix(&year, &month, &day, &hour, &minute, &second);

			if((minute == 0 && second == 0) || (isntpsynced() == 0 && x == 0)) {
				x = 1;
				dst = isdst(t, tz);
			}

			datetime->message = json_mkobject();

			JsonNode *code = json_mkobject();
			json_append_member(code, "longitude", json_mknumber(longitude, 6));
			json_append_member(code, "latitude", json_mknumber(latitude, 6));
			json_append_member(code, "year", json_mknumber(year, 0));
			json_append_member(code, "month", json_mknumber(month, 0));
			json_append_member(code, "day", json_mknumber(day, 0));
			json_append_member(code, "weekday", json_mknumber(weekday, 0));
			json_append_member(code, "hour", json_mknumber(hour, 0));
			json_append_member(code, "minute", json_mknumber(minute, 0));
			json_append_member(code, "second", json_mknumber(second, 0));
			json_append_member(code, "dst", json_mknumber(dst, 0));

			json_append_member(datetime->message, "message", code);
			json_append_member(datetime->message, "origin", json_mkstring("receiver"));
			json_append_member(datetime->message, "protocol", json_mkstring(datetime->id));

			if(pilight.broadcast != NULL) {
				pilight.broadcast(datetime->id, datetime->message, PROTOCOL);
			}

			json_delete(datetime->message);
			datetime->message = NULL;
			counter++;
		}
		pthread_mutex_unlock(&lock);
		sleep(1);
	}
	pthread_mutex_unlock(&lock);

	threads--;
	return (void *)NULL;
}

static struct threadqueue_t *initDev(JsonNode *jdevice) {
	loop = 1;
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	json_free(output);

	struct protocol_threads_t *node = protocol_thread_init(datetime, json);
	return threads_register("datetime", &thread, (void *)node, 0);
}

static void threadGC(void) {
	loop = 0;

	protocol_thread_stop(datetime);
	while(threads > 0) {
		usleep(10);
	}
	protocol_thread_free(datetime);
}

static void gc(void) {
	FREE(format);
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void datetimeInit(void) {

	format = MALLOC(20);
	strcpy(format, "HH:mm:ss YYYY-MM-DD");

	protocol_register(&datetime);
	protocol_set_id(datetime, "datetime");
	protocol_device_add(datetime, "datetime", "Date and Time protocol");
	datetime->devtype = DATETIME;
	datetime->hwtype = API;
	datetime->multipleId = 0;
	datetime->masterOnly = 1;

	options_add(&datetime->options, 'o', "longitude", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&datetime->options, 'a', "latitude", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&datetime->options, 'y', "year", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{3,4}$");
	options_add(&datetime->options, 'm', "month", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{3,4}$");
	options_add(&datetime->options, 'd', "day", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&datetime->options, 'w', "weekday", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&datetime->options, 'h', "hour", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&datetime->options, 'i', "minute", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&datetime->options, 's', "second", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&datetime->options, 'z', "dst", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);

	options_add(&datetime->options, 0, "show-datetime", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&datetime->options, 0, "format", OPTION_HAS_VALUE, GUI_SETTING, JSON_STRING, (void *)format, NULL);

	datetime->initDev=&initDev;
	datetime->threadGC=&threadGC;
	datetime->gc=&gc;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "datetime";
	module->version = "2.6";
	module->reqversion = "6.0";
	module->reqcommit = "115";
}

void init(void) {
	datetimeInit();
}
#endif
