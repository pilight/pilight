/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _REGISTRY_H_
#define _REGISTRY_H_

#include "../core/json.h"
#include "../config/config.h"
#include "../lua_c/lua.h"

int config_registry_get(lua_State *L, char *key, struct varcont_t *ret);
int config_registry_set_number(lua_State *L, char *key, double val);
int config_registry_set_string(lua_State *L, char *key, char *val);
int config_registry_set_boolean(lua_State *L, char *key, int val);
int config_registry_set_null(lua_State *L, char *key);

#endif
