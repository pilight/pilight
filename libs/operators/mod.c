/*
	Copyright (C) 2015 CurlyMo & Niek

	This file is part of pilight.

	pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

	pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "operator.h"
#include "mod.h"
#include "dso.h"
#include "log.h"
#include "math.h"

static void operatorModCallback(double a, double b, char **ret) {
	//Do NOT USE C++ "%" operator here, because it gives bad results with negative operands
	sprintf(*ret, "%f",  a - b * floor(a / b));
	logprintf(LOG_DEBUG, "mod a:%f", a);
	logprintf(LOG_DEBUG, "mod b:%f", b);
	logprintf(LOG_DEBUG, "mod result:%f",  a - b * floor(a / b));
        logprintf(LOG_DEBUG, "mod result (culyMo):%f", (double)((int)a % (int)b));
 
}

#ifndef MODULE
__attribute__((weak))
#endif
void operatorModInit(void) {
	event_operator_register(&operator_mod, "%");
	operator_mod->callback_number = &operatorModCallback;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "%";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	operatorModInit();
}
#endif
