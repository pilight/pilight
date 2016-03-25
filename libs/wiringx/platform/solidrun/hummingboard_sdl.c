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
#include "hummingboard_sdl.h"			

struct platform_t *hummingboardSDL = NULL;

/*
|3v|5v|
| 0|5v|
| 1|0v|
| 2| 3|
|0v| 4|
| 5| 6|
| 7|0v|
| 8| 9|
|3v|10|
|11|0v|
|12|13|
|14|15|
|0v|16|

|-----|
|SPDIF|
|-----|
*/

/*
 * Not all GPIO where usable through sysfs
 * from the kernel used.
 */
static int irq[] = {
	 -1,  -1,   1,
	 -1,  -1,  73,
	 72,  71,  70,
	194, 195,  -1,
	 -1,  67,  -1,
	 -1,  -1
};

static int map[] = {
	/*	GPIO3_IO18,	GPIO3_IO17,	GPIO1_IO01 */
			 82,				 81,				  1,
	/*	GPIO5_IO28,	GPIO5_IO29,	GPIO3_IO09 */
			149,				150,				 73,
	/*	GPIO3_IO08,	GPIO3_IO07,	GPIO3_IO06 */
			 72,				 71,				 70,
	/*	GPIO7_IO02,	GPIO7_IO03,	GPIO2_IO24 */
			185,				186,				 56,
	/*	GPIO2_IO25,	GPIO3_IO03,	GPIO2_IO23 */
			 57,				 67,				 55,
	/*	GPIO2_IO26,	GPIO2_IO27 */
			 58,				 59
};

static int hummingboardSDLValidGPIO(int pin) {
	if(pin >= 0 && pin < (sizeof(map)/sizeof(map[0]))) {
		return 0;
	} else {
		return -1;
	}
}

static int hummingboardSDLISR(int i, enum isr_mode_t mode) {
	if(irq[i] == -1) {
		wiringXLog(LOG_ERR, "The %s gpio %d cannot be used as interrupt", hummingboardSDL->name, i);
		return -1;
	}
	return hummingboardSDL->soc->isr(i, mode);
}

void hummingboardSDLInit(void) {
	hummingboardSDL = malloc(sizeof(struct platform_t));
	strcpy(hummingboardSDL->name, "hummingboard_sdl");

	hummingboardSDL->soc = soc_get("NXP", "IMX6SDLRM");
	hummingboardSDL->soc->setMap(map);
	hummingboardSDL->soc->setIRQ(irq);

	hummingboardSDL->digitalRead = hummingboardSDL->soc->digitalRead;
	hummingboardSDL->digitalWrite = hummingboardSDL->soc->digitalWrite;
	hummingboardSDL->pinMode = hummingboardSDL->soc->pinMode;
	hummingboardSDL->setup = hummingboardSDL->soc->setup;

	hummingboardSDL->isr = &hummingboardSDLISR;
	hummingboardSDL->waitForInterrupt = hummingboardSDL->soc->waitForInterrupt;

	hummingboardSDL->selectableFd = hummingboardSDL->soc->selectableFd;
	hummingboardSDL->gc = hummingboardSDL->soc->gc;

	hummingboardSDL->validGPIO = &hummingboardSDLValidGPIO;

	platform_register(hummingboardSDL);
}
