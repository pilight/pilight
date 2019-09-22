/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _LUA_ASYNC_TIMER_H_
#define _LUA_ASYNC_TIMER_H_

typedef struct lua_timer_t {
	PLUA_INTERFACE_FIELDS

	uv_timer_t *timer_req;
	int running;
	int timeout;
	int repeat;
	char *callback;
} lua_timer_t;

int plua_async_timer(struct lua_State *L);

#endif