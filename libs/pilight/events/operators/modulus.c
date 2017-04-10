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
#include <math.h>
#ifndef _WIN32
	#include <unistd.h>
#endif

#include "../operator.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "modulus.h"

static void operatorModulusCallback(struct varcont_t *a, struct varcont_t *b, char **ret) {
	struct varcont_t aa, *aaa = &aa;
	struct varcont_t bb, *bbb = &bb;

	memcpy(&aa, a, sizeof(struct varcont_t));
	memcpy(&bb, b, sizeof(struct varcont_t));

	cast2int(&aaa);
	cast2int(&bbb);

	if(aaa->number_ == 0 || bbb->number_ == 0) {
		strcpy(*ret, "0");
	} else {
		sprintf(*ret, "%.6f", aaa->number_ - bbb->number_ * floor(aaa->number_ / bbb->number_));
	}
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void operatorModulusInit(void) {
	event_operator_register(&operator_modulus, "%");
	operator_modulus->callback = &operatorModulusCallback;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "%";
	module->version = "1.1";
	module->reqversion = "5.0";
	module->reqcommit = "87";
}

void init(void) {
	operatorModulusInit();
}
#endif
