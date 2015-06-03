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

#include "../core/pilight.h"
#include "../core/common.h"
#include "../core/dso.h"
#include "../core/log.h"
#include "../core/json.h"
#include "../core/irq.h"
#include "../config/hardware.h"
#include "../../wiringx/wiringX.h"
#include "IRgpio.h"

static int gpio_ir_in = 0;
static int gpio_ir_out = 0;

static unsigned short gpioIRHwInit(void) {
	if(wiringXSupported() == 0) {
		if(wiringXSetup() == -1) {
			return EXIT_FAILURE;
		}
		if(gpio_ir_out >= 0) {
			if(wiringXValidGPIO(gpio_ir_out) != 0) {
				logprintf(LOG_ERR, "invalid sender pin: %d", gpio_ir_out);
				return EXIT_FAILURE;
			}
			pinMode(gpio_ir_out, OUTPUT);
		}
		if(gpio_ir_in >= 0) {
			if(wiringXValidGPIO(gpio_ir_in) != 0) {
				logprintf(LOG_ERR, "invalid receiver pin: %d", gpio_ir_in);
				return EXIT_FAILURE;
			}
			if(wiringXISR(gpio_ir_in, INT_EDGE_BOTH) < 0) {
				logprintf(LOG_ERR, "unable to register interrupt for pin %d", gpio_ir_in);
				return EXIT_SUCCESS;
			}
		}
		return EXIT_SUCCESS;
	} else {
		logprintf(LOG_ERR, "the IRgpio module is not supported on this hardware", gpio_ir_in);
		return EXIT_FAILURE;
	}
}

static unsigned short gpioIRHwDeinit(void) {
	return EXIT_SUCCESS;
}

static int gpioIRSend(int *code, int rawlen, int repeats) {
	int r = 0, x = 0;
	if(gpio_ir_out >= 0) {
		for(r=0;r<repeats;r++) {
			for(x=0;x<rawlen;x+=2) {
				digitalWrite(gpio_ir_out, 1);
				usleep((__useconds_t)code[x]);
				digitalWrite(gpio_ir_out, 0);
				if(x+1 < rawlen) {
					usleep((__useconds_t)code[x+1]);
				}
			}
		}
		digitalWrite(gpio_ir_out, 0);
	} else {
		sleep(1);
	}
	return EXIT_SUCCESS;
}

static int gpioIRReceive(void) {
	if(gpio_ir_in >= 0) {
		return irq_read(gpio_ir_in);
	} else {
		sleep(1);
		return 0;
	}
}

static unsigned short gpioIRSettings(JsonNode *json) {
	if(strcmp(json->key, "receiver") == 0) {
		if(json->tag == JSON_NUMBER) {
			gpio_ir_in = (int)json->number_;
		} else {
			return EXIT_FAILURE;
		}
	}
	if(strcmp(json->key, "sender") == 0) {
		if(json->tag == JSON_NUMBER) {
			gpio_ir_out = (int)json->number_;
		} else {
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void gpioIRInit(void) {
	hardware_register(&gpioIR);
	hardware_set_id(gpioIR, "IRgpio");

	options_add(&gpioIR->options, 'r', "receiver", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9-]+$");
	options_add(&gpioIR->options, 's', "sender", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9-]+$");

	gpioIR->minrawlen = 1000;
	gpioIR->maxrawlen = 0;
	gpioIR->mingaplen = 10000;
	gpioIR->maxgaplen = 100000;	
	
	gpioIR->hwtype=RFIR;
	gpioIR->comtype=COMOOK;
	gpioIR->init=&gpioIRHwInit;
	gpioIR->deinit=&gpioIRHwDeinit;
	gpioIR->sendOOK=&gpioIRSend;
	gpioIR->receiveOOK=&gpioIRReceive;
	gpioIR->settings=&gpioIRSettings;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "IRgpio";
	module->version = "1.2";
	module->reqversion = "5.0";
	module->reqcommit = "86";
}

void init(void) {
	gpioIRInit();
}
#endif
