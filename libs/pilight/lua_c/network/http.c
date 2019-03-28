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
#include "../../core/http.h"
#include "../table.h"
#include "../network.h"

#define GET 0
#define POST 1

typedef struct lua_http_t {
	PLUA_INTERFACE_FIELDS

	char *url;
	char *mimetype;
	char *data;
	char *callback;

	int code;
	int size;
	int type;
} lua_http_t;

static void plua_network_http_object(lua_State *L, struct lua_http_t *http);
static void plua_network_http_gc(void *ptr);

static int plua_network_http_set_userdata(lua_State *L) {
	struct lua_http_t *http = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 1) {
		luaL_error(L, "http.setUserdata requires 1 argument, %d given", lua_gettop(L));
	}

	if(http == NULL) {
		luaL_error(L, "internal error: http object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "userdata expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TLIGHTUSERDATA || lua_type(L, -1) == LUA_TTABLE),
		1, buf);

	if(lua_type(L, -1) == LUA_TLIGHTUSERDATA) {
		if(http->table != (void *)lua_topointer(L, -1)) {
			plua_metatable_free(http->table);
		}
		http->table = (void *)lua_topointer(L, -1);

		if(http->table->ref != NULL) {
			uv_sem_post(http->table->ref);
		}

		plua_ret_true(L);

		return 1;
	}

	if(lua_type(L, -1) == LUA_TTABLE) {
		lua_pushnil(L);
		while(lua_next(L, -2) != 0) {
			plua_metatable_parse_set(L, http->table);
			lua_pop(L, 1);
		}

		plua_ret_true(L);
		return 1;
	}

	plua_ret_false(L);

	return 0;
}

static int plua_network_http_get_userdata(lua_State *L) {
	struct lua_http_t *http = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "http.getUserdata requires 0 argument, %d given", lua_gettop(L));
		return 0;
	}

	if(http == NULL) {
		luaL_error(L, "internal error: http object not passed");
		return 0;
	}

	plua_metatable__push(L, (struct plua_interface_t *)http);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_http_set_mimetype(lua_State *L) {
	struct lua_http_t *http = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *mimetype = NULL;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "http.setMimetype requires 1 argument, %d given", lua_gettop(L));
	}

	if(http == NULL) {
		luaL_error(L, "internal error: http object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	mimetype = (void *)lua_tostring(L, -1);
	if(http->mimetype != NULL) {
		FREE(http->mimetype);
	}
	if((http->mimetype = STRDUP(mimetype)) == NULL) {
		OUT_OF_MEMORY
	}
	lua_remove(L, -1);

	lua_pushboolean(L, 1);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_http_set_data(lua_State *L) {
	struct lua_http_t *http = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *data = NULL;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "http.setData requires 1 argument, %d given", lua_gettop(L));
	}

	if(http == NULL) {
		luaL_error(L, "internal error: http object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	data = (void *)lua_tostring(L, -1);
	if(http->data != NULL) {
		FREE(http->data);
	}
	if((http->data = STRDUP(data)) == NULL) {
		OUT_OF_MEMORY
	}
	lua_remove(L, -1);

	lua_pushboolean(L, 1);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_http_set_url(lua_State *L) {
	struct lua_http_t *http = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *url = NULL;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "http.setUrl requires 1 argument, %d given", lua_gettop(L));
	}

	if(http == NULL) {
		luaL_error(L, "internal error: http object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	url = (void *)lua_tostring(L, -1);
	if(http->url != NULL) {
		FREE(http->url);
	}
	if((http->url = STRDUP(url)) == NULL) {
		OUT_OF_MEMORY
	}
	lua_remove(L, -1);

	lua_pushboolean(L, 1);

	assert(lua_gettop(L) == 1);

	return 1;
}

static void plua_network_http_callback(int code, char *content, int size, char *type, void *userdata) {
	struct lua_http_t *data = userdata;
	char name[255], *p = name;
	memset(name, '\0', 255);

	/*
	 * Only create a new state once the http callback is called
	 */
	struct lua_state_t *state = plua_get_free_state();
	state->module = data->module;

	logprintf(LOG_DEBUG, "lua http on state #%d", state->idx);

	plua_namespace(state->module, p);

	lua_getglobal(state->L, name);
	if(lua_type(state->L, -1) == LUA_TNIL) {
		luaL_error(state->L, "cannot find %s lua module", name);
	}

	lua_getfield(state->L, -1, data->callback);

	if(lua_type(state->L, -1) != LUA_TFUNCTION) {
		luaL_error(state->L, "%s: http callback %s does not exist", state->module->file, data->callback);
	}

	plua_network_http_object(state->L, data);

	data->code = code;
	data->size = size;

	if(type != NULL) {
		if(data->mimetype != NULL) {
			FREE(data->mimetype);
		}
		if((data->mimetype = STRDUP(type)) == NULL) {
			OUT_OF_MEMORY
		}
	}

	if(content != NULL) {
		if(data->data != NULL) {
			FREE(data->data);
		}
		if((data->data = STRDUP((char *)content)) == NULL) {
			OUT_OF_MEMORY
		}
	}

	plua_gc_unreg(state->L, data);
	if(lua_pcall(state->L, 1, 0, 0) == LUA_ERRRUN) {
		if(lua_type(state->L, -1) == LUA_TNIL) {
			logprintf(LOG_ERR, "%s: syntax error", state->module->file);
			goto error;
		}
		if(lua_type(state->L, -1) == LUA_TSTRING) {
			logprintf(LOG_ERR, "%s", lua_tostring(state->L,  -1));
			lua_pop(state->L, -1);
			plua_clear_state(state);
			goto error;
		}
	}
	lua_remove(state->L, 1);
	plua_clear_state(state);

error:
	FREE(data->url);
	if(data->mimetype != NULL) {
		FREE(data->mimetype);
	}
	if(data->data != NULL) {
		FREE(data->data);
	}

	plua_metatable_free(data->table);
	FREE(data->callback);
	FREE(data);
}

static int plua_network_http_set_callback(lua_State *L) {
	struct lua_http_t *http = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *func = NULL;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "http.setCallback requires 1 argument, %d given", lua_gettop(L));
	}

	if(http == NULL) {
		luaL_error(L, "internal error: http object not passed");
	}

	if(http->module == NULL) {
		luaL_error(L, "internal error: lua state not properly initialized");
	}

	char buf[128] = { '\0' }, *p = buf, name[255] = { '\0' };
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	func = (void *)lua_tostring(L, -1);
	lua_remove(L, -1);

	p = name;
	plua_namespace(http->module, p);

	lua_getglobal(L, name);
	if(lua_type(L, -1) == LUA_TNIL) {
		luaL_error(L, "cannot find %s lua module", http->module->name);
	}

	lua_getfield(L, -1, func);
	if(lua_type(L, -1) != LUA_TFUNCTION) {
		luaL_error(L, "%s: http callback %s does not exist", http->module->file, func);
	}
	lua_remove(L, -1);
	lua_remove(L, -1);

	if(http->callback != NULL) {
		FREE(http->callback);
	}
	if((http->callback = STRDUP(func)) == NULL) {
		OUT_OF_MEMORY
	}

	lua_pushboolean(L, 1);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_http_get(lua_State *L) {
	struct lua_http_t *http = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "http.get requires 0 arguments, %d given", lua_gettop(L));
	}

	if(http == NULL) {
		luaL_error(L, "internal error: http object not passed");
	}

	http->type = GET;

	if(http->url == NULL) {
		luaL_error(L, "http server url not set");
	}

	if(http->callback == NULL) {
		luaL_error(L, "http callback not set");
	}

	if(http_get_content(http->url, plua_network_http_callback, http) != 0) {
		plua_gc_unreg(http->L, http);
		plua_network_http_gc((void *)http);

		lua_pushboolean(L, 0);
		assert(lua_gettop(L) == 1);

		return 1;
	}

	plua_gc_unreg(http->L, http);

	lua_pushboolean(L, 1);
	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_http_post(lua_State *L) {
	struct lua_http_t *http = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "http.post requires 0 arguments, %d given", lua_gettop(L));
	}

	if(http == NULL) {
		luaL_error(L, "internal error: http object not passed");
	}

	http->type = POST;

	if(http->url == NULL) {
		luaL_error(L, "http server url not set");
	}

	if(http->data == NULL) {
		luaL_error(L, "http data not set");
	}

	if(http->mimetype == NULL) {
		luaL_error(L, "http mimetype not set");
	}

	if(http->callback == NULL) {
		luaL_error(L, "http callback not set");
	}

	if(http_post_content(http->url, http->mimetype, http->data, plua_network_http_callback, http) != 0) {
		plua_gc_unreg(http->L, http);
		plua_network_http_gc((void *)http);

		lua_pushboolean(L, 0);
		assert(lua_gettop(L) == 1);

		return 1;
	}

	plua_gc_unreg(http->L, http);

	lua_pushboolean(L, 1);
	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_http_get_code(lua_State *L) {
	struct lua_http_t *http = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "http.getCode requires 0 arguments, %d given", lua_gettop(L));
	}

	if(http == NULL) {
		luaL_error(L, "internal error: http object not passed");
	}

	if(http->code > -1) {
		lua_pushnumber(L, http->code);
	} else {
		lua_pushnil(L);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_http_get_size(lua_State *L) {
	struct lua_http_t *http = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "http.getSize requires 0 arguments, %d given", lua_gettop(L));
	}

	if(http == NULL) {
		luaL_error(L, "internal error: http object not passed");
	}

	if(http->size > -1) {
		lua_pushnumber(L, http->size);
	} else {
		lua_pushnil(L);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_http_get_mimetype(lua_State *L) {
	struct lua_http_t *http = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "http.getSubject requires 0 arguments, %d given", lua_gettop(L));
	}

	if(http == NULL) {
		luaL_error(L, "internal error: http object not passed");
	}

	if(http->mimetype != NULL) {
		lua_pushstring(L, http->mimetype);
	} else {
		lua_pushnil(L);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_http_get_url(lua_State *L) {
	struct lua_http_t *http = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "http.getTo requires 0 arguments, %d given", lua_gettop(L));
	}

	if(http == NULL) {
		luaL_error(L, "internal error: http object not passed");
	}

	if(http->url != NULL) {
		lua_pushstring(L, http->url);
	} else {
		lua_pushnil(L);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_http_get_data(lua_State *L) {
	struct lua_http_t *http = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "http.getData requires 0 arguments, %d given", lua_gettop(L));
	}

	if(http == NULL) {
		luaL_error(L, "internal error: http object not passed");
	}

	if(http->data != NULL) {
		lua_pushstring(L, http->data);
	} else {
		lua_pushnil(L);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_http_get_callback(lua_State *L) {
	struct lua_http_t *http = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "http.getSSL requires 0 arguments, %d given", lua_gettop(L));
	}

	if(http == NULL) {
		luaL_error(L, "internal error: http object not passed");
	}

	if(http->callback != NULL) {
		lua_pushstring(L, http->callback);
	} else {
		lua_pushnil(L);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

static void plua_network_http_gc(void *ptr) {
	struct lua_http_t *data = ptr;

	if(data->url != NULL) {
		FREE(data->url);
	}
	if(data->mimetype != NULL) {
		FREE(data->mimetype);
	}
	if(data->data != NULL) {
		FREE(data->data);
	}

	plua_metatable_free(data->table);

	if(data->callback != NULL) {
		FREE(data->callback);
	}
	FREE(data);
}

static void plua_network_http_object(lua_State *L, struct lua_http_t *http) {
	lua_newtable(L);

	lua_pushstring(L, "setMimetype");
	lua_pushlightuserdata(L, http);
	lua_pushcclosure(L, plua_network_http_set_mimetype, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getMimetype");
	lua_pushlightuserdata(L, http);
	lua_pushcclosure(L, plua_network_http_get_mimetype, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setData");
	lua_pushlightuserdata(L, http);
	lua_pushcclosure(L, plua_network_http_set_data, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getData");
	lua_pushlightuserdata(L, http);
	lua_pushcclosure(L, plua_network_http_get_data, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setUrl");
	lua_pushlightuserdata(L, http);
	lua_pushcclosure(L, plua_network_http_set_url, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getUrl");
	lua_pushlightuserdata(L, http);
	lua_pushcclosure(L, plua_network_http_get_url, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setCallback");
	lua_pushlightuserdata(L, http);
	lua_pushcclosure(L, plua_network_http_set_callback, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getCallback");
	lua_pushlightuserdata(L, http);
	lua_pushcclosure(L, plua_network_http_get_callback, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "get");
	lua_pushlightuserdata(L, http);
	lua_pushcclosure(L, plua_network_http_get, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "post");
	lua_pushlightuserdata(L, http);
	lua_pushcclosure(L, plua_network_http_post, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getCode");
	lua_pushlightuserdata(L, http);
	lua_pushcclosure(L, plua_network_http_get_code, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getSize");
	lua_pushlightuserdata(L, http);
	lua_pushcclosure(L, plua_network_http_get_size, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getUserdata");
	lua_pushlightuserdata(L, http);
	lua_pushcclosure(L, plua_network_http_get_userdata, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setUserdata");
	lua_pushlightuserdata(L, http);
	lua_pushcclosure(L, plua_network_http_set_userdata, 1);
	lua_settable(L, -3);
}

int plua_network_http(struct lua_State *L) {
	if(lua_gettop(L) != 0) {
		luaL_error(L, "timer requires 0 arguments, %d given", lua_gettop(L));
		return 0;
	}

	struct lua_state_t *state = plua_get_current_state(L);
	if(state == NULL) {
		return 0;
	}

	struct lua_http_t *lua_http = MALLOC(sizeof(struct lua_http_t));
	if(lua_http == NULL) {
		OUT_OF_MEMORY
	}
	memset(lua_http, '\0', sizeof(struct lua_http_t));

	plua_metatable_init(&lua_http->table);

	lua_http->module = state->module;
	lua_http->L = L;
	lua_http->code = -1;

	plua_gc_reg(L, lua_http, plua_network_http_gc);

	plua_network_http_object(L, lua_http);

	lua_assert(lua_gettop(L) == 1);

	return 1;
}
