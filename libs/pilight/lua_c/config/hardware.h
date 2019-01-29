/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _LUA_CONFIG_HARDWARE_H_
#define _LUA_CONFIG_HARDWARE_H_

#include "../lua.h"

typedef struct plua_hardware_t {
	char *name;
	struct plua_metatable_t *table;
	struct hardware_t *ptr;
} plua_hardware_t;

int plua_config_hardware(lua_State *L);

#endif