/*
	Copyright (c) 2016 CurlyMo <curlymoo1@gmail.com>

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <signal.h>   
	
#include "../../soc/soc.h"
#include "../../wiringX.h"	
#include "../platform.h"	
#include "hummingboard_gate_edge_sdl.h"			

struct platform_t *hummingboardGateEdgeSDL = NULL;

/*
-------
  MIC
-------
| 0| 1| 2|0v|
| 3| 4| 5| 6|
| 7| 8| 9|10|
|11|12|13|14|
|15|16|17|18|
|19|20|21|22|

|23|24|25|26|
|27|28|29|30|
|31|32|5v|3v|
|--|
|IR|
|--|

*/

/*
 * Not all GPIO where usable through sysfs
 * from the kernel used.
 */
static int irq[] = {
	204, -1, 54,
	 91, 90, 94,
	 95, -1, -1,
	 -1, -1, -1,
	 -1, -1, -1,
	 -1, -1, -1,
	 -1, 70, -1,
	 -1, -1, -1,
	 67, -1, -1,
	 70, 71, 72,
	 73, -1, -1
};

static int map[] = {
	/*	GPIO7_IO12,	GPIO7_IO11,	GPIO2_IO22 */
			195,				194,				 54,
	/*	GPIO3_IO27,	GPIO3_IO26,	GPIO3_IO30 */
			 91,				 90,				 94,
	/*	GPIO3_IO31,	GPIO5_IO04,	GPIO6_IO06 */
			 95,				125,				159,
	/*	GPIO2_IO16,	GPIO2_IO17,	GPIO2_IO18 */
			 48,				 47,				 50,
	/*	GPIO2_IO19,	GPIO2_IO20,	GPIO2_IO21 */
			 51,				 52,				 53,
	/*	GPIO2_IO28,	GPIO2_IO29,	GPIO3_IO00 */
			 60,				 61,				 64,
	/*	GPIO3_IO01,	GPIO3_IO12,	GPIO3_IO15 */
			 65,				 76,				 79,
	/*	GPIO3_IO14,	GPIO3_IO13,	GPIO3_IO02 */
			 78,				 77,				 66,
	/*	GPIO3_IO03,	GPIO3_IO04,	GPIO3_IO05 */
			 67,				 68,				 69,
	/*	GPIO3_IO06,	GPIO3_IO07,	GPIO3_IO08 */
			 70,				 71,				 72,
	/*	GPIO3_IO09,	GPIO3_IO11,	GPIO3_IO10 */
			 73,				 75,				 74
};

static int hummingboardGateEdgeSDLValidGPIO(int pin) {
	if(pin == 1) {
		return -1;
	}
	if(pin >= 0 && pin < (sizeof(map)/sizeof(map[0]))) {
		return 0;
	} else {
		return -1;
	}
}

static int hummingboardGateEdgeSDLISR(int i, enum isr_mode_t mode) {
	if(irq[i] == -1) {
		wiringXLog(LOG_ERR, "The %s gpio %d cannot be used as interrupt", hummingboardGateEdgeSDL->name, i);
		return -1;
	}
	return hummingboardGateEdgeSDL->soc->isr(i, mode);
}

void hummingboardGateEdgeSDLInit(void) {
	if((hummingboardGateEdgeSDL = malloc(sizeof(struct platform_t))) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	hummingboardGateEdgeSDL->nralias = 2;
	if((hummingboardGateEdgeSDL->name = malloc(hummingboardGateEdgeSDL->nralias*sizeof(char *))) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	if((hummingboardGateEdgeSDL->name[0] = strdup("hummingboard_edge_sdl")) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	if((hummingboardGateEdgeSDL->name[1] = strdup("hummingboard_gate_sdl")) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}

	hummingboardGateEdgeSDL->soc = soc_get("NXP", "IMX6SDLRM");
	hummingboardGateEdgeSDL->soc->setMap(map);
	hummingboardGateEdgeSDL->soc->setIRQ(irq);

	hummingboardGateEdgeSDL->digitalRead = hummingboardGateEdgeSDL->soc->digitalRead;
	hummingboardGateEdgeSDL->digitalWrite = hummingboardGateEdgeSDL->soc->digitalWrite;
	hummingboardGateEdgeSDL->pinMode = hummingboardGateEdgeSDL->soc->pinMode;
	hummingboardGateEdgeSDL->setup = hummingboardGateEdgeSDL->soc->setup;

	hummingboardGateEdgeSDL->isr = &hummingboardGateEdgeSDLISR;
	hummingboardGateEdgeSDL->waitForInterrupt = hummingboardGateEdgeSDL->soc->waitForInterrupt;

	hummingboardGateEdgeSDL->selectableFd = hummingboardGateEdgeSDL->soc->selectableFd;
	hummingboardGateEdgeSDL->gc = hummingboardGateEdgeSDL->soc->gc;

	hummingboardGateEdgeSDL->validGPIO = &hummingboardGateEdgeSDLValidGPIO;

	platform_register(hummingboardGateEdgeSDL);
}
