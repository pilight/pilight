/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _LUA_DATETIME_H_
#define _LUA_DATETIME_H_

#include "lua.h"

extern int plua_datetime_strptime(struct lua_State *L);

static const luaL_Reg pilight_datetime_lib[] = {
	{"strptime", plua_datetime_strptime},
	{NULL, NULL}
};

#endif