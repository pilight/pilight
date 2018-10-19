/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _CONFIG_REGISTRY_H_
#define _CONFIG_REGISTRY_H_

#include "../lua_c/lua.h"

int config_registry_get(char *key, struct varcont_t *ret);
int config_registry_set_number(char *key, double val);
int config_registry_set_string(char *key, char *val);
int config_registry_set_boolean(char *key, int val);
int config_registry_set_null(char *key);

#endif