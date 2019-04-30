/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "../core/json.h"
#include "../lua_c/lua.h"

#define CONFIG_INTERNAL	0
#define CONFIG_FORWARD	1
#define CONFIG_USER			2

typedef enum config_objects_t {
	CONFIG_DEVICES = 0x1,
	CONFIG_RULES = 0x2,
	CONFIG_GUI = 0x4,
	CONFIG_SETTINGS = 0x8,
	CONFIG_HARDWARE = 0x10,
	CONFIG_REGISTRY = 0x20,
	CONFIG_ALL = 0x3F
} config_objects_t;

struct lua_state_t *plua_get_module(lua_State *L, char *namespace, char *module);
int config_callback_func(char *module, char *func, char *settings);
int config_root(char *path);
void plua_pause_coverage(int status);
void config_init(void);
struct JsonNode *config_print(int level, const char *media);
int config_parse(struct JsonNode *root, unsigned short objects);
int config_read(lua_State *L, unsigned short objects);
int config_write(int level, char *media);
struct plua_metatable_t *config_get_metatable(void);
int config_exists(char *module);
int config_set_file(char *settfile);
char *config_get_file(void);
int config_gc(void);

#endif
