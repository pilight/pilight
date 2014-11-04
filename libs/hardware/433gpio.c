/*
	Copyright (C) 2013 - 2014 CurlyMo

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
#include <unistd.h>

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "hardware.h"
#include "wiringX.h"
#include "json.h"
#include "irq.h"
#include "433gpio.h"

static int gpio_433_in = 0;
static int gpio_433_out = 0;
static int gpio_433_initialized = 0;

static unsigned short gpio433HwInit(void) {
	if(wiringXSetup() == -1) {
		return EXIT_FAILURE;
	}
	gpio_433_initialized = 1;
	if(gpio_433_out >= 0) {
		if(wiringXValidGPIO(gpio_433_out) != 0) {
			logprintf(LOG_ERR, "invalid sender pin: %d", gpio_433_out);
			return EXIT_FAILURE;
		}
		pinMode(gpio_433_out, OUTPUT);
	}
	if(gpio_433_in >= 0) {
		if(wiringXValidGPIO(gpio_433_in) != 0) {
			logprintf(LOG_ERR, "invalid receiver pin: %d", gpio_433_in);
			return EXIT_FAILURE;
		}
		if(wiringXISR(gpio_433_in, INT_EDGE_BOTH) < 0) {
			logprintf(LOG_ERR, "unable to register interrupt for pin %d", gpio_433_in);
			return EXIT_SUCCESS;
		}
	}
	return EXIT_SUCCESS;
}

static unsigned short gpio433HwDeinit(void) {
	return EXIT_SUCCESS;
}

static int gpio433Send(int *code) {
	unsigned short i = 0;
	if(gpio_433_out >= 0) {
		while(code[i]) {
			digitalWrite(gpio_433_out, 1);
			usleep((__useconds_t)code[i++]);
			digitalWrite(gpio_433_out, 0);
			usleep((__useconds_t)code[i++]);
		}
	} else {
		sleep(1);
	}
	return EXIT_SUCCESS;
}

static int gpio433Receive(void) {
	if(gpio_433_in >= 0) {
		return irq_read(gpio_433_in);
	} else {
		sleep(1);
		return 0;
	}
}

static unsigned short gpio433Settings(JsonNode *json) {
	if(strcmp(json->key, "receiver") == 0) {
		if(json->tag == JSON_NUMBER) {
			gpio_433_in = (int)json->number_;
		} else {
			return EXIT_FAILURE;
		}
	}
	if(strcmp(json->key, "sender") == 0) {
		if(json->tag == JSON_NUMBER) {
			gpio_433_out = (int)json->number_;
		} else {
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

#ifndef MODULE
__attribute__((weak))
#endif
void gpio433Init(void) {
	hardware_register(&gpio433);
	hardware_set_id(gpio433, "433gpio");

	options_add(&gpio433->options, 'r', "receiver", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9-]+$");
	options_add(&gpio433->options, 's', "sender", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9-]+$");

	gpio433->type=RF433;
	gpio433->init=&gpio433HwInit;
	gpio433->deinit=&gpio433HwDeinit;
	gpio433->send=&gpio433Send;
	gpio433->receive=&gpio433Receive;
	gpio433->settings=&gpio433Settings;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "433gpio";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	gpio433Init();
}
#endif
