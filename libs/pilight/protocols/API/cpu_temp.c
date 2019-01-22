/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>
#include <assert.h>
#ifndef _WIN32
	#include <unistd.h>
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
#endif

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/json.h"
#include "cpu_temp.h"

static char cpu_path[PATH_MAX] = "/sys/class/thermal/thermal_zone0/temp";

typedef struct data_t {
	char *name;
	int id;
	int interval;
	uv_timer_t *timer_req;
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
	uv_timer_t *timer_req = param;
	struct data_t *settings = timer_req->data;
	FILE *fp = NULL;
	struct stat st;
	char *content = NULL;
	size_t bytes = 0;

	if((fp = fopen(cpu_path, "rb"))) {
		fstat(fileno(fp), &st);
		bytes = (size_t)st.st_size;
		if((content = REALLOC(content, bytes+1)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		memset(content, '\0', bytes+1);

		if(fread(content, sizeof(char), bytes, fp) == -1) {
			logprintf(LOG_NOTICE, "cannot read file: %s", cpu_path);
			fclose(fp);
			return NULL;
		} else {
			fclose(fp);
			double temp = atof(content)+settings->temp_offset;
			FREE(content);

			struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
			if(data == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			snprintf(data->message, 1024, "{\"id\":%d,\"temperature\":%.3f}", settings->id, (temp/1000));
			strcpy(data->origin, "receiver");
			data->protocol = cpuTemp->id;
			if(strlen(pilight_uuid) > 0) {
				data->uuid = pilight_uuid;
			} else {
				data->uuid = NULL;
			}
			data->repeat = 1;
			eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);
		}
	} else {
		logprintf(LOG_NOTICE, "CPU sysfs \"%s\" does not exists", cpu_path);
	}

	return (void *)NULL;
}

static void *adaptDevice(int reason, void *param, void *userdata) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jpath = NULL;

	if(param == NULL) {
		return NULL;
	}

	if((jdevice = json_first_child(param)) == NULL) {
		return NULL;
	}

	if(strcmp(jdevice->key, "cpu_temp") != 0) {
		return NULL;
	}

	if((jpath = json_find_member(jdevice, "path")) != NULL) {
		if(jpath->tag == JSON_STRING && strlen(jpath->string_) < PATH_MAX) {
			strcpy(cpu_path, jpath->string_);
		}
	}

	return NULL;
}

static void *addDevice(int reason, void *param, void *userdata) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct data_t *node = NULL;
	FILE *fp = NULL;
	double itmp = 0;
	int match = 0;

	if(param == NULL) {
		return NULL;
	}

	if((jdevice = json_first_child(param)) == NULL) {
		return NULL;
	}

	if((jprotocols = json_find_member(jdevice, "protocol")) != NULL) {
		jchild = json_first_child(jprotocols);
		while(jchild) {
			if(strcmp(cpuTemp->id, jchild->string_) == 0) {
				match = 1;
				break;
		}
			jchild = jchild->next;
		}
	}

	if(match == 0) {
		return NULL;
	}

	if(!(fp = fopen(cpu_path, "rb"))) {
		logprintf(LOG_NOTICE, "CPU sysfs \"%s\" does not exists", cpu_path);
		return NULL;
	} else {
		fclose(fp);
	}

	if((node = MALLOC(sizeof(struct data_t)))== NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(node, '\0', sizeof(struct data_t));
	node->interval = 10;
	node->temp_offset = 0;

	if((jid = json_find_member(jdevice, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_number(jchild, "id", &itmp) == 0) {
				node->id = (int)round(itmp);
			}
			jchild = jchild->next;
		}
	}

	if(json_find_number(jdevice, "poll-interval", &itmp) == 0)
		node->interval = (int)round(itmp);
	json_find_number(jdevice, "temperature-offset", &node->temp_offset);

	if((node->name = MALLOC(strlen(jdevice->key)+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(node->name, jdevice->key);
	node->timer_req = NULL;
	node->next = data;
	data = node;

	if((node->timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	node->timer_req->data = node;
	uv_timer_init(uv_default_loop(), node->timer_req);
	assert(node->interval > 0);
	uv_timer_start(node->timer_req, (void (*)(uv_timer_t *))thread, node->interval*1000, node->interval*1000);

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

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void cpuTempInit(void) {
	protocol_register(&cpuTemp);
	protocol_set_id(cpuTemp, "cpu_temp");
	protocol_device_add(cpuTemp, "cpu_temp", "CPU temperature sensor");
	cpuTemp->devtype = WEATHER;
	cpuTemp->hwtype = SENSOR;
	cpuTemp->multipleId = 0;

	options_add(&cpuTemp->options, "t", "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");
	options_add(&cpuTemp->options, "i", "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "[0-9]");

	options_add(&cpuTemp->options, "0", "temperature-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&cpuTemp->options, "0", "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)3, "[0-9]");
	options_add(&cpuTemp->options, "0", "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&cpuTemp->options, "0", "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)10, "[0-9]");

	// cpuTemp->initDev=&initDev;
	cpuTemp->gc=&gc;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice, NULL);
	eventpool_callback(REASON_DEVICE_ADAPT, adaptDevice, NULL);
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "cpu_temp";
	module->version = "3.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	cpuTempInit();
}
#endif
