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
#include "../../core/mdns.h"
#include "../table.h"
#include "../network.h"

#define LISTEN	0
#define	SEND		1

typedef struct lua_mdns_t {
	PLUA_INTERFACE_FIELDS

	int type;
	char *callback;
} lua_mdns_t;

static void plua_network_mdns_object(lua_State *L, struct lua_mdns_t *mdns);
static void plua_network_mdns_gc(void *ptr);

static int plua_network_mdns_set_userdata(lua_State *L) {
	struct lua_mdns_t *mdns = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 1) {
		pluaL_error(L, "mdns.setUserdata requires 1 argument, %d given", lua_gettop(L));
	}

	if(mdns == NULL) {
		pluaL_error(L, "internal error: mdns object not passed");
	}

	char buf[128] = { '\0' }, *p = buf;
	char *error = "userdata expected, got %s";

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		(lua_type(L, -1) == LUA_TLIGHTUSERDATA || lua_type(L, -1) == LUA_TTABLE),
		1, buf);

	if(lua_type(L, -1) == LUA_TLIGHTUSERDATA) {
		if(mdns->table != (void *)lua_topointer(L, -1)) {
			plua_metatable_free(mdns->table);
		}
		mdns->table = (void *)lua_topointer(L, -1);

		if(mdns->table->ref != NULL) {
			uv_sem_post(mdns->table->ref);
		}

		lua_pushboolean(L, 1);

		assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

		return 1;
	}

	if(lua_type(L, -1) == LUA_TTABLE) {
		lua_pushnil(L);
		while(lua_next(L, -2) != 0) {
			plua_metatable_parse_set(L, mdns->table);
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

static int plua_network_mdns_get_userdata(lua_State *L) {
	struct lua_mdns_t *mdns = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		pluaL_error(L, "mdns.getUserdata requires 0 argument, %d given", lua_gettop(L));
		return 0;
	}

	if(mdns == NULL) {
		pluaL_error(L, "internal error: mdns object not passed");
		return 0;
	}

	push__plua_metatable(L, (struct plua_interface_t *)mdns);

	assert(plua_check_stack(L, 1, PLUA_TTABLE) == 0);

	return 1;
}

static int plua_network_mdns_set_callback(lua_State *L) {
	struct lua_mdns_t *mdns = (void *)lua_topointer(L, lua_upvalueindex(1));
	char *func = NULL;

	if(lua_gettop(L) != 1) {
		pluaL_error(L, "mdns.setCallback requires 1 argument, %d given", lua_gettop(L));
	}

	if(mdns == NULL) {
		pluaL_error(L, "internal error: mdns object not passed");
	}

	if(mdns->module == NULL) {
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
	plua_namespace(mdns->module, p);

	lua_getglobal(L, name);
	if(lua_type(L, -1) == LUA_TNIL) {
		pluaL_error(L, "cannot find %s lua module", mdns->module->name);
	}

	lua_getfield(L, -1, func);
	if(lua_type(L, -1) != LUA_TFUNCTION) {
		pluaL_error(L, "%s: mdns callback %s does not exist", mdns->module->file, func);
	}
	lua_remove(L, -1);
	lua_remove(L, -1);

	if(mdns->callback != NULL) {
		FREE(mdns->callback);
	}
	if((mdns->callback = STRDUP(func)) == NULL) {
		OUT_OF_MEMORY
	}

	lua_pushboolean(L, 1);

	assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

	return 1;
}

static void parse_payload(lua_State *L, struct mdns_packet_t *pkt, int type) {
	struct mdns_payload_t *payload = NULL;
	int x = 0, nr = 0;
	switch(type) {
		/*case 0: {
			nr = pkt->nrqueries;
		} break;*/
		case 1: {
			nr = pkt->nranswers;
		} break;
		case 2: {
			nr = pkt->rr_add;
		} break;
	}

	for(x=0;x<nr;x++) {
		lua_pushnumber(L, x+1);
		lua_newtable(L);
		switch(type) {
			/*case 0: {
				payload = &pkt->queries[x]->payload;
			} break;*/
			case 1: {
				payload = pkt->answers[x];
			} break;
			case 2: {
				payload = pkt->records[x];
			} break;
			default:
			break;
		}
		lua_pushstring(L, "name");
		lua_pushstring(L, payload->name);
		lua_settable(L, -3);

		lua_pushstring(L, "type");
		lua_pushnumber(L, payload->type);
		lua_settable(L, -3);

		switch(payload->type) {
			case 1:
			case 12:
			case 16:
			case 28:
			case 33:
			case 47: {
				lua_pushstring(L, "flush");
				lua_pushnumber(L, payload->flush);
				lua_settable(L, -3);

				lua_pushstring(L, "class");
				lua_pushnumber(L, payload->class);
				lua_settable(L, -3);

				lua_pushstring(L, "ttl");
				lua_pushnumber(L, payload->ttl);
				lua_settable(L, -3);
			} break;
		}

		switch(payload->type) {
			case 1:
			case 12:
			case 16:
			case 28:
			case 33: {
				lua_pushstring(L, "length");
				lua_pushnumber(L, payload->length);
				lua_settable(L, -3);
			} break;
			case 47: {
				lua_pushstring(L, "length");
				lua_pushnumber(L, 0);
				lua_settable(L, -3);
			} break;
		}
		switch(payload->type) {
			case 33: {
				lua_pushstring(L, "priority");
				lua_pushnumber(L, payload->priority);
				lua_settable(L, -3);

				lua_pushstring(L, "weight");
				lua_pushnumber(L, payload->weight);
				lua_settable(L, -3);

				lua_pushstring(L, "port");
				lua_pushnumber(L, payload->port);
				lua_settable(L, -3);
			} break;
		}
		switch(payload->type) {
			case 1: {
				char ip[17];
				memset(&ip, 0, sizeof(ip));

				snprintf((char *)&ip, 17, "%d.%d.%d.%d", payload->data.ip4[0], payload->data.ip4[1], payload->data.ip4[2], payload->data.ip4[3]);

				lua_pushstring(L, "ip");
				lua_pushstring(L, ip);
				lua_settable(L, -3);
			} break;
			case 16: {
				lua_pushstring(L, "options");
				lua_newtable(L);

				int i = 0;
				for(i=0;i<payload->data.text.nrvalues;i++) {
					lua_pushnumber(L, i+1);
					lua_pushstring(L, payload->data.text.values[i]);
					lua_settable(L, -3);
				}
				lua_settable(L, -3);
			} break;
			case 12:
			case 33: {
				lua_pushstring(L, "domain");
				lua_pushstring(L, payload->data.domain);
				lua_settable(L, -3);
			} break;
			case 28: {
				char ip[40];
				memset(&ip, 0, sizeof(ip));

				snprintf((char *)&ip, 40, "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
					payload->data.ip6[0], payload->data.ip6[1], payload->data.ip6[2], payload->data.ip6[3],
					payload->data.ip6[4], payload->data.ip6[5], payload->data.ip6[6], payload->data.ip6[7],
					payload->data.ip6[8], payload->data.ip6[9], payload->data.ip6[10], payload->data.ip6[11],
					payload->data.ip6[12], payload->data.ip6[13], payload->data.ip6[14], payload->data.ip6[15]
				);

				lua_pushstring(L, "ip");
				lua_pushstring(L, ip);
				lua_settable(L, -3);
			} break;
		}
		lua_settable(L, -3);
	}
}

static void read_cb(const struct sockaddr *addr, struct mdns_packet_t *pkt, void *userdata) {
	struct lua_mdns_t *data = userdata;

	char name[255], *p = name;
	memset(name, '\0', 255);

	/*
	 * Only create a new state once the mdns callback is called
	 */
	struct lua_state_t *state = plua_get_free_state();
	state->module = data->module;

	logprintf(LOG_DEBUG, "lua mdns on state #%d", state->idx);

	plua_namespace(state->module, p);

	lua_getglobal(state->L, name);
	if(lua_type(state->L, -1) == LUA_TNIL) {
		pluaL_error(state->L, "cannot find %s lua module", name);
	}

	lua_getfield(state->L, -1, data->callback);

	if(lua_type(state->L, -1) != LUA_TFUNCTION) {
		pluaL_error(state->L, "%s: mdns callback %s does not exist", state->module->file, data->callback);
	}

	plua_network_mdns_object(state->L, data);

	lua_newtable(state->L);

	lua_pushstring(state->L, "id");
	lua_pushnumber(state->L, pkt->id);
	lua_settable(state->L, -3);

	lua_pushstring(state->L, "qr");
	lua_pushnumber(state->L, pkt->qr);
	lua_settable(state->L, -3);

	lua_pushstring(state->L, "opcode");
	lua_pushnumber(state->L, pkt->opcode);
	lua_settable(state->L, -3);

	lua_pushstring(state->L, "aa");
	lua_pushnumber(state->L, pkt->aa);
	lua_settable(state->L, -3);

	lua_pushstring(state->L, "tc");
	lua_pushnumber(state->L, pkt->tc);
	lua_settable(state->L, -3);

	lua_pushstring(state->L, "rd");
	lua_pushnumber(state->L, pkt->rd);
	lua_settable(state->L, -3);

	lua_pushstring(state->L, "ra");
	lua_pushnumber(state->L, pkt->ra);
	lua_settable(state->L, -3);

	lua_pushstring(state->L, "z");
	lua_pushnumber(state->L, pkt->z);
	lua_settable(state->L, -3);

	lua_pushstring(state->L, "ad");
	lua_pushnumber(state->L, pkt->ad);
	lua_settable(state->L, -3);

	lua_pushstring(state->L, "cd");
	lua_pushnumber(state->L, pkt->cd);
	lua_settable(state->L, -3);

	lua_pushstring(state->L, "rcode");
	lua_pushnumber(state->L, pkt->rcode);
	lua_settable(state->L, -3);

	lua_pushstring(state->L, "rr_auth");
	lua_pushnumber(state->L, pkt->rr_auth);
	lua_settable(state->L, -3);

	int x = 0;
	if(pkt->nrqueries > 0) {
		lua_pushstring(state->L, "queries");
		lua_newtable(state->L);
		if(pkt->qr == 0) {
			for(x=0;x<pkt->nrqueries;x++) {
				lua_pushnumber(state->L, x+1);
				lua_newtable(state->L);

				/*
				lua_pushstring(state->L, "name");
				lua_pushstring(state->L, pkt->queries[x]->query.name);
				lua_settable(state->L, -3);

				lua_pushstring(state->L, "qu");
				lua_pushnumber(state->L, pkt->queries[x]->query.qu);
				lua_settable(state->L, -3);

				lua_pushstring(state->L, "class");
				lua_pushnumber(state->L, pkt->queries[x]->query.class);
				lua_settable(state->L, -3);

				lua_pushstring(state->L, "type");
				lua_pushnumber(state->L, pkt->queries[x]->query.type);
				lua_settable(state->L, -3);
				*/

				lua_pushstring(state->L, "name");
				lua_pushstring(state->L, pkt->queries[x]->name);
				lua_settable(state->L, -3);

				lua_pushstring(state->L, "qu");
				lua_pushnumber(state->L, pkt->queries[x]->qu);
				lua_settable(state->L, -3);

				lua_pushstring(state->L, "class");
				lua_pushnumber(state->L, pkt->queries[x]->class);
				lua_settable(state->L, -3);

				lua_pushstring(state->L, "type");
				lua_pushnumber(state->L, pkt->queries[x]->type);
				lua_settable(state->L, -3);

				lua_settable(state->L, -3);
			}
		}/* else {
			parse_payload(state->L, pkt, 0);
		}*/
		lua_settable(state->L, -3);
	}

	if(pkt->nranswers > 0) {
		lua_pushstring(state->L, "answers");
		lua_newtable(state->L);
		parse_payload(state->L, pkt, 1);
		lua_settable(state->L, -3);
	}

	if(pkt->rr_add > 0) {
		lua_pushstring(state->L, "records");
		lua_newtable(state->L);
		parse_payload(state->L, pkt, 2);
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

static int plua_network_mdns_send(lua_State *L) {
	struct lua_mdns_t *mdns = (void *)lua_topointer(L, lua_upvalueindex(1));
	struct plua_metatable_t *data = NULL;

	double ret = 0.0;
	char *str = NULL;
	char nr[255], *p = nr;

	if(lua_gettop(L) != 1) {
		pluaL_error(L, "mdns.send requires 1 arguments, %d given", lua_gettop(L));
	}

	if(mdns == NULL) {
		pluaL_error(L, "internal error: mdns object not passed");
	}

	{
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
	}

	struct mdns_packet_t pkt;
	memset(&pkt, 0, sizeof(struct mdns_packet_t));
	if(plua_metatable_get_number(data, "id", &ret) == 0) {
		pkt.id = (int)ret;
	} else {
		plua_metatable_free(data);
		mdns_free(&pkt);
		pluaL_error(L, "mdns packet requires a numeric 'id'");
	}

	if(plua_metatable_get_number(data, "qr", &ret) == 0) {
		if(pkt.qr < 0 || pkt.qr > 1) {
			plua_metatable_free(data);
			mdns_free(&pkt);
			pluaL_error(L, "mdns packet requires a numeric 'qr'");
		}
		pkt.qr = (int)ret;
	} else {
		plua_metatable_free(data);
		mdns_free(&pkt);
		pluaL_error(L, "mdns packet requires a numeric 'qr'");
	}

	if(plua_metatable_get_number(data, "opcode", &ret) == 0) {
		pkt.opcode = (int)ret;
	} else {
		plua_metatable_free(data);
		mdns_free(&pkt);
		pluaL_error(L, "mdns packet requires a numeric 'opcode'");
	}

	if(plua_metatable_get_number(data, "aa", &ret) == 0) {
		pkt.aa = (int)ret;
	} else {
		plua_metatable_free(data);
		mdns_free(&pkt);
		pluaL_error(L, "mdns packet requires a numeric 'aa'");
	}

	if(plua_metatable_get_number(data, "tc", &ret) == 0) {
		pkt.tc = (int)ret;
	} else {
		plua_metatable_free(data);
		mdns_free(&pkt);
		pluaL_error(L, "mdns packet requires a numeric 'tc'");
	}

	if(plua_metatable_get_number(data, "rd", &ret) == 0) {
		pkt.rd = (int)ret;
	} else {
		plua_metatable_free(data);
		mdns_free(&pkt);
		pluaL_error(L, "mdns packet requires a numeric 'rd'");
	}

	if(plua_metatable_get_number(data, "ra", &ret) == 0) {
		pkt.ra = (int)ret;
	} else {
		plua_metatable_free(data);
		mdns_free(&pkt);
		pluaL_error(L, "mdns packet requires a numeric 'ra'");
	}

	if(plua_metatable_get_number(data, "z", &ret) == 0) {
		pkt.z = (int)ret;
	} else {
		plua_metatable_free(data);
		mdns_free(&pkt);
		pluaL_error(L, "mdns packet requires a numeric 'z'");
	}

	if(plua_metatable_get_number(data, "ad", &ret) == 0) {
		pkt.ad = (int)ret;
	} else {
		plua_metatable_free(data);
		mdns_free(&pkt);
		pluaL_error(L, "mdns packet requires a numeric 'ad'");
	}

	if(plua_metatable_get_number(data, "cd", &ret) == 0) {
		pkt.cd = (int)ret;
	} else {
		plua_metatable_free(data);
		mdns_free(&pkt);
		pluaL_error(L, "mdns packet requires a numeric 'cd'");
	}

	if(plua_metatable_get_number(data, "rcode", &ret) == 0) {
		pkt.rcode = (int)ret;
	} else {
		plua_metatable_free(data);
		mdns_free(&pkt);
		pluaL_error(L, "mdns packet requires a numeric 'rcode'");
	}

	if(plua_metatable_get_number(data, "rr_auth", &ret) == 0) {
		pkt.rr_auth = (int)ret;
	} else {
		plua_metatable_free(data);
		mdns_free(&pkt);
		pluaL_error(L, "mdns packet requires a numeric 'rr_auth'");
	}

	struct varcont_t var;
	struct varcont_t var1;
	struct varcont_t var2;
	if(plua_metatable_get(data, "queries", &var) > 0) {
		if(pkt.qr == 1) {
			plua_metatable_free(data);
			mdns_free(&pkt);
			pluaL_error(L, "mdns packet 'queries' must be matched with qr 0");
		} else {
			if(var.type_ != LUA_TTABLE) {
				plua_metatable_free(data);
				mdns_free(&pkt);
				pluaL_error(L, "mdns packet 'queries' must be a table");
			}
			char key[255] = { '\0' };
			int x = 0;
			pkt.nrqueries = ((struct plua_metatable_t *)var.void_)->nrvar;
			for(x=0;x<pkt.nrqueries;x++) {
				snprintf((char *)&key, 255, "%d", x+1);
				if(plua_metatable_get(var.void_, key, &var1) > 0) {
					if(var1.type_ != LUA_TTABLE) {
						plua_metatable_free(data);
						mdns_free(&pkt);
						pluaL_error(L, "mdns packet 'queries' must be a table");
					} else {
						if((pkt.queries = REALLOC(pkt.queries, sizeof(struct mdns_payload_t *)*(x+1))) == NULL) {
							OUT_OF_MEMORY
						}
						if((pkt.queries[x] = MALLOC(sizeof(struct mdns_payload_t))) == NULL) {
							OUT_OF_MEMORY
						}
						memset(pkt.queries[x], 0, sizeof(struct mdns_payload_t));
						if(plua_metatable_get_number(var1.void_, "type", &ret) == 0) {
							pkt.queries[x]->type = (int)ret;
						}
						if(plua_metatable_get_number(var1.void_, "qu", &ret) == 0) {
							pkt.queries[x]->qu = (int)ret;
						}
						if(plua_metatable_get_number(var1.void_, "class", &ret) == 0) {
							pkt.queries[x]->class = (int)ret;
						}
						if(plua_metatable_get_string(var1.void_, "name", &str) == 0) {
							if((pkt.queries[x]->name = STRDUP(str)) == NULL) {
								OUT_OF_MEMORY
							}
						}
					}
				}
			}
		}
	}

	if(plua_metatable_get(data, "answers", &var) > 0) {
		if(pkt.qr == 0) {
			plua_metatable_free(data);
			mdns_free(&pkt);
			pluaL_error(L, "mdns packet 'answers' must be matched with qr 0");
		} else {
			if(var.type_ != LUA_TTABLE) {
				plua_metatable_free(data);
				mdns_free(&pkt);
				pluaL_error(L, "mdns packet 'answers' must be a table");
			}

			char key[255] = { '\0' };
			int x = 0;
			pkt.nranswers = ((struct plua_metatable_t *)var.void_)->nrvar;
			for(x=0;x<pkt.nranswers;x++) {
				snprintf((char *)&key, 255, "%d", x+1);
				if(plua_metatable_get(var.void_, key, &var1) > 0) {
					if(var1.type_ != LUA_TTABLE) {
						plua_metatable_free(data);
						mdns_free(&pkt);
						pluaL_error(L, "mdns packet 'answers' must be a table");
					} else {
						if((pkt.answers = REALLOC(pkt.answers, sizeof(struct mdns_payload_t *)*(x+1))) == NULL) {
							OUT_OF_MEMORY
						}
						if((pkt.answers[x] = MALLOC(sizeof(struct mdns_payload_t))) == NULL) {
							OUT_OF_MEMORY
						}
						memset(pkt.answers[x], 0, sizeof(struct mdns_payload_t));

						if(plua_metatable_get_number(var1.void_, "type", &ret) == 0) {
							pkt.answers[x]->type = (int)ret;
						}
						if(plua_metatable_get_number(var1.void_, "flush", &ret) == 0) {
							pkt.answers[x]->flush = (int)ret;
						}
						if(plua_metatable_get_number(var1.void_, "ttl", &ret) == 0) {
							pkt.answers[x]->ttl = (int)ret;
						}
						if(plua_metatable_get_number(var1.void_, "class", &ret) == 0) {
							pkt.answers[x]->class = (int)ret;
						}
						if(plua_metatable_get_number(var1.void_, "priority", &ret) == 0) {
							if(pkt.answers[x]->type == 33) {
								pkt.answers[x]->priority = (int)ret;
							} else {
								plua_metatable_free(data);
								mdns_free(&pkt);
								pluaL_error(L, "mdns packet 'port' should be matched with type 33");
							}
						}
						if(plua_metatable_get_number(var1.void_, "weight", &ret) == 0) {
							if(pkt.answers[x]->type == 33) {
								pkt.answers[x]->weight = (int)ret;
							} else {
								plua_metatable_free(data);
								mdns_free(&pkt);
								pluaL_error(L, "mdns packet 'port' should be matched with type 33");
							}
						}
						if(plua_metatable_get_number(var1.void_, "port", &ret) == 0) {
							if(pkt.answers[x]->type == 33) {
								pkt.answers[x]->port = (int)ret;
							} else {
								plua_metatable_free(data);
								mdns_free(&pkt);
								pluaL_error(L, "mdns packet 'port' should be matched with type 33");
							}
						}
						if(plua_metatable_get(var1.void_, "length", &var2) > 0) {
							plua_metatable_free(data);
							mdns_free(&pkt);
							pluaL_error(L, "mdns packet 'answers' -> 'length' is an invalid key");
						}
						if(plua_metatable_get_string(var1.void_, "name", &str) == 0) {
							if((pkt.answers[x]->name = STRDUP(str)) == NULL) {
								OUT_OF_MEMORY
							}
						}
						if(plua_metatable_get_string(var1.void_, "domain", &str) == 0) {
							if(pkt.answers[x]->type == 12 || pkt.answers[x]->type == 33) {
								if((pkt.answers[x]->data.domain = STRDUP(str)) == NULL) {
									OUT_OF_MEMORY
								}
							} else {
								plua_metatable_free(data);
								mdns_free(&pkt);
								pluaL_error(L, "mdns packet 'domain' should be matched with type 12 or 33");
							}
						}
						if(plua_metatable_get_string(var1.void_, "ip", &str) == 0) {
							if(pkt.answers[x]->type == 1) {
								char **array = NULL;
								int n = explode(str, ".", &array), i = 0;
								if(n == 4) {
									for(i=0;i<n;i++) {
										pkt.answers[x]->data.ip4[i] = atoi(array[i]);
									}
								}
								array_free(&array, n);
							} else if(pkt.answers[x]->type == 28) {
								char **array = NULL;
								int n = explode(str, ":", &array), i = 0;
								if(n == 16) {
									for(i=0;i<n;i++) {
										pkt.answers[x]->data.ip6[i] = (int)strtol(array[i], NULL, 16);
									}
								}
								array_free(&array, n);
							} else {
								plua_metatable_free(data);
								mdns_free(&pkt);
								pluaL_error(L, "mdns packet 'ip' should be matched with type 1 or 28");
							}
						}
						if(plua_metatable_get(var1.void_, "options", &var2) > 0) {
							if(pkt.answers[x]->type == 16) {
								struct plua_metatable_t *answers = var2.void_;
								int i = 0, error = 0;

								pkt.answers[x]->data.text.nrvalues = ((struct plua_metatable_t *)var2.void_)->nrvar;

								if((pkt.answers[x]->data.text.values = MALLOC(sizeof(char *)*pkt.answers[x]->data.text.nrvalues)) == NULL) {
									OUT_OF_MEMORY
								}
								for(i=0;i<pkt.answers[x]->data.text.nrvalues;i++) {
									if(answers->table[i].val.type_ == LUA_TNUMBER) {
										error = 1;
										if((pkt.answers[x]->data.text.values[i] = STRDUP("")) == NULL) {
											OUT_OF_MEMORY
										}
									} else if(answers->table[i].val.type_ == LUA_TSTRING) {
										if((pkt.answers[x]->data.text.values[i] = STRDUP(answers->table[i].val.string_)) == NULL) {
											OUT_OF_MEMORY
										}
									}
								}
								if(error == 1) {
									plua_metatable_free(data);
									mdns_free(&pkt);
									pluaL_error(L, "mdns packet record 'options' can only contain strings");
								}
							} else {
								plua_metatable_free(data);
								mdns_free(&pkt);
								pluaL_error(L, "mdns packet 'options' should be matched with type 16");
							}
						}
					}
				}
			}
		}
	}

	if(plua_metatable_get(data, "records", &var) > 0) {
		if(pkt.qr == 0) {
			plua_metatable_free(data);
			mdns_free(&pkt);
			pluaL_error(L, "mdns packet 'records' must be matched with qr 0");
		} else {
			if(var.type_ != LUA_TTABLE) {
				plua_metatable_free(data);
				mdns_free(&pkt);
				pluaL_error(L, "mdns packet 'records' must be a table");
			}

			char key[255] = { '\0' };
			int x = 0;
			pkt.rr_add = ((struct plua_metatable_t *)var.void_)->nrvar;
			for(x=0;x<pkt.rr_add;x++) {
				snprintf((char *)&key, 255, "%d", x+1);
				if(plua_metatable_get(var.void_, key, &var1) > 0) {
					if(var1.type_ != LUA_TTABLE) {
						plua_metatable_free(data);
						mdns_free(&pkt);
						pluaL_error(L, "mdns packet 'records' must be a table");
					} else {
						if((pkt.records = REALLOC(pkt.records, sizeof(struct mdns_payload_t *)*(x+1))) == NULL) {
							OUT_OF_MEMORY
						}
						if((pkt.records[x] = MALLOC(sizeof(struct mdns_payload_t))) == NULL) {
							OUT_OF_MEMORY
						}
						memset(pkt.records[x], 0, sizeof(struct mdns_payload_t));

						if(plua_metatable_get_number(var1.void_, "type", &ret) == 0) {
							pkt.records[x]->type = (int)ret;
						}
						if(plua_metatable_get_number(var1.void_, "flush", &ret) == 0) {
							pkt.records[x]->flush = (int)ret;
						}
						if(plua_metatable_get_number(var1.void_, "ttl", &ret) == 0) {
							pkt.records[x]->ttl = (int)ret;
						}
						if(plua_metatable_get_number(var1.void_, "class", &ret) == 0) {
							pkt.records[x]->class = (int)ret;
						}
						if(plua_metatable_get_number(var1.void_, "priority", &ret) == 0) {
							if(pkt.records[x]->type == 33) {
								pkt.records[x]->priority = (int)ret;
							} else {
								plua_metatable_free(data);
								mdns_free(&pkt);
								pluaL_error(L, "mdns packet 'port' should be matched with type 33");
							}
						}
						if(plua_metatable_get_number(var1.void_, "weight", &ret) == 0) {
							if(pkt.records[x]->type == 33) {
								pkt.records[x]->weight = (int)ret;
							} else {
								plua_metatable_free(data);
								mdns_free(&pkt);
								pluaL_error(L, "mdns packet 'port' should be matched with type 33");
							}
						}
						if(plua_metatable_get_number(var1.void_, "port", &ret) == 0) {
							if(pkt.records[x]->type == 33) {
								pkt.records[x]->port = (int)ret;
							} else {
								plua_metatable_free(data);
								mdns_free(&pkt);
								pluaL_error(L, "mdns packet 'port' should be matched with type 33");
							}
						}
						if(plua_metatable_get(var1.void_, "length", &var2) > 0) {
							plua_metatable_free(data);
							mdns_free(&pkt);
							pluaL_error(L, "mdns packet 'records' -> 'length' is an invalid key");
						}
						if(plua_metatable_get_string(var1.void_, "name", &str) == 0) {
							if((pkt.records[x]->name = STRDUP(str)) == NULL) {
								OUT_OF_MEMORY
							}
						}
						if(plua_metatable_get_string(var1.void_, "domain", &str) == 0) {
							if(pkt.records[x]->type == 12 || pkt.records[x]->type == 33) {
								if((pkt.records[x]->data.domain = STRDUP(str)) == NULL) {
									OUT_OF_MEMORY
								}
							} else {
								plua_metatable_free(data);
								mdns_free(&pkt);
								pluaL_error(L, "mdns packet 'domain' should be matched with type 12 or 33");
							}
						}
						if(plua_metatable_get_string(var1.void_, "ip", &str) == 0) {
							if(pkt.records[x]->type == 1) {
								char **array = NULL;
								int n = explode(str, ".", &array), i = 0;
								if(n == 4) {
									for(i=0;i<n;i++) {
										pkt.records[x]->data.ip4[i] = atoi(array[i]);
									}
								}
								array_free(&array, n);
							} else if(pkt.records[x]->type == 28) {
								char **array = NULL;
								int n = explode(str, ":", &array), i = 0;
								if(n == 16) {
									for(i=0;i<n;i++) {
										pkt.records[x]->data.ip6[i] = (int)strtol(array[i], NULL, 16);
									}
								}
								array_free(&array, n);
							} else {
								plua_metatable_free(data);
								mdns_free(&pkt);
								pluaL_error(L, "mdns packet 'ip' should be matched with type 1 or 28");
							}
						}
						if(plua_metatable_get(var1.void_, "options", &var2) > 0) {
							if(pkt.records[x]->type == 16) {
								struct plua_metatable_t *records = var2.void_;
								int i = 0, error = 0;

								pkt.records[x]->data.text.nrvalues = ((struct plua_metatable_t *)var2.void_)->nrvar;

								if((pkt.records[x]->data.text.values = MALLOC(sizeof(char *)*pkt.records[x]->data.text.nrvalues)) == NULL) {
									OUT_OF_MEMORY
								}
								for(i=0;i<pkt.records[x]->data.text.nrvalues;i++) {
									if(records->table[i].val.type_ == LUA_TNUMBER) {
										error = 1;
										if((pkt.records[x]->data.text.values[i] = STRDUP("")) == NULL) {
											OUT_OF_MEMORY
										}
									} else if(records->table[i].val.type_ == LUA_TSTRING) {
										if((pkt.records[x]->data.text.values[i] = STRDUP(records->table[i].val.string_)) == NULL) {
											OUT_OF_MEMORY
										}
									}
								}
								if(error == 1) {
									plua_metatable_free(data);
									mdns_free(&pkt);
									pluaL_error(L, "mdns packet record 'options' can only contain strings");
								}
							} else {
								plua_metatable_free(data);
								mdns_free(&pkt);
								pluaL_error(L, "mdns packet 'options' should be matched with type 16");
							}
						}
					}
				}
			}
		}
	}

	unsigned int len = 0;
	unsigned char *buf = mdns_encode(&pkt, &len);
	struct mdns_packet_t pkt1;
	memset(&pkt1, 0, sizeof(struct mdns_packet_t));

	if(mdns_decode(&pkt1, buf, len) == 0) {
		mdns->type = SEND;
		mdns_send(&pkt1, read_cb, mdns);
		mdns_free(&pkt1);
	} else {
		FREE(buf);
		plua_metatable_free(data);
		mdns_free(&pkt);
		pluaL_error(L, "mdns packet failed to parse");
	}
	mdns_free(&pkt);
	FREE(buf);

	plua_metatable_free(data);

	lua_pushboolean(L, 1);
	assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

	return 1;
}

static int plua_network_mdns_listen(lua_State *L) {
	struct lua_mdns_t *mdns = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		pluaL_error(L, "mdns.listen requires 0 arguments, %d given", lua_gettop(L));
	}

	if(mdns == NULL) {
		pluaL_error(L, "internal error: mdns object not passed");
	}

	mdns->type = LISTEN;
	mdns_listen(read_cb, mdns);

	lua_pushboolean(L, 1);
	assert(plua_check_stack(L, 1, PLUA_TBOOLEAN) == 0);

	return 1;
}

static int plua_network_mdns_get_callback(lua_State *L) {
	struct lua_mdns_t *mdns = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(lua_gettop(L) != 0) {
		pluaL_error(L, "mdns.getCallback requires 0 arguments, %d given", lua_gettop(L));
	}

	if(mdns == NULL) {
		pluaL_error(L, "internal error: mdns object not passed");
	}

	if(mdns->callback != NULL) {
		lua_pushstring(L, mdns->callback);
	} else {
		lua_pushnil(L);
	}

	assert(plua_check_stack(L, 1, PLUA_TSTRING | PLUA_TNIL) == 0);

	return 1;
}

static void plua_network_mdns_gc(void *ptr) {
	struct lua_mdns_t *data = ptr;

	plua_metatable_free(data->table);
	if(data->callback != NULL) {
		FREE(data->callback);
	}
	FREE(data);
}

static void plua_network_mdns_object(lua_State *L, struct lua_mdns_t *mdns) {
	lua_newtable(L);

	lua_pushstring(L, "setCallback");
	lua_pushlightuserdata(L, mdns);
	lua_pushcclosure(L, plua_network_mdns_set_callback, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getCallback");
	lua_pushlightuserdata(L, mdns);
	lua_pushcclosure(L, plua_network_mdns_get_callback, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "send");
	lua_pushlightuserdata(L, mdns);
	lua_pushcclosure(L, plua_network_mdns_send, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "listen");
	lua_pushlightuserdata(L, mdns);
	lua_pushcclosure(L, plua_network_mdns_listen, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "getUserdata");
	lua_pushlightuserdata(L, mdns);
	lua_pushcclosure(L, plua_network_mdns_get_userdata, 1);
	lua_settable(L, -3);

	lua_pushstring(L, "setUserdata");
	lua_pushlightuserdata(L, mdns);
	lua_pushcclosure(L, plua_network_mdns_set_userdata, 1);
	lua_settable(L, -3);
}

int plua_network_mdns(struct lua_State *L) {
	if(lua_gettop(L) != 0) {
		pluaL_error(L, "timer requires 0 arguments, %d given", lua_gettop(L));
		return 0;
	}

	struct lua_state_t *state = plua_get_current_state(L);
	if(state == NULL) {
		return 0;
	}

	struct lua_mdns_t *lua_mdns = MALLOC(sizeof(struct lua_mdns_t));
	if(lua_mdns == NULL) {
		OUT_OF_MEMORY
	}
	memset(lua_mdns, '\0', sizeof(struct lua_mdns_t));

	plua_metatable_init(&lua_mdns->table);

	lua_mdns->module = state->module;
	lua_mdns->L = L;

	plua_gc_reg(NULL, lua_mdns, plua_network_mdns_gc);

	plua_network_mdns_object(L, lua_mdns);

	assert(plua_check_stack(L, 1, PLUA_TTABLE) == 0);

	return 1;
}
