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
#include "../../core/mail.h"
#include "../network.h"

typedef struct lua_mail_t {
	struct plua_metatable_t *table;
	struct plua_module_t *module;
	lua_State *L;

	struct mail_t mail;

	char *host;
	char *user;
	char *password;
	int port;
	int is_ssl;
	char *callback;

	int status;
} lua_mail_t;

static void plua_network_mail_object(lua_State *L, struct lua_mail_t *mail);
static void plua_network_mail_gc(void *ptr);

static int plua_network_mail_set_data(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));
	struct plua_metatable_t *cpy = NULL;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "mail.setData requires 1 argument, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "userdata expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TLIGHTUSERDATA || lua_type(L, -1) == LUA_TTABLE),
		1, buf);

	if(lua_type(L, -1) == LUA_TLIGHTUSERDATA) {
		cpy = (void *)lua_topointer(L, -1);
		lua_remove(L, -1);
		plua_metatable_clone(&cpy, &mail->table);

		plua_ret_true(L);

		return 1;
	}

	if(lua_type(L, -1) == LUA_TTABLE) {
		lua_pushnil(L);
		while(lua_next(L, -2) != 0) {
			plua_metatable_parse_set(L, mail->table);
			lua_pop(L, 1);
		}

		plua_ret_true(L);
		return 1;
	}

	plua_ret_false(L);

	return 0;
}

static int plua_network_mail_get_data(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "mail.setUserdata requires 0 argument, %d given", lua_gettop(L));
		return 0;
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
		return 0;
	}

	lua_newtable(L);
	lua_newtable(L);

	lua_pushstring(L, "__index");
	lua_pushlightuserdata(L, mail->table);
	lua_pushcclosure(L, plua_metatable_get, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "__newindex");
	lua_pushlightuserdata(L, mail->table);
	lua_pushcclosure(L, plua_metatable_set, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "__gc");
	lua_pushlightuserdata(L, mail->table);
	lua_pushcclosure(L, plua_metatable_gc, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "__pairs");
	lua_pushlightuserdata(L, mail->table);
	lua_pushcclosure(L, plua_metatable_pairs, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "__next");
	lua_pushlightuserdata(L, mail->table);
	lua_pushcclosure(L, plua_metatable_next, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "__call");
	lua_pushlightuserdata(L, mail->table);
	lua_pushcclosure(L, plua_metatable_call, 1);
	lua_settable(L, -3);

	lua_setmetatable(L, -2);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_mail_set_subject(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *subject = NULL;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "mail.setSubject requires 1 argument, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	subject = (void *)lua_tostring(L, -1);
	if((mail->mail.subject = STRDUP(subject)) == NULL) {
		OUT_OF_MEMORY
	}
	lua_remove(L, -1);

	lua_pushboolean(L, 1);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_mail_set_from(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *from = NULL;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "mail.setFrom requires 1 argument, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	from = (void *)lua_tostring(L, -1);
	if((mail->mail.from = STRDUP(from)) == NULL) {
		OUT_OF_MEMORY
	}
	lua_remove(L, -1);

	lua_pushboolean(L, 1);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_mail_set_to(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *to = NULL;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "mail.setTo requires 1 argument, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	to = (void *)lua_tostring(L, -1);
	if((mail->mail.to = STRDUP(to)) == NULL) {
		OUT_OF_MEMORY
	}
	lua_remove(L, -1);

	lua_pushboolean(L, 1);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_mail_set_message(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *message = NULL;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "mail.setMessage requires 1 argument, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	message = (void *)lua_tostring(L, -1);
	if((mail->mail.message = STRDUP(message)) == NULL) {
		OUT_OF_MEMORY
	}
	lua_remove(L, -1);

	lua_pushboolean(L, 1);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_mail_set_port(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));
	int port = 0;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "mail.setPort requires 1 argument, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "number expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TNUMBER),
		1, buf);

	port = lua_tonumber(L, -1);
	mail->port = port;

	lua_remove(L, -1);

	lua_pushboolean(L, 1);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_mail_set_host(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *host = NULL;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "mail.setHost requires 1 argument, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	host = (void *)lua_tostring(L, -1);
	if((mail->host = STRDUP(host)) == NULL) {
		OUT_OF_MEMORY
	}
	lua_remove(L, -1);

	lua_pushboolean(L, 1);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_mail_set_user(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *user = NULL;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "mail.setUser requires 1 argument, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	user = (void *)lua_tostring(L, -1);
	if((mail->user = STRDUP(user)) == NULL) {
		OUT_OF_MEMORY
	}
	lua_remove(L, -1);

	lua_pushboolean(L, 1);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_mail_set_password(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *password = NULL;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "mail.setPassword requires 1 argument, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "string expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TSTRING),
		1, buf);

	password = (void *)lua_tostring(L, -1);
	if((mail->password = STRDUP(password)) == NULL) {
		OUT_OF_MEMORY
	}
	lua_remove(L, -1);

	lua_pushboolean(L, 1);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_mail_set_callback(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *func = NULL;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "mail.setCallback requires 1 argument, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	if(mail->module == NULL) {
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
	switch(mail->module->type) {
		case UNITTEST: {
			sprintf(p, "unittest.%s", mail->module->name);
		} break;
		case FUNCTION: {
			sprintf(p, "function.%s", mail->module->name);
		} break;
		case OPERATOR: {
			sprintf(p, "operator.%s", mail->module->name);
		} break;
		case ACTION: {
			sprintf(p, "action.%s", mail->module->name);
		} break;
	}

	lua_getglobal(L, name);
	if(lua_type(L, -1) == LUA_TNIL) {
		luaL_error(L, "cannot find %s lua module", mail->module->name);
	}

	lua_getfield(L, -1, func);
	if(lua_type(L, -1) != LUA_TFUNCTION) {
		luaL_error(L, "%s: mail callback %s does not exist", mail->module->file, func);
	}
	lua_remove(L, -1);
	lua_remove(L, -1);

	if((mail->callback = STRDUP(func)) == NULL) {
		OUT_OF_MEMORY
	}

	lua_pushboolean(L, 1);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_mail_set_ssl(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));
	int ssl = 0;

	if(lua_gettop(L) != 1) {
		luaL_error(L, "mail.setSSL requires 1 argument, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "number expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TNUMBER) || (lua_type(L, -1) == LUA_TBOOLEAN),
		1, buf);

	ssl = lua_tonumber(L, -1);
	mail->is_ssl = ssl;


	lua_remove(L, -1);

	lua_pushboolean(L, 1);

	assert(lua_gettop(L) == 1);

	return 1;
}

static void plua_network_mail_callback(int status, struct mail_t *mail) {
	struct lua_mail_t *data = mail->data;
	char name[255], *p = name;
	memset(name, '\0', 255);

	/*
	 * Only create a new state once the mail callback is called
	 */
	struct lua_state_t *state = plua_get_free_state();
	state->module = data->module;

	logprintf(LOG_DEBUG, "lua mail on state #%d", state->idx);

	switch(state->module->type) {
		case UNITTEST: {
			sprintf(p, "unittest.%s", state->module->name);
		} break;
		case FUNCTION: {
			sprintf(p, "function.%s", state->module->name);
		} break;
		case OPERATOR: {
			sprintf(p, "operator.%s", state->module->name);
		} break;
		case ACTION: {
			sprintf(p, "action.%s", state->module->name);
		} break;
	}

	lua_getglobal(state->L, name);
	if(lua_type(state->L, -1) == LUA_TNIL) {
		luaL_error(state->L, "cannot find %s lua module", name);
	}

	lua_getfield(state->L, -1, data->callback);

	if(lua_type(state->L, -1) != LUA_TFUNCTION) {
		luaL_error(state->L, "%s: mail callback %s does not exist", state->module->file, data->callback);
	}

	plua_network_mail_object(state->L, data);

	data->status = status;

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
	FREE(mail->from);
	FREE(mail->to);
	FREE(mail->message);
	FREE(mail->subject);

	plua_metatable_free(data->table);
	FREE(data->password);
	FREE(data->callback);
	FREE(data->user);
	FREE(data->host);
	FREE(data);
}

static int plua_network_mail_send(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "mail.send requires 0 arguments, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	if(mail->host == NULL) {
		luaL_error(L, "mail server host not set");
	}

	if(mail->user == NULL) {
		luaL_error(L, "mail server user not set");
	}

	if(mail->password == NULL) {
		luaL_error(L, "mail server password not set");
	}

	if(mail->port == 0) {
		luaL_error(L, "mail server port not set");
	}

	if(mail->mail.subject == NULL) {
		luaL_error(L, "mail subject port not set");
	}

	if(mail->mail.to == NULL) {
		luaL_error(L, "mail recipient not set");
	}

	if(mail->mail.from == NULL) {
		luaL_error(L, "mail sender not set");
	}

	if(mail->mail.message == NULL) {
		luaL_error(L, "mail message not set");
	}

	if(mail->callback == NULL) {
		luaL_error(L, "mail callback not set");
	}

	mail->mail.data = mail;

	if(sendmail(
		mail->host,
		mail->user,
		mail->password,
		mail->port,
		mail->is_ssl,
		&mail->mail, 
		plua_network_mail_callback) != 0) {

		plua_gc_unreg(mail->L, mail);
		plua_network_mail_gc((void *)mail);

		lua_pushboolean(L, 0);
		assert(lua_gettop(L) == 1);

		return 1;
	}

	plua_gc_unreg(mail->L, mail);

	lua_pushboolean(L, 1);
	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_mail_get_status(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "mail.getStatus requires 0 arguments, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	switch(mail->status) {
		case -1:
			lua_pushboolean(L, 0);
		break;
		case 0:
			lua_pushboolean(L, 1);
		break;
		case 1:
			lua_pushnil(L);
		break;
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_mail_get_subject(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "mail.getSubject requires 0 arguments, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	if(mail->mail.subject != NULL) {
		lua_pushstring(L, mail->mail.subject);
	} else {
		lua_pushnil(L);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_mail_get_to(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "mail.getTo requires 0 arguments, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	if(mail->mail.to != NULL) {
		lua_pushstring(L, mail->mail.to);
	} else {
		lua_pushnil(L);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_mail_get_from(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "mail.getFrom requires 0 arguments, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	if(mail->mail.from != NULL) {
		lua_pushstring(L, mail->mail.from);
	} else {
		lua_pushnil(L);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_mail_get_message(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "mail.getMessage requires 0 arguments, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	if(mail->mail.message != NULL) {
		lua_pushstring(L, mail->mail.message);
	} else {
		lua_pushnil(L);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_mail_get_host(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "mail.getHost requires 0 arguments, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	if(mail->host != NULL) {
		lua_pushstring(L, mail->host);
	} else {
		lua_pushnil(L);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_mail_get_user(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "mail.getUser requires 0 arguments, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	if(mail->user != NULL) {
		lua_pushstring(L, mail->user);
	} else {
		lua_pushnil(L);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_mail_get_password(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "mail.getPassword requires 0 arguments, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	if(mail->password != NULL) {
		lua_pushstring(L, mail->password);
	} else {
		lua_pushnil(L);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_mail_get_ssl(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "mail.getSSL requires 0 arguments, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	lua_pushnumber(L, mail->is_ssl);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_mail_get_port(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "mail.getPort requires 0 arguments, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	lua_pushnumber(L, mail->port);

	assert(lua_gettop(L) == 1);

	return 1;
}

static int plua_network_mail_get_callback(lua_State *L) {
	struct lua_mail_t *mail = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		luaL_error(L, "mail.getSSL requires 0 arguments, %d given", lua_gettop(L));
	}

	if(mail == NULL) {
		luaL_error(L, "internal error: mail object not passed");
	}

	if(mail->callback != NULL) {
		lua_pushstring(L, mail->callback);
	} else {
		lua_pushnil(L);
	}

	assert(lua_gettop(L) == 1);

	return 1;
}

static void plua_network_mail_gc(void *ptr) {
	struct lua_mail_t *data = ptr;

	if(data->mail.from != NULL) {
		FREE(data->mail.from);
	}
	if(data->mail.to != NULL) {
		FREE(data->mail.to);
	}
	if(data->mail.message != NULL) {
		FREE(data->mail.message);
	}
	if(data->mail.subject != NULL) {
		FREE(data->mail.subject);
	}

	plua_metatable_free(data->table);

	if(data->password != NULL) {
		FREE(data->password);
	}
	if(data->callback != NULL) {
		FREE(data->callback);
	}
	if(data->user != NULL) {
		FREE(data->user);
	}
	if(data->host != NULL) {
		FREE(data->host);
	}
	FREE(data);
}

static void plua_network_mail_object(lua_State *L, struct lua_mail_t *mail) {
	lua_newtable(L);

	lua_pushstring(L, "setSubject");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_set_subject, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getSubject");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_get_subject, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setFrom");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_set_from, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getFrom");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_get_from, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setTo");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_set_to, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getTo");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_get_to, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setMessage");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_set_message, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getMessage");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_get_message, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setHost");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_set_host, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getHost");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_get_host, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setPort");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_set_port, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getPort");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_get_port, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setUser");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_set_user, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getUser");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_get_user, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setPassword");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_set_password, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getPassword");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_get_password, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setSSL");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_set_ssl, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getSSL");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_get_ssl, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setCallback");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_set_callback, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getCallback");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_get_callback, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "send");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_send, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getStatus");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_get_status, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getUserdata");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_get_data, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setUserdata");
	lua_pushlightuserdata(L, mail);
	lua_pushcclosure(L, plua_network_mail_set_data, 1);
	lua_settable(L, -3);
}

int plua_network_mail(struct lua_State *L) {
	if(lua_gettop(L) != 0) {
		luaL_error(L, "timer requires 0 arguments, %d given", lua_gettop(L));
		return 0;
	}

	struct lua_state_t *state = plua_get_current_state(L);
	if(state == NULL) {
		return 0;
	}

	struct lua_mail_t *lua_mail = MALLOC(sizeof(struct lua_mail_t));
	if(lua_mail == NULL) {
		OUT_OF_MEMORY
	}
	memset(lua_mail, '\0', sizeof(struct lua_mail_t));

	if((lua_mail->table = MALLOC(sizeof(struct lua_mail_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(lua_mail->table, 0, sizeof(struct lua_mail_t));

	lua_mail->module = state->module;
	lua_mail->L = L;
	lua_mail->status = 1;

	plua_gc_reg(L, lua_mail, plua_network_mail_gc);

	plua_network_mail_object(L, lua_mail);

	lua_assert(lua_gettop(L) == 1);

	return 1;
}
