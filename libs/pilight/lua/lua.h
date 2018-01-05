/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _LUA_H_
#define _LUA_H_

#include <lua.h>

#define OPERATOR 1
#define FUNCTION 2

typedef struct plua_module_t {
	char name[255];
	char file[PATH_MAX];
	char version[12];
	char reqversion[12];
	char reqcommit[12];
	int type;

	struct plua_module_t *next;
} plua_module_t;

void plua_module_load(char *, int);
int plua_module_exists(char *, int);
struct lua_State *plua_get_state(void);
struct plua_module_t *plua_get_modules(void);
void plua_init(void);
int plua_gc(void);

#endif