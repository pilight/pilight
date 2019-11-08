/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <limits.h>
#include <assert.h>

#ifndef _WIN32
	#include <unistd.h>
	#include <sys/time.h>
	#include <libgen.h>
	#include <dirent.h>
	#include <unistd.h>
#endif

#include "../../core/log.h"
#include "../async.h"
#include "../table.h"

static void *plua_async_event_free(void *param) {
	struct plua_metatable_t *data = param;

	plua_metatable_free(data);

	return NULL;
}

static void plua_async_event_object(lua_State *L, struct lua_event_t *event);

#ifdef PILIGHT_UNITTEST
extern void plua_async_event_gc(void *ptr);
#else
static void plua_async_event_gc(void *ptr) {
	struct lua_event_t *lua_event = ptr;

	if(lua_event != NULL) {
		int x = 0;
		if((x = atomic_dec(lua_event->ref)) == 0) {
			plua_metatable_free(lua_event->table);
			if(lua_event->callback != NULL) {
				FREE(lua_event->callback);
			}
			if(lua_event->sigterm == 1) {
				lua_event->gc = NULL;
			}
			plua_gc_unreg(NULL, lua_event);
			FREE(lua_event);
		}
		assert(x >= 0);
	}
}
#endif

#ifdef PILIGHT_UNITTEST
extern void plua_async_event_global_gc(void *ptr);
#else
static void plua_async_event_global_gc(void *ptr) {
	struct lua_event_t *lua_event = ptr;
	lua_event->sigterm = 1;

	plua_async_event_gc(ptr);
}
#endif

void *plua_async_event_callback(int reason, void *param, void *userdata) {
	struct lua_event_t *event = userdata;
	char name[512], *p = name;
	memset(name, '\0', 512);

	if(event->callback == NULL){
		return NULL;
	}

	atomic_inc(event->ref);

	/*
	 * Only create a new state once the timer is triggered
	 */
	struct lua_state_t *state = plua_get_free_state();
	state->module = event->module;

	logprintf(LOG_DEBUG, "lua async on state #%d", state->idx);

	p = name;
	plua_namespace(state->module, p);

	lua_getglobal(state->L, name);
	if(lua_type(state->L, -1) == LUA_TNIL) {
		pluaL_error(state->L, "cannot find %s lua module", name);
	}

	lua_getfield(state->L, -1, event->callback);
	if(lua_type(state->L, -1) != LUA_TFUNCTION) {
		pluaL_error(state->L, "%s: async callback %s does not exist", state->module->file, event->callback);
	}

	plua_async_event_object(state->L, event);
	lua_pushnumber(state->L, reason);
	push_plua_metatable(state->L, param);

	plua_gc_reg(state->L, event, plua_async_event_gc);

	assert(plua_check_stack(state->L, 5, PLUA_TTABLE, PLUA_TFUNCTION, PLUA_TTABLE, PLUA_TNUMBER, PLUA_TTABLE) == 0);
	if(plua_pcall(state->L, state->module->file, 3, 0) == -1) {
		assert(plua_check_stack(state->L, 0) == 0);

		plua_clear_state(state);
		return NULL;
	}
	lua_remove(state->L, 1);
	assert(plua_check_stack(state->L, 0) == 0);
	plua_clear_state(state);
	return NULL;
}

static int plua_async_event_register(struct lua_State *L) {
	struct lua_event_t *event = (void *)lua_topointer(L, lua_upvalueindex(1));
	int reason = -1;

	if(lua_gettop(L) != 1) {
		pluaL_error(L, "event.register requires 1 arguments, %d given", lua_gettop(L));
	}

	if(event == NULL) {
		pluaL_error(L, "internal error: event object not passed");
	}

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "number expected, got %s";

		sprintf(p, error, lua_typename(L, lua_type(L, -1)));

		luaL_argcheck(L,
			(lua_type(L, -1) == LUA_TNUMBER),
			1, buf);

		if(lua_type(L, -1) == LUA_TNUMBER) {
			reason = lua_tonumber(L, -1);
			lua_remove(L, -1);
		}
	}

	if((reason < 0) ||
		 ((reason >= REASON_END) && (reason < 10000)) ||
		 ((reason >= REASON_END+10000))) {
		pluaL_error(L, "event reason %d is not a valid reason", reason);
	}

	int is_active = 0, i = 0;
	for(i=0;i<REASON_END+10000;i++) {
		if(event->reasons[i].active == 1) {
			is_active = 1;
			break;
		}
	}

	if(is_active == 0) {
		atomic_inc(event->ref);

		if(event->callback == NULL) {
			plua_gc_reg(L, event, plua_async_event_gc);
		}
	}

	event->reasons[reason].active = 1;

	if(event->callback != NULL) {
		event->reasons[reason].node = eventpool_callback(reason, plua_async_event_callback, event);
	}

	lua_pushboolean(L, 1);

	assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

	return 1;
}

static int plua_async_event_unregister(struct lua_State *L) {
	struct lua_event_t *event = (void *)lua_topointer(L, lua_upvalueindex(1));
	int reason = -1;

	if(lua_gettop(L) != 1) {
		pluaL_error(L, "event.unregister requires 1 arguments, %d given", lua_gettop(L));
	}

	if(event == NULL) {
		pluaL_error(L, "internal error: event object not passed");
	}

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "number expected, got %s";

		sprintf(p, error, lua_typename(L, lua_type(L, -1)));

		luaL_argcheck(L,
			(lua_type(L, -1) == LUA_TNUMBER),
			1, buf);

		if(lua_type(L, -1) == LUA_TNUMBER) {
			reason = lua_tonumber(L, -1);
			lua_remove(L, -1);
		}
	}

	if((reason < 0) ||
		 ((reason >= REASON_END) && (reason < 10000)) ||
		 ((reason >= REASON_END+10000))) {
		pluaL_error(L, "event reason %d is not a valid reason", reason);
	}

	event->reasons[reason].active = 0;

	int is_active = 0, i = 0;
	for(i=0;i<REASON_END+10000;i++) {
		if(event->reasons[i].active == 1) {
			is_active = 1;
			break;
		}
	}

	if(is_active == 0) {
		plua_gc_reg(L, event, plua_async_event_gc);
	}

	if(event->reasons[reason].node != NULL) {
		eventpool_callback_remove(event->reasons[reason].node);
	}
	event->reasons[reason].node = NULL;

	lua_pushboolean(L, 1);

	assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

	return 1;
}

static int plua_async_event_trigger(struct lua_State *L) {
	struct lua_event_t *event = (void *)lua_topointer(L, lua_upvalueindex(1));

	struct plua_metatable_t *cpy = NULL;
	int i = 0, is_table = 0;

	if(lua_gettop(L) != 1) {
		pluaL_error(L, "event.trigger requires 1 argument, %d given", lua_gettop(L));
	}

	{
		char buf[128] = { '\0' }, *p = buf;
		char *error = "userdata expected, got %s";

		sprintf(p, error, lua_typename(L, lua_type(L, -1)));

		luaL_argcheck(L,
			(lua_type(L, -1) == LUA_TLIGHTUSERDATA || lua_type(L, -1) == LUA_TTABLE),
			1, buf);

		if(lua_type(L, -1) == LUA_TLIGHTUSERDATA) {
			cpy = (void *)lua_topointer(L, -1);
			atomic_inc(cpy->ref);
			lua_remove(L, -1);
		} else if(lua_type(L, -1) == LUA_TTABLE) {
			lua_pushnil(L);
			is_table = 1;

			plua_metatable_init(&cpy);
			while(lua_next(L, -2) != 0) {
				plua_metatable_parse_set(L, cpy);
				lua_pop(L, 1);
			}
			lua_remove(L, -1);
		}
	}

	for(i=0;i<REASON_END+10000;i++) {
		if(event->reasons[i].active == 1) {
			if(is_table == 1) {
				struct plua_metatable_t *table = NULL;
				plua_metatable_clone(&cpy, &table);
				eventpool_trigger(i, plua_async_event_free, table);
			} else {
				eventpool_trigger(i, plua_async_event_free, cpy);
			}
		}
	}
	if(is_table == 1) {
		plua_metatable_free(cpy);
	}

	lua_pushboolean(L, 1);

	assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

	return 1;
}

static int plua_async_event_set_callback(lua_State *L) {
	struct lua_event_t *event = (void *)lua_topointer(L, lua_upvalueindex(1));
	int had_callback = 0;

	if(lua_gettop(L) != 1) {
		pluaL_error(L, "event.setCallback requires 1 argument, %d given", lua_gettop(L));
	}

	if(event == NULL) {
		pluaL_error(L, "internal error: event object not passed");
	}

	if(event->module == NULL) {
		pluaL_error(L, "internal error: lua state not properly initialized");
	}

	const char *func = NULL;
	char buf[128] = { '\0' }, *p = buf, name[255] = { '\0' };
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING) || (lua_type(L, -1) == LUA_TNIL),
		1, buf);

	if(lua_type(L, -1) == LUA_TSTRING) {
		func = lua_tostring(L, -1);
		lua_remove(L, -1);
	} else if(lua_type(L, -1) == LUA_TNIL) {
		lua_remove(L, -1);
		if(event->callback != NULL) {
			FREE(event->callback);
		}

		lua_pushboolean(L, 1);

		assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

		return 1;
	}

	p = name;
	plua_namespace(event->module, p);

	lua_getglobal(L, name);
	if(lua_type(L, -1) == LUA_TNIL) {
		pluaL_error(L, "cannot find %s lua module", event->module->name);
	}

	lua_getfield(L, -1, func);
	if(lua_type(L, -1) != LUA_TFUNCTION) {
		pluaL_error(L, "%s: event callback %s does not exist", event->module->file, func);
	}

	if(event->callback != NULL) {
		had_callback = 1;
		FREE(event->callback);
	}

	if((event->callback = STRDUP((char *)func)) == NULL) {
		OUT_OF_MEMORY
	}

	int is_active = 0, i = 0;
	for(i=0;i<REASON_END+10000;i++) {
		if(event->reasons[i].active == 1) {
			is_active = 1;
			break;
		}
	}

	if(had_callback == 0 && is_active == 1) {
		int i = 0;
		plua_gc_unreg(L, event);
		for(i=0;i<REASON_END+10000;i++) {
			if(event->reasons[i].active == 1) {
				event->reasons[i].node = eventpool_callback(i, plua_async_event_callback, event);
			}
		}
	}

	lua_remove(L, -1);
	lua_remove(L, -1);

	lua_pushboolean(L, 1);

	assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

	return 1;
}

static int plua_async_event_set_data(lua_State *L) {
	struct lua_thread_t *event = (void *)lua_topointer(L, lua_upvalueindex(1));

	return plua_set_userdata(L, (struct plua_interface_t *)event, "event");
}

static int plua_async_event_get_data(lua_State *L) {
	struct lua_thread_t *event = (void *)lua_topointer(L, lua_upvalueindex(1));

	return plua_get_userdata(L, (struct plua_interface_t *)event, "event");
}

static int plua_async_event_call(lua_State *L) {
	struct lua_mqtt_t *event = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(event == NULL) {
		logprintf(LOG_ERR, "internal error: event object not passed");
		return 0;
	}

	lua_pushlightuserdata(L, event);

	return 1;
}

static void plua_async_event_object(lua_State *L, struct lua_event_t *event) {
	lua_newtable(L);

	lua_pushstring(L, "trigger");
	lua_pushlightuserdata(L, event);
	lua_pushcclosure(L, plua_async_event_trigger, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "register");
	lua_pushlightuserdata(L, event);
	lua_pushcclosure(L, plua_async_event_register, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "unregister");
	lua_pushlightuserdata(L, event);
	lua_pushcclosure(L, plua_async_event_unregister, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setCallback");
	lua_pushlightuserdata(L, event);
	lua_pushcclosure(L, plua_async_event_set_callback, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getUserdata");
	lua_pushlightuserdata(L, event);
	lua_pushcclosure(L, plua_async_event_get_data, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setUserdata");
	lua_pushlightuserdata(L, event);
	lua_pushcclosure(L, plua_async_event_set_data, 1);
	lua_settable(L, -3);

	lua_newtable(L);

	lua_pushstring(L, "__call");
	lua_pushlightuserdata(L, event);
	lua_pushcclosure(L, plua_async_event_call, 1);
	lua_settable(L, -3);

	lua_setmetatable(L, -2);
}

int plua_async_event(struct lua_State *L) {
	struct lua_event_t *lua_event = NULL;

	int i = 0;
	if(lua_gettop(L) < 0 || lua_gettop(L) > 1) {
		pluaL_error(L, "event requires 0 arguments, %d given", lua_gettop(L));
		return 0;
	}

	if(lua_gettop(L) == 1) {
		char buf[128] = { '\0' }, *p = buf;
		char *error = "lightuserdata expected, got %s";

		sprintf(p, error, lua_typename(L, lua_type(L, -1)));

		luaL_argcheck(L,
			(lua_type(L, -1) == LUA_TLIGHTUSERDATA),
			1, buf);

		lua_event = lua_touserdata(L, -1);
		lua_remove(L, -1);

		if(lua_event->type != PLUA_EVENT) {
			luaL_error(L, "thread object required but %s object passed", plua_interface_to_string(lua_event->type));
		}
		atomic_inc(lua_event->ref);
	} else {
		struct lua_state_t *state = plua_get_current_state(L);
		if(state == NULL) {
			return 0;
		}

		lua_event = MALLOC(sizeof(struct lua_event_t));
		if(lua_event == NULL) {
			OUT_OF_MEMORY
		}
		memset(lua_event, 0, sizeof(struct lua_event_t));
		lua_event->type = PLUA_EVENT;
		lua_event->ref = 1;
		lua_event->callback = NULL;
		lua_event->gc = plua_async_event_gc;

		plua_metatable_init(&lua_event->table);

		for(i=0;i<REASON_END+10000;i++) {
			lua_event->reasons[i].active = 0;
			lua_event->reasons[i].node = NULL;
		}

		lua_event->module = state->module;
		lua_event->L = L;
		plua_gc_reg(NULL, lua_event, plua_async_event_global_gc);
	}
	plua_gc_reg(L, lua_event, plua_async_event_gc);

	plua_async_event_object(L, lua_event);

	assert(plua_check_stack(L, 1, PLUA_TTABLE) == 0);

	return 1;
}
