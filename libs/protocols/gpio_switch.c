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
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "gpio_switch.h"
#include "irq.h"
#include "gc.h"
#include "wiringX.h"

static unsigned short gpio_switch_loop = 1;
static int gpio_switch_threads = 0;

static void gpioSwitchCreateMessage(int gpio, int state) {
	gpio_switch->message = json_mkobject();
	JsonNode *code = json_mkobject();
	json_append_member(code, "gpio", json_mknumber(gpio));
	if(state) {
		json_append_member(code, "state", json_mkstring("on"));
	} else {
		json_append_member(code, "state", json_mkstring("off"));
	}

	json_append_member(gpio_switch->message, "message", code);
	json_append_member(gpio_switch->message, "origin", json_mkstring("receiver"));
	json_append_member(gpio_switch->message, "protocol", json_mkstring(gpio_switch->id));

	pilight.broadcast(gpio_switch->id, gpio_switch->message);
	json_delete(gpio_switch->message);
	gpio_switch->message = NULL;
}

static void *gpioSwitchParse(void *param) {
	struct protocol_threads_t *node = (struct protocol_threads_t *)param;
	struct JsonNode *json = (struct JsonNode *)node->param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	int id = 0, state = 0, nstate = 0;
	double itmp = 0.0;

	gpio_switch_threads++;

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		if(json_find_number(jchild, "gpio", &itmp) == 0) {
			id = (int)round(itmp);
			if(wiringXISR(id, INT_EDGE_BOTH) < 0) {
				gpio_switch_threads--;
				return NULL;
			}
			state = digitalRead(id);
		}
	}

	gpioSwitchCreateMessage(id, state);

	while(gpio_switch_loop) {
		irq_read(id);
		nstate = digitalRead(id);
		if(nstate != state) {
			state = nstate;
			gpioSwitchCreateMessage(id, state);
			usleep(100000);
		}
	}

	gpio_switch_threads--;
	return (void *)NULL;
}

static struct threadqueue_t *gpioSwitchInitDev(JsonNode *jdevice) {
	gpio_switch_loop = 1;
	wiringXSetup();
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	sfree((void *)&output);

	struct protocol_threads_t *node = protocol_thread_init(gpio_switch, json);
	return threads_register("gpio_switch", &gpioSwitchParse, (void *)node, 0);
}

static int gpioSwitchCheckValues(struct JsonNode *jvalues) {
	struct JsonNode *jid = NULL;

	if((jid = json_find_member(jvalues, "id"))) {
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
	return 0;
}

static void gpioSwitchThreadGC(void) {
	gpio_switch_loop = 0;
	protocol_thread_stop(gpio_switch);
	while(gpio_switch_threads > 0) {
		usleep(10);
	}
	protocol_thread_free(gpio_switch);
}

#ifndef MODULE
__attribute__((weak))
#endif
void gpioSwitchInit(void) {

	protocol_register(&gpio_switch);
	protocol_set_id(gpio_switch, "gpio_switch");
	protocol_device_add(gpio_switch, "gpio_switch", "GPIO as a switch");
	gpio_switch->devtype = SWITCH;
	gpio_switch->hwtype = SENSOR;

	options_add(&gpio_switch->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&gpio_switch->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&gpio_switch->options, 'g', "gpio", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]{1}|1[0-9]|20)$");

	options_add(&gpio_switch->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	gpio_switch->initDev=&gpioSwitchInitDev;
	gpio_switch->threadGC=&gpioSwitchThreadGC;	
	gpio_switch->checkValues=&gpioSwitchCheckValues;	
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "gpio_switch";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	gpioSwitchInit();
}
#endif
