/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/binary.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/config/config.h"
#include "../libs/pilight/config/registry.h"

#include "alltests.h"

#include <wiringx.h>

struct registry_t {
	char *key;
	union {
		double number_;
		char *string_;
		int bool_;
	} val;
	int type_;
} registry[] = {
	{ "a", { .string_ = "b" }, JSON_STRING },
	{ "a", { .number_ = 1 }, JSON_NUMBER },
	{ "a", { .bool_ = 1 }, JSON_BOOL },
	{ "a.b", { .string_ = "c" }, JSON_STRING },
	{ "a.b.c.d", { .string_ = "d.e" }, JSON_STRING },
	{ "a.b.c.e", { .string_ = "1.2" }, JSON_STRING },
	{ "a.b.c", { .number_ = 1.2 }, JSON_NUMBER },
};

void test_config_registry(CuTest *tc) {
	memtrack();
	char *file = STRDUP(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}
	str_replace("config_registry.c", "", &file);

	struct varcont_t out;
	int len = sizeof(registry)/sizeof(registry[0]);
	int i = 0;
	for(i=0;i<len;i++) {
		printf("[ %-10s %-3d %-26s ]\n", __FUNCTION__, i, registry[i].key);
		fflush(stdout);

		plua_init();

		test_set_plua_path(tc, __FILE__, "config_registry.c");

		config_init();

		char config[1024] = "{\"devices\":{},\"gui\":{},\"rules\":{},\"settings\":{},\"hardware\":{},\"registry\":{}}";

		FILE *f = fopen("storage_core.json", "w");
		fprintf(f, config, "");
		fclose(f);

		memset(&out, 0, sizeof(struct varcont_t));
		CuAssertIntEquals(tc, 0, config_read("storage_core.json", CONFIG_REGISTRY));
		if(registry[i].type_ == JSON_STRING) {
			CuAssertIntEquals(tc, 0, config_registry_set_string(registry[i].key, registry[i].val.string_));
			CuAssertIntEquals(tc, 0, config_registry_get(registry[i].key, &out));
			CuAssertIntEquals(tc, LUA_TSTRING, out.type_);
			CuAssertStrEquals(tc, registry[i].val.string_, out.string_);
			FREE(out.string_);
		} else if(registry[i].type_ == JSON_NUMBER) {
			CuAssertIntEquals(tc, 0, config_registry_set_number(registry[i].key, registry[i].val.number_));
			CuAssertIntEquals(tc, 0, config_registry_get(registry[i].key, &out));
			CuAssertIntEquals(tc, LUA_TNUMBER, out.type_);
			CuAssertDblEquals(tc, registry[i].val.number_, out.number_, EPSILON);
		} else if(registry[i].type_ == JSON_BOOL) {
			CuAssertIntEquals(tc, 0, config_registry_set_boolean(registry[i].key, registry[i].val.bool_));
			CuAssertIntEquals(tc, 0, config_registry_get(registry[i].key, &out));
			CuAssertIntEquals(tc, LUA_TBOOLEAN, out.type_);
			CuAssertIntEquals(tc, registry[i].val.bool_, out.bool_);
		}

		config_write();
		config_gc();
		plua_gc();
		wiringXGC();
	}
	FREE(file);

	CuAssertIntEquals(tc, 0, xfree());
}

