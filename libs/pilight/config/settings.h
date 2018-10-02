/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _CONFIG_SETTINGS_H_
#define _CONFIG_SETTINGS_H_

#include "../lua_c/lua.h"

int config_setting_get_number(char *key, int idx, int *ret);
int config_setting_get_string(char *key, int idx, char **ret);
int config_setting_set_number(char *key, int idx, int val);
int config_setting_set_string(char *key, int idx, char *val);

#endif