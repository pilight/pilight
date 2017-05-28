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
#include "raspberrypizero.h"

struct platform_t *raspberrypizero = NULL;

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

static int raspberrypizeroValidGPIO(int pin) {
	if(pin >= 0 && pin < (sizeof(map)/sizeof(map[0]))) {
		return 0;
	} else {
		return -1;
	}
}

static int raspberrypizeroSetup(void) {
	const size_t size = sizeof(map) / sizeof(map[0]);
	raspberrypizero->soc->setup();
	raspberrypizero->soc->setMap(map, size);
	raspberrypizero->soc->setIRQ(map, size);
	return 0;
}

void raspberrypizeroInit(void) {
	platform_register(&raspberrypizero, "raspberrypizero");

	raspberrypizero->soc = soc_get("Broadcom", "2835");
	raspberrypizero->soc->setMap(map, sizeof(map) / sizeof(map[0]));

	raspberrypizero->digitalRead = raspberrypizero->soc->digitalRead;
	raspberrypizero->digitalWrite = raspberrypizero->soc->digitalWrite;
	raspberrypizero->pinMode = raspberrypizero->soc->pinMode;
	raspberrypizero->setup = raspberrypizeroSetup;

	raspberrypizero->isr = raspberrypizero->soc->isr;
	raspberrypizero->waitForInterrupt = raspberrypizero->soc->waitForInterrupt;

	raspberrypizero->selectableFd = raspberrypizero->soc->selectableFd;
	raspberrypizero->gc = raspberrypizero->soc->gc;

	raspberrypizero->validGPIO = &raspberrypizeroValidGPIO;
}
