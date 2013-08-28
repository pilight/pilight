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
#include "settings.h"

#include "../lirc/lirc.h"
#include "../lirc/lircd.h"
#include "../lirc/hardware.h"
#include "../lirc/transmit.h"
#include "../lirc/hw-types.h"
#include "../lirc/hw_default.h"

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
	//genWeatherInit();
}

/* Initialize the hardware module lirc_rpi */
int module_init(void) {
	int use_lirc = USE_LIRC;
	int gpio_in = GPIO_IN_PIN;
	int gpio_out = GPIO_OUT_PIN;

	settings_find_number("use-lirc", &use_lirc);
	settings_find_number("gpio-receiver", &gpio_in);
	settings_find_number("gpio-sender", &gpio_out);
	
	if(use_lirc == 1) {
		if(initialized == 0) {
			if(!hw.init_func()) {
				exit(EXIT_FAILURE);
			}

			/* Only set the frequency once */
			if(setfreq == 0) {
				freq = FREQ433;
				/* Set the lirc_rpi frequency to 433.92Mhz */
				if(hw.ioctl_func(LIRC_SET_SEND_CARRIER, &freq) == -1) {
					logprintf(LOG_ERR, "could not set lirc_rpi send frequency");
					exit(EXIT_FAILURE);
				}
				setfreq = 1;
			}
			logprintf(LOG_DEBUG, "initialized lirc_rpi module");
			initialized = 1;
		
		}
	} else {
		if(wiringPiSetup() == -1)
			return EXIT_FAILURE;

		pinMode(gpio_out, OUTPUT);
		if(wiringPiISR(gpio_in, INT_EDGE_BOTH) < 0) {
			logprintf(LOG_ERR, "unable to register interrupt for pin %d", gpio_in) ;
			return 1 ;
		}
	}
	return EXIT_SUCCESS;
}

/* Release the hardware module lirc_rpi */
int module_deinit(void) {
	int use_lirc = 0;
	settings_find_number("use-lirc", &use_lirc);
	if(use_lirc == 1) {
		if(initialized == 1) {
			freq = FREQ38;
			if(hw.ioctl_func(LIRC_SET_SEND_CARRIER, &freq) == -1) {
				logprintf(LOG_ERR, "could not restore default freq of the lirc_rpi module");
				exit(EXIT_FAILURE);
			} else {
				logprintf(LOG_DEBUG, "default freq of the lirc_rpi module set");
			}
			/* Restore the lirc_rpi frequency to its default value */
			hw.deinit_func();

			logprintf(LOG_DEBUG, "deinitialized lirc_rpi module");
			initialized	= 0;
		}
	}
	return EXIT_SUCCESS;
}
