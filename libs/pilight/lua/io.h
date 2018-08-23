/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _LUA_IO_H_
#define _LUA_IO_H_

#include "lua.h"

#include "io/file.h"

extern int plua_io_file(struct lua_State *L);

static const luaL_Reg pilight_io_lib[] = {
	{"file", plua_io_file},
	{NULL, NULL}
};

#endif