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

#include "../../pilight.h"
#include "common.h"
#include "irq.h"
#include "wiringPi.h"
#include "log.h"

#include "../hardwares/module.h"

#ifdef HARDWARE_433_GPIO
	#include "../hardwares/gpio.h"
#endif
#ifdef HARDWARE_433_MODULE
	#include "../hardwares/none.h"
#endif

#include "hardware.h"

void hardware_init(void) {
#ifdef HARDWARE_433_MODULE
	moduleInit();
#endif
#ifdef HARDWARE_433_GPIO	
	gpioInit();
#endif
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
		sfree((void *)&htmp->listener->id);
		sfree((void *)&htmp->listener);
		hardwares = hardwares->next;
		sfree((void *)&htmp);
	}
	sfree((void *)&hardwares);

	logprintf(LOG_DEBUG, "garbage collected hardware library");
	return EXIT_SUCCESS;
}

