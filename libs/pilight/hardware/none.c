/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../core/common.h"
#include "../core/dso.h"
#include "../hardware/hardware.h"
#include "none.h"

static int noneSend(int *code, int rawlen, int repeats) {
	sleep(1);
	return EXIT_SUCCESS;
}

static int noneReceive(void) {
	sleep(1);
	return EXIT_SUCCESS;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void noneInit(void) {
	hardware_register(&none);
	hardware_set_id(none, "none");
	none->hwtype=NONE;
	none->comtype=COMNONE;
	none->receiveOOK=&noneReceive;
	none->sendOOK=&noneSend;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "none";
	module->version = "2.0";
	module->reqversion = "8.0";
	module->reqcommit = NULL;
}

void init(void) {
	noneInit();
}
#endif
