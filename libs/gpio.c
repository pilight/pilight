/*
	Copyright (C) 2013 CurlyMo

	This file is part of the pilight.

    pilight is free software: you can redistribute it and/or modify it under the 
	terms of the GNU General Public License as published by the Free Software 
	Foundation, either version 3 of the License, or (at your option) any later 
	version.

    pilight transceiver is distributed in the hope that it will be useful, but 
	WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
	or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License 
	for more details.

    You should have received a copy of the GNU General Public License
    along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>

#include "log.h"
#include "gc.h"
#include "gpio.h"
#include "settings.h"

/* Stores all requested GPIO pins */
void gpio_deregister(int pin) {
	unsigned i;

	for(i=0; i<pins.nr; i++) {
		if(pins.pins[i] == pin) {
			pins.nr--;
			pins.pins[i] = pins.pins[pins.nr];
		}
	}
}

/* Add pin to be cleaned */
void gpio_register(int pin) {
	gpio_deregister(pin);
	pins.pins[pins.nr++] = pin;
}

/* Requests an interrupt for usage */
int gpio_request(int gpio_pin) {
	char command[50], folder[32];
	int err;
	struct stat s;

	/* Frees GPIO pins on exit */
	gc_attach(gpio_free);
	
	sprintf(command, "echo %d > /sys/class/gpio/export 2>/dev/null", gpio_wiringPi2BCM(gpio_pin));
	if(system(command) != -1) {
		sprintf(folder, "/sys/class/gpio/gpio%d", gpio_wiringPi2BCM(gpio_pin));
		if((err = stat(folder, &s)) != -1 && S_ISDIR(s.st_mode)) {
			gpio_register(gpio_pin);	
			return EXIT_SUCCESS;
		} else {
			logprintf(LOG_ERR, "can't claim gpio pin %d", gpio_pin);
			return EXIT_FAILURE;
		}
	} else {
		logprintf(LOG_ERR, "can't claim gpio pin %d", gpio_pin);
		return EXIT_FAILURE;
	}
}

/* Set the pinmode for a certain GPIO pin */
int gpio_pinmode(int gpio_pin, int mode) {
	char command[50], folder[32];
	int err;
	struct stat s;

	sprintf(folder, "/sys/class/gpio/gpio%d", gpio_wiringPi2BCM(gpio_pin));
	if((err = stat(folder, &s)) != -1 && S_ISDIR(s.st_mode)) {
		if(mode == INPUT) {
			sprintf(command, "echo in > /sys/class/gpio/gpio%d/direction 2>/dev/null", gpio_wiringPi2BCM(gpio_pin));
		} else {
			sprintf(command, "echo out > /sys/class/gpio/gpio%d/direction 2>/dev/null", gpio_wiringPi2BCM(gpio_pin));
		}
	} else {
		logprintf(LOG_ERR, "can't claim gpio pin %d", gpio_pin);
		return EXIT_FAILURE;
	}
	if(system(command) != -1) {
		return EXIT_SUCCESS;
	} else {
		logprintf(LOG_ERR, "can't claim gpio pin %d", gpio_pin);
		return EXIT_FAILURE;
	}
}

/* Free the GPIO pins used */
int gpio_free(void) {
	char command[50], folder[32];
	int err;
	struct stat s;

	int i = 0;
	int x = pins.nr;
	for(i=0;i<x;i++) {
		sprintf(command, "echo %d > /sys/class/gpio/unexport 2>/dev/null", gpio_wiringPi2BCM(pins.pins[i]));
		if(system(command) != -1) {
			sprintf(folder, "/sys/class/gpio/gpio%d", gpio_wiringPi2BCM(pins.pins[i]));
			if((err = stat(folder, &s)) != -1 && S_ISDIR(s.st_mode)) {
				logprintf(LOG_ERR, "can't free gpio pin %d", pins.pins[i]);
				return EXIT_FAILURE;
			}
		} else {
			logprintf(LOG_ERR, "can't free gpio pin %d", pins.pins[i]);
			return EXIT_FAILURE;			
		}
	}
	for(i=0;i<x;i++) {
		gpio_deregister(pins.pins[i]);
	}
	
	return EXIT_SUCCESS;
}

int gpio_wiringPi2BCM(int gpio_pin) {
	int wiringPi2BCM[8] = {17, 18, 21, 22, 23, 24, 25, 4};
	return wiringPi2BCM[gpio_pin];
}