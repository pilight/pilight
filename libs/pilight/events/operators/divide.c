/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
	#include <unistd.h>
#endif

#include "../operator.h"
#include "../../core/dso.h"
#include "divide.h"

static void operatorDivideCallback(struct varcont_t *a, struct varcont_t *b, char **ret) {
	struct varcont_t aa, *aaa = &aa;
	struct varcont_t bb, *bbb = &bb;

	memcpy(&aa, a, sizeof(struct varcont_t));
	memcpy(&bb, b, sizeof(struct varcont_t));

	cast2int(&aaa);
	cast2int(&bbb);

	if(aaa->number_ == 0 || bbb->number_ == 0) {
		strcpy(*ret, "0.000000");
	} else {
		sprintf(*ret, "%.6f", (aaa->number_ / bbb->number_));
	}
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void operatorDivideInit(void) {
	event_operator_register(&operator_divide, "/");
	operator_divide->callback = &operatorDivideCallback;
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
