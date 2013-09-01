/*
	Copyright (C) 2013 CurlyMo

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

#include <stdlib.h>
#include <sys/ioctl.h>
#include "settings.h"

#include "../lirc/hardware.h"

#include "irq.h"
#include "wiringPi.h"
#include "log.h"

#include "protocol.h"
#include "../../protocols/arctech_switch.h"
#include "../../protocols/arctech_dimmer.h"
#include "../../protocols/arctech_old.h"
#include "../../protocols/sartano.h"
#include "../../protocols/impuls.h"
#include "../../protocols/alecto.h"
#include "../../protocols/raw.h"
#include "../../protocols/relay.h"
#include "../../protocols/generic_weather.h"

#include "hardware.h"

/* The frequency on which the lirc module needs to send 433.92Mhz*/
int freq = FREQ433; // Khz
/* Is the module initialized */
int initialized = 0;
/* Is the right frequency set */
int setfreq = 0;

void hw_init(void) {
	arctechSwInit();
	arctechDimInit();
	arctechOldInit();
	sartanoInit();
	impulsInit();
	relayInit();
	rawInit();
	alectoInit();
	genWeatherInit();
}

/* Initialize the hardware module lirc_rpi */
int module_init(void) {
	char *mode = NULL;
	int allocated = 0;
	int gpio_in = GPIO_IN_PIN;
	int gpio_out = GPIO_OUT_PIN;

	if(settings_find_string("hw-mode", &mode) != 0) {
		mode = malloc(strlen(HW_MODE)+1);
		strcpy(mode, HW_MODE);
		allocated = 1;
	}
	settings_find_number("gpio-receiver", &gpio_in);
	settings_find_number("gpio-sender", &gpio_out);

	if(strcmp(mode, "module") == 0) {
		if(initialized == 0) {
			if(!hw.init_func()) {
				if(allocated) {
					free(mode);
				}
				exit(EXIT_FAILURE);
			}

			/* Only set the frequency once */
			if(setfreq == 0) {
				freq = FREQ433;
				/* Set the lirc_rpi frequency to 433.92Mhz */
				if(ioctl(hw.fd, _IOW('i', 0x00000013, __u32), &freq) == -1) {
					logprintf(LOG_ERR, "could not set lirc_rpi send frequency");
					if(allocated) {
						free(mode);
					}
					exit(EXIT_FAILURE);
				}
				setfreq = 1;
			}
			logprintf(LOG_DEBUG, "initialized lirc_rpi module");
			initialized = 1;
		}
		if(allocated) {
			free(mode);
		}
		return EXIT_SUCCESS;
	} else if(strcmp(mode, "gpio") == 0) {
		if(wiringPiSetup() == -1) {
			if(allocated) {
				free(mode);
			}
			return EXIT_FAILURE;
		}
		pinMode(gpio_out, OUTPUT);
		if(wiringPiISR(gpio_in, INT_EDGE_BOTH) < 0) {
			logprintf(LOG_ERR, "unable to register interrupt for pin %d", gpio_in) ;
			if(allocated) {
				free(mode);
			}
			return EXIT_SUCCESS;
		}
	}
	if(allocated) {
		free(mode);
	}
	return EXIT_FAILURE;
}

/* Release the hardware module lirc_rpi */
int module_deinit(void) {
	char *mode = NULL;
	int allocated = 0;
	
	if(settings_find_string("hw-mode", &mode) != 0) {
		mode = malloc(strlen(HW_MODE)+1);
		strcpy(mode, HW_MODE);
		allocated = 1;
	}

	if(strcmp(mode, "module") == 0) {
		if(initialized == 1) {
			freq = FREQ38;
			if(ioctl(hw.fd, _IOW('i', 0x00000013, __u32), &freq) == -1) {
				logprintf(LOG_ERR, "could not restore default freq of the lirc_rpi module");
				if(allocated) {
					free(mode);
				}
				exit(EXIT_FAILURE);
			} else {
				logprintf(LOG_DEBUG, "default freq of the lirc_rpi module set");
			}
			/* Restore the lirc_rpi frequency to its default value */
			hw.deinit_func();

			logprintf(LOG_DEBUG, "deinitialized lirc_rpi module");
			initialized	= 0;
		}
		if(allocated) {
			free(mode);
		}
		return EXIT_SUCCESS;
	}
	if(allocated) {
		free(mode);
	}
	return EXIT_FAILURE;
}
