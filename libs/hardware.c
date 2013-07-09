/*
	Copyright (C) 2013 CurlyMo

	This file is part of the Raspberry Pi 433.92Mhz transceiver.

    Raspberry Pi 433.92Mhz transceiver is free software: you can redistribute
	it and/or modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation, either version 3 of the License,
	or (at your option) any later version.

    Raspberry Pi 433.92Mhz transceiver is distributed in the hope that it will
	be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Raspberry Pi 433.92Mhz transceiver. If not, see
	<http://www.gnu.org/licenses/>
*/

#include "../lirc/lirc.h"
#include "../lirc/lircd.h"
#include "../lirc/hardware.h"
#include "../lirc/transmit.h"
#include "../lirc/hw-types.h"
#include "../lirc/hw_default.h"

#include "protocol.h"
#include "protocols/arctech_switch.h"
#include "protocols/arctech_dimmer.h"
#include "protocols/arctech_old.h"
#include "protocols/sartano.h"
#include "protocols/alecto.h"
#include "protocols/raw.h"

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
	rawInit();
	alectoInit();
}

/* Initialize the hardware module lirc_rpi */
int module_init(void) {
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
	return EXIT_SUCCESS;
}

/* Release the hardware module lirc_rpi */
int module_deinit(void) {
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
	return EXIT_SUCCESS;
}