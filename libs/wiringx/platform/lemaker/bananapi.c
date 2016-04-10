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
#include "bananapi.h"			

struct platform_t *bananapi = NULL;

/*
3v|5v
 8|5v
 9|0v
 7|15
0v|16
 0| 1
 2|0v
 3| 4
3v| 5
12|0v
13| 6
14|10
0v|11

5v|5v
17|18
19|20
0v|0v
*/

static int map[] = {
	/*	PI19,	PH2,	PI18,	PI17	*/
			167,	126,	166,	165,
	/*	PH20,	PH21,	PI16,	PI3		*/
			144,	145,	164,	151,
	/*	PB21,	PB20,	PI10,	PI14	*/
			 39,	 38,	158,	162,
	/*	PI12,	PI13,	PI11,	PH0		*/
			160,	161,	159,	124,
	/*	PH1,	PH5,	PI21,	PH3		*/
			125,	129,	169,	127,
	/*	PI20	*/
			168
};

static int bananapiValidGPIO(int pin) {
	if(pin >= 0 && pin < (sizeof(map)/sizeof(map[0]))) {
		return 0;
	} else {
		return -1;
	}
}

static int bananapiSetup(void) {
	bananapi->soc->setup();
	bananapi->soc->setMap(map);
	return 0;
}

void bananapiInit(void) {
	platform_register(&bananapi, "bananapi");

	bananapi->soc = soc_get("Allwinner", "A10");

	bananapi->digitalRead = bananapi->soc->digitalRead;
	bananapi->digitalWrite = bananapi->soc->digitalWrite;
	bananapi->pinMode = bananapi->soc->pinMode;
	bananapi->setup = &bananapiSetup;

	bananapi->selectableFd = bananapi->soc->selectableFd;
	bananapi->gc = bananapi->soc->gc;

	bananapi->validGPIO = &bananapiValidGPIO;
}
