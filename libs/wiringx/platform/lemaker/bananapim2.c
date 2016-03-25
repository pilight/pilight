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
#include "bananapim2.h"			

struct platform_t *bananapim2 = NULL;

/*
 PG10: Bananapi M2 LED D4 (green)
 PG11: Bananapi M2 LED D5 (blue)
 PH19: Bananapi M2 GPIO 2
 PH18: Bananapi M2 GPIO 3
 PH9:  Bananapi M2 GPIO 4
 PB0:  Bananapi M2 GPIO 5
 PB1:  Bananapi M2 GPIO 6
 PG12: Bananapi M2 GPIO 7
 PG13: Bananapi M2 GPIO 8
 PG16: Bananapi M2 GPIO 9
 PG15: Bananapi M2 GPIO 10
 PG14: Bananapi M2 GPIO 11
 PB7:  Bananapi M2 GPIO 12
 PB2:  Bananapi M2 GPIO 13
 PE4:  Bananapi M2 GPIO 14
 PE5:  Bananapi M2 GPIO 15
 PE6:  Bananapi M2 GPIO 16
 PG7:  Bananapi M2 GPIO 17
 PH10: Bananapi M2 GPIO 18
 PB3:  Bananapi M2 GPIO 19
 PE7:  Bananapi M2 GPIO 20
 PM2:  Bananapi M2 GPIO 21
 PG9:  Bananapi M2 GPIO 22
 PH11: Bananapi M2 GPIO 23
 PH12: Bananapi M2 GPIO 24
 PG8:  Bananapi M2 GPIO 24
 PB4:  Bananapi M2 GPIO 25
 PG6:  Bananapi M2 GPIO 26
*/

static int map[] = {
	/* 	PG10,	PG11,	PH19,	PH18 	*/										
			116,	117,	135,	134,
	/* 	PH9,	PB0,	PB1		PG12	*/
			125,	 28,	 29, 	118,
	/*	PG13,	PG16,	PG15,	PG14	*/
			119,	122,	121,	120,
	/*	PB7,	PB2,	PE4,	PE5		*/
			 35,	 30,	 88,	 89,
	/*	PE6		PG7,	PH10,	PB3		*/
			 90,	113,	126,	 31,
	/*	PE7,	PM2,	PG9,	PH11	*/
			91,	  155,	115,	127,
	/*	PH12,	PG8,	PB4,	PG6		*/
			128,	114,	 32,	112
};

// SYSFS GPIO 132 LED D3 (red)

static int bananapiM2ValidGPIO(int pin) {
	if(pin >= 0 && pin < (sizeof(map)/sizeof(map[0]))) {
		if(map[pin] == -1) {
			return -1;
		}
		return 0;
	} else {
		return -1;
	}
}

static int bananapiM2PinMode(int i, enum pinmode_t mode) {
	if(map[i] == -1) {
		return -1;
	}
	if((i == 0 || i == 1) && (mode != PINMODE_OUTPUT)) {
		wiringXLog(LOG_ERR, "The %s pin %d can only be used as output", bananapim2->name, i);
		return -1;
	}
	return bananapim2->soc->pinMode(i, mode);
}

static int bananapiM2DigitalWrite(int i, enum digital_value_t value) {
	if(map[i] == -1) {
		return -1;
	}	
	return bananapim2->soc->digitalWrite(i, value);	
}

void bananapiM2Init(void) {
	bananapim2 = malloc(sizeof(struct platform_t));
	strcpy(bananapim2->name, "bananapiM2");

	bananapim2->soc = soc_get("Allwinner", "A31s");
	bananapim2->soc->setMap(map);

	bananapim2->digitalRead = bananapim2->soc->digitalRead;
	bananapim2->digitalWrite = &bananapiM2DigitalWrite;
	bananapim2->pinMode = &bananapiM2PinMode;
	bananapim2->setup = bananapim2->soc->setup;

	bananapim2->isr = NULL;
	bananapim2->waitForInterrupt = NULL;

	bananapim2->selectableFd = bananapim2->soc->selectableFd;
	bananapim2->gc = bananapim2->soc->gc;

	bananapim2->validGPIO = &bananapiM2ValidGPIO;

	platform_register(bananapim2);
}
