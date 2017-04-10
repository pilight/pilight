/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifndef _WIN32
	#include <unistd.h>
#endif

#include "../../core/pilight.h"
#include "../operator.h"
#include "../../core/dso.h"
#include "ne.h"

static void operatorNeCallback(struct varcont_t *a, struct varcont_t *b, char **ret) {
	struct varcont_t aa, *aaa = &aa;
	struct varcont_t bb, *bbb = &bb;

	memcpy(&aa, a, sizeof(struct varcont_t));
	memcpy(&bb, b, sizeof(struct varcont_t));

	cast2str(&aaa);
	cast2str(&bbb);

	if(strcmp(aa.string_, bb.string_) != 0) {
		strcpy(*ret, "1");
	} else {
		strcpy(*ret, "0");
	}

	FREE(aa.string_);
	FREE(bb.string_);
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void operatorNeInit(void) {
	event_operator_register(&operator_ne, "!=");
	operator_ne->callback = &operatorNeCallback;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "!=";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = "87";
}

void init(void) {
	operatorNeInit();
}
#endif
