/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _LUA_ASYNC_EVENT_H_
#define _LUA_ASYNC_EVENT_H_

typedef struct lua_event_t {
	PLUA_INTERFACE_FIELDS

	struct {
		int active;
		struct eventpool_listener_t *node;
	} reasons[REASON_END+10000];
	char *callback;
} lua_event_t;

int plua_async_event(struct lua_State *L);

#endif