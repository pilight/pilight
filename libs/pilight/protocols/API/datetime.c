/*
	Copyright (C) 2014 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <math.h>
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

#include "../../core/threadpool.h"
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

static char *format = NULL;

typedef struct data_t {
	char *name;
	int id;
	int interval;
	int target_offset;

	double longitude;
	double latitude;

	char *tz;

	struct data_t *next;
} data_t;

static struct data_t *data = NULL;

static void *reason_code_received_free(void *param) {
	struct reason_code_received_t *data = param;
	FREE(data);
	return NULL;
}

static void *thread(void *param) {
	struct threadpool_tasks_t *task = param;
	struct data_t *settings = task->userdata;
	struct tm tm;
	time_t t;
	int dst = 0;

	t = time(NULL);
	t -= getntpdiff();
	dst = isdst(t, settings->tz);

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
		tm.tm_hour += settings->target_offset;
		/* Add possible daylist savings time hour */
		tm.tm_hour += dst;
		int hour = tm.tm_hour;
		int minute = tm.tm_min;
		int second = tm.tm_sec;
		int weekday = tm.tm_wday+1;

		datefix(&year, &month, &day, &hour, &minute, &second);

		struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
		if(data == NULL) {
			OUT_OF_MEMORY
		}
		snprintf(data->message, 1024,
			"{\"longitude\":%.6f,\"latitude\":%.6f,\"year\":%d,\"month\":%d,\"day\":%d,\"weekday\":%d,\"hour\":%d,\"minute\":%d,\"second\":%d,\"dst\":%d}",
			settings->longitude, settings->latitude, year, month, day, weekday, hour, minute, second, dst
		);
		strncpy(data->origin, "receiver", 255);
		data->protocol = datetime->id;
		if(strlen(pilight_uuid) > 0) {
			data->uuid = pilight_uuid;
		} else {
			data->uuid = NULL;
		}
		data->repeat = 1;
		eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);

		struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		threadpool_add_scheduled_work(settings->name, thread, tv, (void *)settings);
	}

	return (void *)NULL;
}

static void *addDevice(void *param) {
	struct threadpool_tasks_t *task = param;
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct JsonNode *jchild1 = NULL;
	struct data_t *node = NULL;
	struct timeval tv;
	char UTC[] = "UTC";
	int match = 0;

	if(task->userdata == NULL) {
		return NULL;
	}

	if(!(datetime->masterOnly == 0 || pilight.runmode == STANDALONE)) {
		return NULL;
	}

	if((jdevice = json_first_child(task->userdata)) == NULL) {
		return NULL;
	}

	if((jprotocols = json_find_member(jdevice, "protocol")) != NULL) {
		jchild = json_first_child(jprotocols);
		while(jchild) {
			if(strcmp(datetime->id, jchild->string_) == 0) {
				match = 1;
				break;
			}
			jchild = jchild->next;
		}
	}

	if(match == 0) {
		return NULL;
	}

	if((node = MALLOC(sizeof(struct data_t)))== NULL) {
		OUT_OF_MEMORY
	}
	memset(node, '\0', sizeof(struct data_t));

	if((jid = json_find_member(jdevice, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			jchild1 = json_first_child(jchild);
			while(jchild1) {
				if(strcmp(jchild1->key, "longitude") == 0) {
					node->longitude = jchild1->number_;
				}
				if(strcmp(jchild1->key, "latitude") == 0) {
					node->latitude = jchild1->number_;
				}
				jchild1 = jchild1->next;
			}
			jchild = jchild->next;
		}
	}

	if((node->tz = coord2tz(node->longitude, node->latitude)) == NULL) {
		logprintf(LOG_INFO, "datetime %s, could not determine timezone", jdevice->key);
		node->tz = UTC;
	} else {
		logprintf(LOG_INFO, "datetime %s %.6f:%.6f seems to be in timezone: %s", jdevice->key, node->longitude, node->latitude, node->tz);
	}

	node->target_offset = tzoffset(UTC, node->tz);

	if((node->name = MALLOC(strlen(jdevice->key)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(node->name, jdevice->key);

	node->next = data;
	data = node;

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	threadpool_add_scheduled_work(jdevice->key, thread, tv, (void *)node);

	return NULL;
}

static void gc(void) {
	struct data_t *tmp = NULL;
	while(data) {
		tmp = data;
		FREE(tmp->name);
		data = data->next;
		FREE(tmp);
	}
	if(data != NULL) {
		FREE(data);
	}
	FREE(format);
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void datetimeInit(void) {

	if((format = MALLOC(20)) == NULL) {
		OUT_OF_MEMORY
	}
	memset(format, 0, 20);
	strncpy(format, "HH:mm:ss YYYY-MM-DD", 19);

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

	// datetime->initDev=&initDev;
	datetime->gc=&gc;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice);
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "datetime";
	module->version = "4.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	datetimeInit();
}
#endif
