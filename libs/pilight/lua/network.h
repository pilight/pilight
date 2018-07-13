/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _LUA_NETWORK_H_
#define _LUA_NETWORK_H_

#include "lua.h"
#include "network/mail.h"

extern int plua_network_mail(struct lua_State *L);
extern int plua_network_http(struct lua_State *L);

static const luaL_Reg pilight_network_lib[] = {
	{"mail", plua_network_mail},
	{"http", plua_network_http},
	{NULL, NULL}
};

#endif