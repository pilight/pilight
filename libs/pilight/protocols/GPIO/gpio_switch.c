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
#include <math.h>
#include <assert.h>

#ifndef _WIN32
	#include <unistd.h>
#endif

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../../core/eventpool.h"
#include "../../config/settings.h"
#include "../protocol.h"
#include "gpio_switch.h"

#if (!defined(__FreeBSD__) && !defined(_WIN32)) || defined(PILIGHT_UNITTEST)
#include <wiringx.h>

typedef struct data_t {
	unsigned int id;
	unsigned int state;
	unsigned int resolution;
	int pollpri;
	uv_poll_t *poll_req;
	uv_timer_t *timer_req;

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
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	snprintf(data->message, 1024, "{\"gpio\":%d,\"state\":\"%s\"}", gpio, ((state == 1) ? "on" : "off"));
	strncpy(data->origin, "receiver", 255);
	data->protocol = gpio_switch->id;
	if(strlen(pilight_uuid) > 0) {
		data->uuid = pilight_uuid; /*LCOV_EXCL_LINE*/
	} else {
		data->uuid = NULL;
	}
	data->repeat = 1;
	eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);
}

static void poll_cb(uv_poll_t *req, int status, int events);

static void restart(uv_timer_t *req) {
	struct data_t *data = req->data;

	uv_poll_start(data->poll_req, data->pollpri, poll_cb);
	uv_timer_stop(req);
}

static void poll_cb(uv_poll_t *req, int status, int events) {
	int fd = req->io_watcher.fd;
	struct data_t *node = req->data;

	if(events & node->pollpri) {
		uint8_t c = 0;

		(void)read(fd, &c, 1);
		lseek(fd, 0, SEEK_SET);

		uv_poll_stop(req);
		pinMode(node->id, PINMODE_INPUT);

		int s = digitalRead(node->id);
		if(s != node->state) {
			node->state = s;
			createMessage(node->id, node->state);
		}

		assert(node->resolution > 0);
		uv_timer_stop(node->timer_req);
		uv_timer_start(node->timer_req, (void (*)(uv_timer_t *))restart, node->resolution, 0);
	}
}

#if defined(__arm__) || defined(__mips__) || defined(PILIGHT_UNITTEST)
static void *addDevice(int reason, void *param, void *userdata) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct data_t *node = NULL;
	int match = 0;
	double itmp = 0.0;

	if(param == NULL) {
		return NULL;
	}

	if((jdevice = json_first_child(param)) == NULL) {
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
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	node->id = 0;
	node->state = 0;
	node->resolution = 1000;

	if(json_find_number(jdevice, "resolution", &itmp) == 0) {
		node->resolution = (int)itmp;
	}

	if((jid = json_find_member(jdevice, "id"))) {
		jchild = json_first_child(jid);
		if(json_find_number(jchild, "gpio", &itmp) == 0) {
			node->id = (int)round(itmp);
			pinMode(node->id, PINMODE_INPUT);
			node->state = digitalRead(node->id);
		}
	}

	createMessage(node->id, node->state);

	if(wiringXISR(node->id, ISR_MODE_BOTH) < 0) {
		logprintf(LOG_ERR, "unable to register interrupt for pin %d", node->id);
		FREE(node);
		return EXIT_SUCCESS;
	}

	if((node->timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	node->timer_req->data = node;
	uv_timer_init(uv_default_loop(), node->timer_req);

	if((node->poll_req = MALLOC(sizeof(uv_poll_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	node->poll_req->data = node;
	int fd = wiringXSelectableFd(node->id);

	uv_poll_init(uv_default_loop(), node->poll_req, fd);

	node->pollpri = UV_PRIORITIZED;
#ifdef PILIGHT_UNITTEST
	char *dev = getenv("PILIGHT_GPIO_SWITCH_READ");
	if(dev == NULL) {
		node->pollpri = UV_PRIORITIZED; /*LCOV_EXCL_LINE*/
	} else {
		node->pollpri = UV_READABLE;
	}
#endif
	uv_poll_start(node->poll_req, node->pollpri, poll_cb);

	node->next = data;
	data = node;

	return NULL;
}
#endif

static int checkValues(struct JsonNode *jvalues) {
	double readonly = 0.0;

#if defined(__arm__) || defined(__mips__) || defined(PILIGHT_UNITTEST)
	struct JsonNode *jid = NULL;
	char *platform = GPIO_PLATFORM;
	if(config_setting_get_string("gpio-platform", 0, &platform) != 0) {
		logprintf(LOG_ERR, "gpio_switch: no gpio-platform configured");
		return -1;
	}
	if(strcmp(platform, "none") == 0) {
		FREE(platform);
		logprintf(LOG_ERR, "gpio_switch: no gpio-platform configured");
		return -1;
	}
	if(wiringXSetup(platform, _logprintf) < 0) {
		FREE(platform);
		logprintf(LOG_ERR, "unable to setup wiringX") ;
		return -1;
	} else if((jid = json_find_member(jvalues, "id"))) {
		FREE(platform);
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

static void gc(void) {
	struct data_t *tmp = NULL;
	while(data) {
		tmp = data;
		data = data->next;
		FREE(tmp);
	}
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

	options_add(&gpio_switch->options, "t", "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&gpio_switch->options, "f", "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&gpio_switch->options, "g", "gpio", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-9]{1}|1[0-9]|20)$");
	options_add(&gpio_switch->options, "r", "resolution", OPTION_HAS_VALUE, DEVICES_OPTIONAL, JSON_NUMBER, (void *)1000, "^[0-9]+$");

	options_add(&gpio_switch->options, "0", "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&gpio_switch->options, "0", "confirm", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

#if (!defined(__FreeBSD__) && !defined(_WIN32)) || defined(PILIGHT_UNITTEST)
	gpio_switch->checkValues=&checkValues;
	gpio_switch->gc = &gc;

	#if defined(__arm__) || defined(__mips__) || defined(PILIGHT_UNITTEST)
		eventpool_callback(REASON_DEVICE_ADDED, addDevice, NULL);
	#endif
#endif
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "gpio_switch";
	module->version = "4.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	gpioSwitchInit();
}
#endif
