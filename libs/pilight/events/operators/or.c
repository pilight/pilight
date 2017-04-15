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
#include "../../core/cast.h"
#include "or.h"

static void operatorOrCallback(struct varcont_t *a, struct varcont_t *b, char **ret) {
	struct varcont_t aa, *aaa = &aa;
	struct varcont_t bb, *bbb = &bb;

	memcpy(&aa, a, sizeof(struct varcont_t));
	memcpy(&bb, b, sizeof(struct varcont_t));

	cast2bool(&aaa);
	cast2bool(&bbb);

	if(aa.bool_ == 1 || bb.bool_ == 1) {
		strcpy(*ret, "1");
	} else {
		strcpy(*ret, "0");
	}
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void operatorOrInit(void) {
	event_operator_register(&operator_or, "OR");
	operator_or->callback = &operatorOrCallback;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "OR";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = "87";
}

void init(void) {
	operatorOrInit();
}
#endif
