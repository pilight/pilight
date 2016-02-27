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
#include <math.h>
#include <sys/stat.h>
#include <sys/time.h>
#ifndef _WIN32
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
#endif
#include <pthread.h>

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../../core/threadpool.h"
#include "../../core/eventpool.h"
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "../../core/json.h"
#ifndef _WIN32
	#include "../../../wiringx/wiringX.h"
#endif
#include "../protocol.h"
#include "lm76.h"

#if !defined(__FreeBSD__) && !defined(_WIN32)
typedef struct data_t {
	char *name;

	unsigned int id;
	int fd;
	int interval;

	double temp_offset;

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

	int raw = wiringXI2CReadReg16(settings->fd, 0x00);
	float temp = ((float)((raw&0x00ff)+((raw>>12)*0.0625)));

	struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
	if(data == NULL) {
		OUT_OF_MEMORY
	}
	snprintf(data->message, 1024, "{\"id\":%d,\"temperature\":%.3f}", settings->id, (temp+settings->temp_offset));
	strncpy(data->origin, "receiver", 255);
	data->protocol = lm76->id;
	if(strlen(pilight_uuid) > 0) {
		data->uuid = pilight_uuid;
	} else {
		data->uuid = NULL;
	}
	data->repeat = 1;
	eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);

	struct timeval tv;
	tv.tv_sec = settings->interval;
	tv.tv_usec = 0;
	threadpool_add_scheduled_work(settings->name, thread, tv, (void *)settings);

	return (void *)NULL;
}

static void *addDevice(void *param) {
	struct threadpool_tasks_t *task = param;
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct data_t *node = NULL;
	struct timeval tv;
	int match = 0, interval = 10;
	double itmp = 0.0;

	if(task->userdata == NULL) {
		return NULL;
	}

	if((jdevice = json_first_child(task->userdata)) == NULL) {
		return NULL;
	}

	if((jprotocols = json_find_member(jdevice, "protocol")) != NULL) {
		jchild = json_first_child(jprotocols);
		while(jchild) {
			if(strcmp(lm76->id, jchild->string_) == 0) {
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
	node->id = 0;
	node->temp_offset = 0;

	if((jid = json_find_member(jdevice, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_number(jchild, "id", &itmp) == 0) {
				node->id = (int)itmp;
			}
			jchild = jchild->next;
		}
	}

	if(json_find_number(jdevice, "poll-interval", &itmp) == 0)
		interval = (int)round(itmp);
	json_find_number(jdevice, "temperature-offset", &node->temp_offset);

	node->fd = wiringXI2CSetup(node->id);
	if(node->fd <= 0) {
		logprintf(LOG_NOTICE, "error connecting to lm76");
		logprintf(LOG_DEBUG, "(probably i2c bus error from wiringXI2CSetup)");
		logprintf(LOG_DEBUG, "(maybe wrong id? use i2cdetect to find out)");
	}

	if((node->name = MALLOC(strlen(jdevice->key)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(node->name, jdevice->key);

	node->interval = interval;

	node->next = data;
	data = node;

	tv.tv_sec = interval;
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
}
#endif

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void lm76Init(void) {
	protocol_register(&lm76);
	protocol_set_id(lm76, "lm76");
	protocol_device_add(lm76, "lm76", "TI I2C Temperature Sensor");
	lm76->devtype = WEATHER;
	lm76->hwtype = SENSOR;
	lm76->multipleId = 0;

	options_add(&lm76->options, 't', "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&lm76->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "0x[0-9a-f]{2}");

	// options_add(&lm76->options, 0, "decimals", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)3, "[0-9]");
	options_add(&lm76->options, 0, "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)3, "[0-9]");
	options_add(&lm76->options, 0, "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

#if !defined(__FreeBSD__) && !defined(_WIN32)
	// lm76->initDev=&initDev;
	lm76->gc=&gc;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice);
#endif
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "lm76";
	module->version = "3.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	lm76Init();
}
#endif
