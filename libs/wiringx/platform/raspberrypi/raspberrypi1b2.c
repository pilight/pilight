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
#include "raspberrypi1b2.h"

struct platform_t *raspberrypi1b2 = NULL;

static int map[] = {
	/* 	FSEL17,	FSEL18,	FSEL27,	FSEL22 	*/
			17, 		18, 		27, 		22,
	/* 	FSEL23,	FSEL24,	FSEL25,	FSEL4 	*/
			23, 		24, 		25, 		 4,
	/* 	FSEL2,	FSEL3,	FSEL8,	FSEL7 	*/
			 2, 		 3, 		 8, 		 7,
	/*	FSEL10,	FSEL9,	FSEL11,	FSEL14	*/
			10,			 9,			11,			14,
	/*	FSEL15, FSEL28,	FSEL29,	FSEL30	*/
			15,			28,			29,			30,
	/*	FSEL31		*/
			31
};

static int raspberrypi1b2ValidGPIO(int pin) {
	if(pin >= 0 && pin < (sizeof(map)/sizeof(map[0]))) {
		return 0;
	} else {
		return -1;
	}
}

static int raspberrypi1b2Setup(void) {
	const size_t size = sizeof(map) / sizeof(map[0]);
	raspberrypi1b2->soc->setup();
	raspberrypi1b2->soc->setMap(map, size);
	raspberrypi1b2->soc->setIRQ(map, size);
	return 0;
}

void raspberrypi1b2Init(void) {
	platform_register(&raspberrypi1b2, "raspberrypi1b2");

	raspberrypi1b2->soc = soc_get("Broadcom", "2835");
	raspberrypi1b2->soc->setMap(map, sizeof(map) / sizeof(map[0]));

	raspberrypi1b2->digitalRead = raspberrypi1b2->soc->digitalRead;
	raspberrypi1b2->digitalWrite = raspberrypi1b2->soc->digitalWrite;
	raspberrypi1b2->pinMode = raspberrypi1b2->soc->pinMode;
	raspberrypi1b2->setup = raspberrypi1b2Setup;

	raspberrypi1b2->isr = raspberrypi1b2->soc->isr;
	raspberrypi1b2->waitForInterrupt = raspberrypi1b2->soc->waitForInterrupt;

	raspberrypi1b2->selectableFd = raspberrypi1b2->soc->selectableFd;
	raspberrypi1b2->gc = raspberrypi1b2->soc->gc;

	raspberrypi1b2->validGPIO = &raspberrypi1b2ValidGPIO;
}
