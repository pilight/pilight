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
#include "raspberrypi1b+.h"			

struct platform_t *raspberrypi1bp = NULL;

static int map[] = {
	/* 	FSEL17,	FSEL18,	FSEL27,	FSEL22 	*/
			17, 		18, 		27, 		22,
	/* 	FSEL23,	FSEL24,	FSEL25,	FSEL4 	*/
			23, 		24, 		25, 		4,
	/* 	FSEL2,	FSEL3,	FSEL8,	FSEL7 	*/
			2, 			3, 			8, 			7,
	/*	FSEL10,	FSEL9,	FSEL11,	FSEL14	*/
			10,			9,			11,			14,
	/*	FSEL15, FSEL28,	FSEL29,	FSEL30	*/
			15,			28,			29,			30,
	/*	FSEL31,	FSEL5,	FSEL6,	FSEL13	*/
			31,			5,			6,			13,
	/*	FSEL19,	FSEL26,	FSEL12,	FSEL16	*/
			19,			26,			12,			16,
	/*	FSEL20,	FSEL21,	FSEL0,	FSEL1		*/
			20,			21,			0,			1
};

static int raspberrypi1bpValidGPIO(int pin) {
	if(pin >= 0 && pin < (sizeof(map)/sizeof(map[0]))) {
		return 0;
	} else {
		return -1;
	}
}

void raspberrypi1bpInit(void) {
	platform_register(&raspberrypi1bp, "raspberrypi1b+");

	raspberrypi1bp->soc = soc_get("Broadcom", "2835");
	raspberrypi1bp->soc->setMap(map);

	raspberrypi1bp->digitalRead = raspberrypi1bp->soc->digitalRead;
	raspberrypi1bp->digitalWrite = raspberrypi1bp->soc->digitalWrite;
	raspberrypi1bp->pinMode = raspberrypi1bp->soc->pinMode;
	raspberrypi1bp->setup = raspberrypi1bp->soc->setup;

	raspberrypi1bp->isr = raspberrypi1bp->soc->isr;
	raspberrypi1bp->waitForInterrupt = raspberrypi1bp->soc->waitForInterrupt;

	raspberrypi1bp->selectableFd = raspberrypi1bp->soc->selectableFd;
	raspberrypi1bp->gc = raspberrypi1bp->soc->gc;

	raspberrypi1bp->validGPIO = &raspberrypi1bpValidGPIO;
}
