/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _LUA_ASYNC_H_
#define _LUA_ASYNC_H_

#include "lua.h"

extern int plua_async_thread(struct lua_State *L);
extern int plua_async_timer(struct lua_State *L);
extern int plua_async_event(struct lua_State *L);

static const luaL_Reg pilight_async_lib[] = {
	{"thread", plua_async_thread},
	{"timer", plua_async_timer},
	{"event", plua_async_event},
	{NULL, NULL}
};

#endif