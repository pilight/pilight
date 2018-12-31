/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _LUA_CAST_H_
#define _LUA_CAST_H_

#include "lua.h"

extern int plua_cast_toboolean(struct lua_State *L);
extern int plua_cast_tonumber(struct lua_State *L);
extern int plua_cast_tostring(struct lua_State *L);

static const luaL_Reg pilight_cast_lib[] = {
	{"toboolean", plua_cast_toboolean},
	{"tonumber", plua_cast_tonumber},
	{"tostring", plua_cast_tostring},
	{NULL, NULL}
};

#endif