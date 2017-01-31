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

#include "../operator.h"
#include "../../core/dso.h"
#include "divide.h"

static void operatorDivideCallback(double a, double b, char **ret) {
	if(a == 0 || b == 0) {
		strcpy(*ret, "0.000000");
	} else {
		sprintf(*ret, "%.6f", (a / b));
	}
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void operatorDivideInit(void) {
	event_operator_register(&operator_divide, "/");
	operator_divide->callback_number = &operatorDivideCallback;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "/";
	module->version = "1.1";
	module->reqversion = "5.0";
	module->reqcommit = "87";
}

void init(void) {
	operatorDivideInit();
}
#endif
