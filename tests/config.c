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
#include "../libs/pilight/config/settings.h"
#include "alltests.h"

void test_config_settings(CuTest *tc);
void test_config_registry(CuTest *tc);
void test_config_hardware(CuTest *tc);

void test_config_read(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();
	char *file = STRDUP(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}
	str_replace("config_registry.c", "", &file);

	fflush(stdout);

	plua_init();

	test_set_plua_path(tc, __FILE__, "config.c");

	config_init();

	char config[1024] = "{\"devices\":{\
		\"testlabel\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":1}],\"label\":\"\",\"color\":\"black\"},\
		\"testlabel\":{\"protocol\":[\"generic_label\"],\"id\":[{\"id\":2}],\"label\":\"\",\"color\":\"black\"}\
	},\
	\"rules\":{},\"gui\":{},\"settings\":{},\"hardware\":{},\"registry\":{}}";

	FILE *f = fopen("storage_core.json", "w");
	fprintf(f, config, "");
	fclose(f);

	CuAssertIntEquals(tc, 0, config_read("storage_core.json", CONFIG_SETTINGS | CONFIG_REGISTRY));
	config_write();
	config_gc();
	plua_gc();
	FREE(file);

	CuAssertIntEquals(tc, 0, xfree());
}

CuSuite *suite_config(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_config_read);
	SUITE_ADD_TEST(suite, test_config_settings);
	SUITE_ADD_TEST(suite, test_config_registry);
	SUITE_ADD_TEST(suite, test_config_hardware);

	return suite;
}


