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
#include "../libs/pilight/lua_c/table.h"
#include "../libs/pilight/config/config.h"
#include "../libs/pilight/config/hardware.h"

#include "alltests.h"

#include <wiringx.h>

void test_config_hardware(CuTest *tc) {
	if(suiteFailed()) return;

	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);

	memtrack();
	char *file = STRDUP(__FILE__);
	if(file == NULL) {
		OUT_OF_MEMORY
	}
	str_replace("config_hardware.c", "", &file);

	struct varcont_t out;

	plua_init();

	test_set_plua_path(tc, __FILE__, "config_hardware.c");

	config_init();

	char config[1024] = "{\"devices\":{},\"gui\":{},\"rules\":{},\
		\"settings\":{\"hardware-root\":\"%s../libs/pilight/hardware/\"},\"hardware\":{\
		\"433gpio\": {\
			\"sender\": 0,\
			\"receiver\": 1\
	}},\"registry\":{}}";

	FILE *f = fopen("storage_core.json", "w");
	fprintf(f, config, file);
	fclose(f);

	CuAssertIntEquals(tc, 0, config_read("storage_core.json", CONFIG_SETTINGS));

	hardware_init();

	CuAssertIntEquals(tc, 0, config_read("storage_core.json", CONFIG_HARDWARE));

	memset(&out, 0, sizeof(struct varcont_t));

	{
		struct plua_metatable_t *config = config_get_metatable();
		double sender = -1, receiver = -1;

		CuAssertIntEquals(tc, 0, plua_metatable_get_number(config, "hardware.433gpio.sender", &sender));
		CuAssertIntEquals(tc, 0, plua_metatable_get_number(config, "hardware.433gpio.receiver", &receiver));
		CuAssertIntEquals(tc, 0, sender);
		CuAssertIntEquals(tc, 1, receiver);
	}

	config_write();
	config_gc();
	hardware_gc();
	plua_gc();
	wiringXGC();

	FREE(file);

	CuAssertIntEquals(tc, 0, xfree());
}

