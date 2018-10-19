/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "../core/json.h"

struct lua_state_t *plua_get_module(char *namespace, char *module);
int config_callback_func(char *module, char *func, char *settings);
char *config_callback_write(char *);
int config_root(char *path);
void plua_pause_coverage(int status);
void config_init(void);
int config_read(char *file, unsigned short objects);
char *config_write(void);
struct plua_metatable_t *config_get_metatable(void);
int config_exists(char *module);
int config_gc(void);

#endif
