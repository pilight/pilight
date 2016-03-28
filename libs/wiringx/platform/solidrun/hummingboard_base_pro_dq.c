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
#include "hummingboard_base_pro_dq.h"			

struct platform_t *hummingboardBaseProDQ = NULL;

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
	 73,	 72,	 71,
	 70,	194,	195,
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

static int hummingboardBaseProDQValidGPIO(int pin) {
	if(pin >= 0 && pin < (sizeof(map)/sizeof(map[0]))) {
		return 0;
	} else {
		return -1;
	}
}

static int hummingboardBaseProDQISR(int i, enum isr_mode_t mode) {
	if(irq[i] == -1) {
		wiringXLog(LOG_ERR, "The %s gpio %d cannot be used as interrupt", hummingboardBaseProDQ->name, i);
		return -1;
	}
	return hummingboardBaseProDQ->soc->isr(i, mode);
}

static int hummingboardBaseProDQSetup(void) {
	hummingboardBaseProDQ->soc->setup();
	hummingboardBaseProDQ->soc->setMap(map);
	hummingboardBaseProDQ->soc->setIRQ(irq);
	return 0;
}

void hummingboardBaseProDQInit(void) {
	platform_register(&hummingboardBaseProDQ, "hummingboard_base_dq");
	platform_add_alias(&hummingboardBaseProDQ, "hummingboard_pro_dq");

	hummingboardBaseProDQ->soc = soc_get("NXP", "IMX6DQRM");

	hummingboardBaseProDQ->digitalRead = hummingboardBaseProDQ->soc->digitalRead;
	hummingboardBaseProDQ->digitalWrite = hummingboardBaseProDQ->soc->digitalWrite;
	hummingboardBaseProDQ->pinMode = hummingboardBaseProDQ->soc->pinMode;
	hummingboardBaseProDQ->setup = &hummingboardBaseProDQSetup;

	hummingboardBaseProDQ->isr = &hummingboardBaseProDQISR;
	hummingboardBaseProDQ->waitForInterrupt = hummingboardBaseProDQ->soc->waitForInterrupt;

	hummingboardBaseProDQ->selectableFd = hummingboardBaseProDQ->soc->selectableFd;
	hummingboardBaseProDQ->gc = hummingboardBaseProDQ->soc->gc;

	hummingboardBaseProDQ->validGPIO = &hummingboardBaseProDQValidGPIO;
}
