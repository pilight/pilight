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
#include "gt.h"

static void operatorGtCallback(struct varcont_t *a, struct varcont_t *b, char **ret) {
	double ad = 0, bd = 0;
	struct varcont_t aa, *aaa = &aa;
	struct varcont_t bb, *bbb = &bb;

	memcpy(&aa, a, sizeof(struct varcont_t));
	memcpy(&bb, b, sizeof(struct varcont_t));

	if(a->type_ == JSON_BOOL) {
		cast2int(&aaa);
	}
	if(b->type_ == JSON_BOOL) {
		cast2int(&bbb);
	}

	if(aa.type_ == JSON_STRING && bb.type_ == JSON_STRING) {
		int al = strlen(aa.string_);
		int bl = strlen(bb.string_);
		int i = 0, y = (al < bl) ? al : bl;
		for(i=0;i<y;i++) {
			if(aa.string_[i] > bb.string_[i]) {
				strcpy(*ret, "1");
				return;
			}
		}
		if(al > bl) {
			strcpy(*ret, "1");
		} else {
			strcpy(*ret, "0");
		}
		return;
	}
	if(aa.type_ == JSON_STRING && bb.type_ == JSON_NUMBER) {
		bd = b->number_;
	}
	if(bb.type_ == JSON_STRING && aa.type_ == JSON_NUMBER) {
		ad = a->number_;
	}
	if(aa.type_ == JSON_NUMBER && bb.type_ == JSON_NUMBER) {
		ad = a->number_;
		bd = b->number_;
	}
	if(ad > bd) {
		strcpy(*ret, "1");
	} else {
		strcpy(*ret, "0");
	}
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void operatorGtInit(void) {
	event_operator_register(&operator_gt, ">");
	operator_gt->callback = &operatorGtCallback;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = ">";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = "87";
}

void init(void) {
	operatorGtInit();
}
#endif
