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

#include "irq.h"
#include "wiringPi.h"
#include "log.h"

#include "protocol.h"
#include "../protocols/arctech_switch.h"
#include "../protocols/arctech_dimmer.h"
#include "../protocols/arctech_old.h"
#include "../protocols/sartano.h"
#include "../protocols/impuls.h"
#include "../protocols/alecto.h"
#include "../protocols/raw.h"
#include "../protocols/relay.h"
#include "../protocols/generic_weather.h"
#include "../protocols/generic_switch.h"
#include "../protocols/generic_dimmer.h"

#include "../hardwares/module.h"
#include "../hardwares/gpio.h"
#include "../hardwares/none.h"

#include "hardware.h"

/* The frequency on which the lirc module needs to send 433.92Mhz*/
int freq = FREQ433; // Khz
/* Is the module initialized */
int initialized = 0;
/* Is the right frequency set */
int setfreq = 0;

void hardware_init(void) {
	arctechSwInit();
	arctechDimInit();
	arctechOldInit();
	sartanoInit();
	impulsInit();
	relayInit();
	rawInit();
	alectoInit();
	genWeatherInit();
	genSwitchInit();
	genDimInit();

	moduleInit();
	gpioInit();
	noneInit();
}

void hardware_register(hardware_t **hw) {
	*hw = malloc(sizeof(struct hardware_t));

	(*hw)->init = NULL;
	(*hw)->deinit = NULL;
	(*hw)->receive = NULL;
	(*hw)->send = NULL;
	
	struct hardwares_t *hnode = malloc(sizeof(struct hardwares_t));
	hnode->listener = *hw;
	hnode->next = hardwares;
	hardwares = hnode;
}

int hardware_gc(void) {
	struct hardwares_t *htmp;

	while(hardwares) {
		htmp = hardwares;
		free(htmp->listener->id);
		free(htmp->listener);
		hardwares = hardwares->next;
		free(htmp);
	}
	free(hardwares);

	logprintf(LOG_DEBUG, "garbage collected hardware library");
	return EXIT_SUCCESS;
}

