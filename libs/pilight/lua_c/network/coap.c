/*
  Copyright (C) CurlyMo

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
#include "../../core/coap.h"
#include "../table.h"
#include "../network.h"

#define LISTEN	0
#define	SEND		1

typedef struct lua_coap_t {
	PLUA_INTERFACE_FIELDS

	int type;
	char *callback;
} lua_coap_t;

static void plua_network_coap_object(lua_State *L, struct lua_coap_t *coap);
static void plua_network_coap_gc(void *ptr);

static int plua_network_coap_set_userdata(lua_State *L) {
	struct lua_coap_t *coap = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 1) {
		pluaL_error(L, "coap.setUserdata requires 1 argument, %d given", lua_gettop(L));
	}

	if(coap == NULL) {
		pluaL_error(L, "internal error: coap object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "userdata expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TLIGHTUSERDATA || lua_type(L, -1) == LUA_TTABLE),
		1, buf);

	if(lua_type(L, -1) == LUA_TLIGHTUSERDATA) {
		if(coap->table != (void *)lua_topointer(L, -1)) {
			plua_metatable_free(coap->table);
		}
		coap->table = (void *)lua_topointer(L, -1);

		if(coap->table->ref != NULL) {
			uv_sem_post(coap->table->ref);
		}

		lua_pushboolean(L, 1);

		assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

		return 1;
	}

	if(lua_type(L, -1) == LUA_TTABLE) {
		lua_pushnil(L);
		while(lua_next(L, -2) != 0) {
			plua_metatable_parse_set(L, coap->table);
			lua_pop(L, 1);
		}

		lua_pushboolean(L, 1);

		assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

		return 1;
	}

	lua_pushboolean(L, 0);

	assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

	return 1;
}

static int plua_network_coap_get_userdata(lua_State *L) {
	struct lua_coap_t *coap = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		pluaL_error(L, "coap.getUserdata requires 0 argument, %d given", lua_gettop(L));
		return 0;
	}

	if(coap == NULL) {
		pluaL_error(L, "internal error: coap object not passed");
		return 0;
	}

	push__plua_metatable(L, (struct plua_interface_t *)coap);

	assert(plua_check_stack(L, 1, PLUA_TTABLE) == 0);

	return 1;
}

static int plua_network_coap_set_callback(lua_State *L) {
	struct lua_coap_t *coap = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *func = NULL;

	if(lua_gettop(L) != 1) {
		pluaL_error(L, "coap.setCallback requires 1 argument, %d given", lua_gettop(L));
	}

	if(coap == NULL) {
		pluaL_error(L, "internal error: coap object not passed");
	}

	if(coap->module == NULL) {
		pluaL_error(L, "internal error: lua state not properly initialized");
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
	plua_namespace(coap->module, p);

	lua_getglobal(L, name);
	if(lua_type(L, -1) == LUA_TNIL) {
		pluaL_error(L, "cannot find %s lua module", coap->module->name);
	}

	lua_getfield(L, -1, func);
	if(lua_type(L, -1) != LUA_TFUNCTION) {
		pluaL_error(L, "%s: coap callback %s does not exist", coap->module->file, func);
	}
	lua_remove(L, -1);
	lua_remove(L, -1);

	if(coap->callback != NULL) {
		FREE(coap->callback);
	}
	if((coap->callback = STRDUP(func)) == NULL) {
		OUT_OF_MEMORY
	}

	lua_pushboolean(L, 1);

	assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

	return 1;
}


static void read_cb(const struct sockaddr *addr, struct coap_packet_t *pkt, void *userdata) {
	struct lua_coap_t *data = userdata;

	char name[255], *p = name;
	memset(name, '\0', 255);

	/*
	 * Only create a new state once the http callback is called
	 */
	struct lua_state_t *state = plua_get_free_state();
	state->module = data->module;

	logprintf(LOG_DEBUG, "lua coap on state #%d", state->idx);

	plua_namespace(state->module, p);

	lua_getglobal(state->L, name);
	if(lua_type(state->L, -1) == LUA_TNIL) {
		pluaL_error(state->L, "cannot find %s lua module", name);
	}

	lua_getfield(state->L, -1, data->callback);

	if(lua_type(state->L, -1) != LUA_TFUNCTION) {
		pluaL_error(state->L, "%s: coap callback %s does not exist", state->module->file, data->callback);
	}

	plua_network_coap_object(state->L, data);

	lua_newtable(state->L);

	lua_pushstring(state->L, "ver");
	lua_pushnumber(state->L, pkt->ver);
	lua_settable(state->L, -3);

	lua_pushstring(state->L, "t");
	lua_pushnumber(state->L, pkt->t);
	lua_settable(state->L, -3);

	if(pkt->token != NULL && pkt->tkl > 0) {
		lua_pushstring(state->L, "tkl");
		lua_pushnumber(state->L, pkt->tkl);
		lua_settable(state->L, -3);

		lua_pushstring(state->L, "token");
		lua_pushstring(state->L, pkt->token);
		lua_settable(state->L, -3);
	}

	lua_pushstring(state->L, "code");
	lua_pushnumber(state->L, (pkt->code[0]*100)+pkt->code[1]);
	lua_settable(state->L, -3);

	lua_pushstring(state->L, "msgid");
	lua_pushnumber(state->L, (pkt->msgid[0]*100)+pkt->msgid[1]);
	lua_settable(state->L, -3);

	if(pkt->payload != NULL && pkt->payload > 0) {
		lua_pushstring(state->L, "payload");
		lua_pushstring(state->L, pkt->payload);
		lua_settable(state->L, -3);
	}

	lua_pushstring(state->L, "numopt");
	lua_pushnumber(state->L, pkt->numopt);
	lua_settable(state->L, -3);

	if(pkt->numopt > 0) {
		lua_pushstring(state->L, "options");
		lua_newtable(state->L);
		int i = 0;
		for(i=0;i<pkt->numopt;i++) {
			lua_pushnumber(state->L, i);
			lua_newtable(state->L);

			lua_pushstring(state->L, "val");
			lua_pushstring(state->L, (char *)pkt->options[i]->val);
			lua_settable(state->L, -3);

			lua_pushstring(state->L, "num");
			lua_pushnumber(state->L, pkt->options[i]->num);
			lua_settable(state->L, -3);

			lua_settable(state->L, -3);
		}
		lua_settable(state->L, -3);
	}

	char ip[INET6_ADDRSTRLEN] = { 0 };
	int port = -1;
	int numargs = 2;
	switch(addr->sa_family) {
		case AF_INET: {
			struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
			inet_ntop(AF_INET, &(addr_in->sin_addr), (char *)&ip, INET_ADDRSTRLEN);
			port = htons(addr_in->sin_port);
			break;
    }
    case AF_INET6: {
			struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)addr;
			inet_ntop(AF_INET6, &(addr_in6->sin6_addr), (char *)&ip, INET6_ADDRSTRLEN);
			port = htons(addr_in6->sin6_port);
			break;
    }
    default:
			break;
	}

	if(strlen(ip) > 0 && port > 0) {
		numargs = 4;
		lua_pushstring(state->L, ip);
		lua_pushnumber(state->L, port);
	}

	if(numargs == 2) {
		assert(plua_check_stack(state->L, 4, PLUA_TTABLE, PLUA_TFUNCTION, PLUA_TTABLE, PLUA_TTABLE) == 0);
	} else if(numargs == 4) {
		assert(plua_check_stack(state->L, 6, PLUA_TTABLE, PLUA_TFUNCTION, PLUA_TTABLE, PLUA_TTABLE, PLUA_TSTRING, PLUA_TNUMBER) == 0);
	}

	if(plua_pcall(state->L, state->module->file, numargs, 0) == -1) {
		plua_clear_state(state);
		return;
	}

	lua_remove(state->L, 1);
	assert(plua_check_stack(state->L, 0) == 0);
	plua_clear_state(state);

	return;
}

static int plua_network_coap_send(lua_State *L) {
	struct lua_coap_t *coap = (void *)lua_topointer(L, lua_upvalueindex(1));
	struct plua_metatable_t *option = NULL;
	struct plua_metatable_t *data = NULL;

	double ret = 0.0;
	char *str = NULL;
	int high = 0, low = 0;
	char nr[255], *p = nr;

	if(lua_gettop(L) != 1) {
		pluaL_error(L, "coap.send requires 1 arguments, %d given", lua_gettop(L));
	}

	if(coap == NULL) {
		pluaL_error(L, "internal error: coap object not passed");
	}

	char buf[128] = { '\0' };
	char *error = "userdata expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TLIGHTUSERDATA || lua_type(L, -1) == LUA_TTABLE),
		1, buf);

	if(lua_type(L, -1) == LUA_TLIGHTUSERDATA) {
		data = (void *)lua_topointer(L, -1);
	} else if(lua_type(L, -1) == LUA_TTABLE) {
		lua_pushnil(L);
		plua_metatable_init(&data);

		while(lua_next(L, -2) != 0) {
			plua_metatable_parse_set(L, data);
			lua_pop(L, 1);
		}
	}
	lua_remove(L, -1);

	struct coap_packet_t pkt;
	memset(&pkt, 0, sizeof(struct coap_packet_t));
	if(plua_metatable_get_number(data, "ver", &ret) == 0) {
		pkt.ver = (int)ret;
	} else {
		plua_metatable_free(data);
		pluaL_error(L, "coap packet requires a numeric 'ver'");
	}

	if(plua_metatable_get_number(data, "t", &ret) == 0) {
		pkt.t = (int)ret;
	} else {
		plua_metatable_free(data);
		pluaL_error(L, "coap packet requires a numeric 't'");
	}

	struct varcont_t var;
	if(plua_metatable_get(data, "token", &var) > 0) {
		if(var.type_ != LUA_TSTRING) {
			plua_metatable_free(data);
			pluaL_error(L, "coap packet 'token' must be a string");
		}
	}
	if(plua_metatable_get_string(data, "token", &str) == 0) {
		pkt.tkl = strlen(str);

		if((pkt.token = STRDUP(str)) == NULL) {
			OUT_OF_MEMORY
		}
	}

	if(plua_metatable_get(data, "payload", &var) > 0) {
		if(var.type_ != LUA_TSTRING) {
			plua_metatable_free(data);
			pluaL_error(L, "coap packet 'payload' must be a string");
		}
	}
	if(plua_metatable_get_string(data, "payload", &str) == 0) {
		if((pkt.payload = STRDUP(str)) == NULL) {
			OUT_OF_MEMORY
		}
	}

	if(plua_metatable_get(data, "code", &var) > 0) {
		if(var.type_ != LUA_TNUMBER) {
			plua_metatable_free(data);
			pluaL_error(L, "coap packet 'code' must be a number");
		}
	}
	if(plua_metatable_get_number(data, "code", &ret) == 0) {
		if((int)ret < 0 || (int)ret > 705) {
			plua_metatable_free(data);
			pluaL_error(L, "coap packet 'code' %d is invalid", (int)ret);
		}

		high = (int)ret / 100;
		low = (int)ret - (high * 100);

		pkt.code[0] = high;
		pkt.code[1] = low;
	} else {
		plua_metatable_free(data);
		pluaL_error(L, "coap packet requires a numeric 'code'");
	}

	if(plua_metatable_get(data, "msgid", &var) > 0) {
		if(var.type_ != LUA_TNUMBER) {
			plua_metatable_free(data);
			pluaL_error(L, "coap packet 'msgid' must be a number");
		}
	}

	if(plua_metatable_get_number(data, "msgid", &ret) == 0) {
		high = (int)ret / 100;
		low = (int)ret - (high * 100);

		if((int)ret < 0 || (int)ret > 0xFFFF) {
			plua_metatable_free(data);
			pluaL_error(L, "coap packet 'msgid' %d is invalid", (int)ret);
		}

		pkt.msgid[0] = high;
		pkt.msgid[1] = low;
	}

	int i = 0;
	p = nr;
	while(1) {
		snprintf(p, 254, "options.%d", i);
		if(plua_metatable_get_table(data, nr, &option) == -1) {
			break;
		}

		if(plua_metatable_get_number(option, "num", &ret) == -1 ||
		   plua_metatable_get_string(option, "val", &str) == -1) {
			coap_free(&pkt);
			plua_metatable_free(data);
			pluaL_error(L, "coap packet options require a numeric 'num' and string 'val'");
		} else if(str == NULL) {
			break;
		}

		if((pkt.options = REALLOC(pkt.options, sizeof(struct coap_options_t *)*(i+1))) == NULL) {
			OUT_OF_MEMORY
		}
		if((pkt.options[i] = MALLOC(sizeof(struct coap_options_t))) == NULL) {
			OUT_OF_MEMORY
		}
		memset(pkt.options[i], 0, sizeof(struct coap_options_t));

		if((pkt.options[i]->val = (unsigned char *)STRDUP(str)) == NULL) {
			OUT_OF_MEMORY
		}
		pkt.options[i]->len = strlen(str);
		pkt.options[i]->num = ret;
		i++;
	}
	pkt.numopt = i;

	if(i == 0) {
		if(plua_metatable_get(data, "options", &var) > 0) {
			coap_free(&pkt);
			plua_metatable_free(data);
			pluaL_error(L, "coap packet options must be an array type");
		}
	}

	coap->type = SEND;
	coap_send(&pkt, read_cb, coap);
	coap_free(&pkt);

	plua_metatable_free(data);

	lua_pushboolean(L, 1);
	assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

	return 1;
}

static int plua_network_coap_listen(lua_State *L) {
	struct lua_coap_t *coap = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		pluaL_error(L, "coap.listen requires 0 arguments, %d given", lua_gettop(L));
	}

	if(coap == NULL) {
		pluaL_error(L, "internal error: coap object not passed");
	}

	coap->type = LISTEN;
	coap_listen(read_cb, coap);

	lua_pushboolean(L, 1);
	assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

	return 1;
}

static int plua_network_coap_get_callback(lua_State *L) {
	struct lua_coap_t *coap = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		pluaL_error(L, "coap.getCallback requires 0 arguments, %d given", lua_gettop(L));
	}

	if(coap == NULL) {
		pluaL_error(L, "internal error: coap object not passed");
	}

	if(coap->callback != NULL) {
		lua_pushstring(L, coap->callback);
	} else {
		lua_pushnil(L);
	}

	assert(plua_check_stack(L, 1, PLUA_TSTRING | PLUA_TNIL) == 0);

	return 1;
}

static void plua_network_coap_gc(void *ptr) {
	struct lua_coap_t *data = ptr;

	plua_metatable_free(data->table);
	if(data->callback != NULL) {
		FREE(data->callback);
	}
	FREE(data);
}

static void plua_network_coap_object(lua_State *L, struct lua_coap_t *coap) {
	lua_newtable(L);

	lua_pushstring(L, "setCallback");
	lua_pushlightuserdata(L, coap);
	lua_pushcclosure(L, plua_network_coap_set_callback, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getCallback");
	lua_pushlightuserdata(L, coap);
	lua_pushcclosure(L, plua_network_coap_get_callback, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "send");
	lua_pushlightuserdata(L, coap);
	lua_pushcclosure(L, plua_network_coap_send, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "listen");
	lua_pushlightuserdata(L, coap);
	lua_pushcclosure(L, plua_network_coap_listen, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getUserdata");
	lua_pushlightuserdata(L, coap);
	lua_pushcclosure(L, plua_network_coap_get_userdata, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setUserdata");
	lua_pushlightuserdata(L, coap);
	lua_pushcclosure(L, plua_network_coap_set_userdata, 1);
	lua_settable(L, -3);
}

int plua_network_coap(struct lua_State *L) {
	if(lua_gettop(L) != 0) {
		pluaL_error(L, "timer requires 0 arguments, %d given", lua_gettop(L));
		return 0;
	}

	struct lua_state_t *state = plua_get_current_state(L);
	if(state == NULL) {
		return 0;
	}

	struct lua_coap_t *lua_coap = MALLOC(sizeof(struct lua_coap_t));
	if(lua_coap == NULL) {
		OUT_OF_MEMORY
	}
	memset(lua_coap, '\0', sizeof(struct lua_coap_t));

	plua_metatable_init(&lua_coap->table);

	lua_coap->module = state->module;
	lua_coap->L = L;

	plua_gc_reg(NULL, lua_coap, plua_network_coap_gc);

	plua_network_coap_object(L, lua_coap);

	assert(plua_check_stack(L, 1, PLUA_TTABLE) == 0);

	return 1;
}
