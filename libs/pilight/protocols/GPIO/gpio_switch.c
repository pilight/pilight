/*
	Copyright (C) 2014 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../../core/gc.h"
#include "../../core/eventpool.h"
#include "../../core/threadpool.h"
#include "../protocol.h"
#include "gpio_switch.h"

#if !defined(__FreeBSD__) && !defined(_WIN32)
#include "../../../wiringx//wiringX.h"

typedef struct data_t {
	unsigned int id;
	unsigned int state;

	struct data_t *next;
} data_t;

static struct data_t *data = NULL;

static void *reason_code_received_free(void *param) {
	struct reason_code_received_t *data = param;
	FREE(data);
	return NULL;
}

static void createMessage(int gpio, int state) {
	struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
	if(data == NULL) {
		OUT_OF_MEMORY
	}
	snprintf(data->message, 1024, "{\"gpio\":%d,\"state\":\"%s\"}", gpio, ((state == 1) ? "on" : "off"));
	strncpy(data->origin, "receiver", 255);
	data->protocol = gpio_switch->id;
	if(strlen(pilight_uuid) > 0) {
		data->uuid = pilight_uuid;
	} else {
		data->uuid = NULL;
	}
	data->repeat = 1;
	eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);
}

static int client_callback(struct eventpool_fd_t *node, int event) {
	struct data_t *data = node->userdata;

	switch(event) {
		case EV_CONNECT_SUCCESS: {
			eventpool_fd_enable_highpri(node);
		} break;
		case EV_HIGHPRI: {
			uint8_t c = 0;
			unsigned int nstate = 0;

			(void)read(node->fd, &c, 1);
			lseek(node->fd, 0, SEEK_SET);

			nstate = digitalRead(data->id);

			if(nstate != data->state) {
				data->state = nstate;
				createMessage(data->id, data->state);
			}

			eventpool_fd_enable_highpri(node);
		} break;
		case EV_DISCONNECTED: {
			FREE(node->userdata);
			eventpool_fd_remove(node);
		} break;
	}
	return 0;
}

static void *addDevice(void *param) {
	struct threadpool_tasks_t *task = param;
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct data_t *node = NULL;
	int match = 0;
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
			if(strcmp(gpio_switch->id, jchild->string_) == 0) {
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
	node->state = 0;

	if((jid = json_find_member(jdevice, "id"))) {
		jchild = json_first_child(jid);
		if(json_find_number(jchild, "gpio", &itmp) == 0) {
			node->id = (int)round(itmp);
			node->state = digitalRead(node->id);
		}
	}

	createMessage(node->id, node->state);

	int fd = wiringXSelectableFd(node->id);

	node->next = data;
	data = node;

	eventpool_fd_add("gpio_switch", fd, client_callback, NULL, data);

	return NULL;
}

static int checkValues(struct JsonNode *jvalues) {
	double readonly = 0.0;

#if defined(__arm__) || defined(__mips__)	
	struct JsonNode *jid = NULL;
	char *platform = GPIO_PLATFORM;
	if(settings_select_string(ORIGIN_MASTER, "gpio-platform", &platform) != 0 || strcmp(platform, "none") == 0) {
		logprintf(LOG_ERR, "gpio_switch: no gpio-platform configured");
		return -1;
	}	
	if(wiringXSetup(platform, logprintf) < 0) {
		logprintf(LOG_ERR, "unable to setup wiringX") ;
		return -1;
	} else if((jid = json_find_member(jvalues, "id"))) {
		struct JsonNode *jchild = NULL;
		struct JsonNode *jchild1 = NULL;

		jchild = json_first_child(jid);
		while(jchild) {
			jchild1 = json_first_child(jchild);
			while(jchild1) {
				if(strcmp(jchild1->key, "gpio") == 0) {
					if(wiringXValidGPIO((int)round(jchild1->number_)) != 0) {
						return -1;
					}
				}
				jchild1 = jchild1->next;
			}
			jchild = jchild->next;
		}
	}
#endif

	if(json_find_number(jvalues, "readonly", &readonly) == 0) {
		if((int)readonly != 1) {
			return -1;
		}
	}

	return 0;
}
#endif

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void gpioSwitchInit(void) {

	protocol_register(&gpio_switch);
	protocol_set_id(gpio_switch, "gpio_switch");
	protocol_device_add(gpio_switch, "gpio_switch", "GPIO as a switch");
	gpio_switch->devtype = SWITCH;
	gpio_switch->hwtype = SENSOR;
	gpio_switch->multipleId = 0;

	options_add(&gpio_switch->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&gpio_switch->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&gpio_switch->options, 'g', "gpio", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-9]{1}|1[0-9]|20)$");

	options_add(&gpio_switch->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&gpio_switch->options, 0, "confirm", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

#if !defined(__FreeBSD__) && !defined(_WIN32)
	// gpio_switch->initDev=&initDev;
	// gpio_switch->gc=&gc;
	gpio_switch->checkValues=&checkValues;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice);
#endif
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "gpio_switch";
	module->version = "3.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	gpioSwitchInit();
}
#endif
