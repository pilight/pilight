/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _LUA_ASYNC_THREAD_H_
#define _LUA_ASYNC_THREAD_H_

typedef struct lua_thread_t {
	PLUA_INTERFACE_FIELDS

	char *callback;
	int running;
	uv_work_t *work_req;
} lua_thread_t;

int plua_async_thread(struct lua_State *L);

#endif