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
#include "hummingboard_base_pro_sdl.h"

struct platform_t *hummingboardBaseProSDL = NULL;

/*
|3v|5v|
| 8|5v|
| 9|0v|
| 7|15|
|0v|16|
| 0| 1|
| 2|0v|
| 3| 4|
|3v| 5|
|12|0v|
|13| 6|
|14|10|
|0v|11|

|-----|
|SPDIF|
|-----|
*/

/*
 * Not all GPIO where usable through sysfs
 * from the kernel used.
 */
static int irq[] = {
	 73,	 72,	 71,
	 7	0,	194,	195,
	 67,    1,	 -1
	 -1		 -1,	 -1,
	 -1,   -1,	 -1,
	 -1,	 -1
};

static int map[] = {
	/*	GPIO3_IO09,	GPIO3_IO08,	GPIO3_IO07	*/
			 73,				 72,				 71,
	/*	GPIO3_IO06,	GPIO7_IO02,	GPIO7_IO03	*/
			 70,				185,				186,
	/*	GPIO3_IO03,	GPIO1_IO01,	GPIO3_IO18	*/
			 67,				  1,			   82,
	/*	GPIO3_IO17,	GPIO2_IO26,	GPIO2_IO27	*/
			 81,				 58,				 59,
	/*	GPIO2_IO24,	GPIO2_IO25,	GPIO2_IO23	*/
			 56,				 57,				 55,
	/*	GPIO5_IO28,	GPIO5_IO29							*/
			149,				150
};

static int hummingboardBaseProSDLValidGPIO(int pin) {
	if(pin >= 0 && pin < (sizeof(map)/sizeof(map[0]))) {
		return 0;
	} else {
		return -1;
	}
}

static int hummingboardBaseProSDLISR(int i, enum isr_mode_t mode) {
	if(irq[i] == -1) {
		wiringXLog(LOG_ERR, "The %s gpio %d cannot be used as interrupt", hummingboardBaseProSDL->name[0], i);
		return -1;
	}
	return hummingboardBaseProSDL->soc->isr(i, mode);
}

static int hummingboardBaseProSDLSetup(void) {
	hummingboardBaseProSDL->soc->setup();
	hummingboardBaseProSDL->soc->setMap(map, sizeof(map) / sizeof(map[0]));
	hummingboardBaseProSDL->soc->setIRQ(irq, sizeof(irq) / sizeof(irq[0]));
	return 0;
}

void hummingboardBaseProSDLInit(void) {
	platform_register(&hummingboardBaseProSDL, "hummingboard_base_sdl");
	platform_add_alias(&hummingboardBaseProSDL, "hummingboard_pro_sdl");

	hummingboardBaseProSDL->soc = soc_get("NXP", "IMX6SDLRM");
	hummingboardBaseProSDL->soc->setMap(map, sizeof(map) / sizeof(map[0]));
	hummingboardBaseProSDL->soc->setIRQ(irq, sizeof(irq) / sizeof(irq[0]));

	hummingboardBaseProSDL->digitalRead = hummingboardBaseProSDL->soc->digitalRead;
	hummingboardBaseProSDL->digitalWrite = hummingboardBaseProSDL->soc->digitalWrite;
	hummingboardBaseProSDL->pinMode = hummingboardBaseProSDL->soc->pinMode;
	hummingboardBaseProSDL->setup = &hummingboardBaseProSDLSetup;

	hummingboardBaseProSDL->isr = &hummingboardBaseProSDLISR;
	hummingboardBaseProSDL->waitForInterrupt = hummingboardBaseProSDL->soc->waitForInterrupt;

	hummingboardBaseProSDL->selectableFd = hummingboardBaseProSDL->soc->selectableFd;
	hummingboardBaseProSDL->gc = hummingboardBaseProSDL->soc->gc;

	hummingboardBaseProSDL->validGPIO = &hummingboardBaseProSDLValidGPIO;

}
