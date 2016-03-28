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
#include "pcduino1.h"			

struct platform_t *pcduino1 = NULL;

/*
 PI19: PCDuino GPIO 0
 PI18: PCDuino GPIO 1
 PH7:	 PCDuino GPIO 2
 PH6:	 PCDuino GPIO 3
 PH8:	 PCDuino GPIO 4
 PB2:  PCDuino GPIO 5
 PI3:  PCDuino GPIO 6
 PH9:  PCDuino GPIO 7 
 PH10: PCDuino GPIO 8
 PH5:  PCDuino GPIO 9
 PI10: PCDuino GPIO 10
 PI12: PCDuino GPIO 11
 PI13: PCDuino GPIO 12
 PI11: PCDuino GPIO 13
 PH11: PCDuino GPIO 14
 PH12: PCDuino GPIO 15
 PH13: PCDuino GPIO 16
 PH14: PCDuino GPIO 17
 PH15: PCDuino GPIO 18 (TX LED)
 PH16: PCDuino GPIO 19 (RX LED)
*/

static int map[] = {
	/* 	PI19,	PI18,	PH7,	PH6 	*/
			167, 	166, 	131, 	130,
	/*	PH8,	PB2, 	PI3, 	PH9 	*/
			132, 	20, 	151, 	133,
	/*	PH10,	PH5,	PI10,	PI12 	*/
			134, 	129, 	158, 	160,
	/*	PI13,	PI11,	PH11,	PH12 	*/
			161,	159,	135,	136,
	/*	PH13,	PH14,	PH15,	PH16	*/
			137,	138,	139,	140
};

static int pcduino1ValidGPIO(int pin) {
	if(pin >= 0 && pin < (sizeof(map)/sizeof(map[0]))) {
		return 0;
	} else {
		return -1;
	}
}

static int pcduino1PinMode(int i, enum pinmode_t mode) {
	if((i == 18 || i == 19) && (mode != PINMODE_OUTPUT)) {
		wiringXLog(LOG_ERR, "The %s pin %d can only be used as output", pcduino1->name, i);
		return -1;
	}
	return pcduino1->soc->pinMode(i, mode);
}

static int pcduino1DigitalWrite(int i, enum digital_value_t value) {
	if(i == 18 || i == 19) {
		if(value == HIGH) {
			value = LOW;
		} else if(value == LOW) {
			value = HIGH;
		}
	}
	return pcduino1->soc->digitalWrite(i, value);	
}

static int pcduino1Setup(void) {
	pcduino1->soc->setup();
	pcduino1->soc->setMap(map);
	return 0;
}

void pcduino1Init(void) {
	platform_register(&pcduino1, "pcduino1");

	pcduino1->soc = soc_get("Allwinner", "A10");

	pcduino1->digitalRead = pcduino1->soc->digitalRead;
	pcduino1->digitalWrite = &pcduino1DigitalWrite;
	pcduino1->pinMode = &pcduino1PinMode;
	pcduino1->setup = &pcduino1Setup;

	pcduino1->isr = pcduino1->soc->isr;
	pcduino1->waitForInterrupt = pcduino1->soc->waitForInterrupt;

	pcduino1->selectableFd = pcduino1->soc->selectableFd;
	pcduino1->gc = pcduino1->soc->gc;

	pcduino1->validGPIO = &pcduino1ValidGPIO;

}
