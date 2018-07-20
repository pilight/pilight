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
	#include <libgen.h>
	#include <dirent.h>
	#include <unistd.h>
#endif

#include "lua.h"
#include "lualibrary.h"

#include "../core/log.h"
#include "../core/json.h"
#include "../core/mem.h"
#include "../core/common.h"

#ifdef PILIGHT_UNITTEST
static struct info_t {
	char module[255];
	char function[255];
} info = {
	{ 0 }, { 0 }
};

struct coverage_t {
	char *file;
	int *lstat;
	int *fstat;
	char **functions;
	char *module;
	int maxline;

	int nrlstat;
	int nrfstat;
	int nrfunctions;
} **coverage = NULL;
static int nrcoverage = 0;

static const char *covfile = NULL;
#endif

static int init = 0;
/*
 * Last state is a global state for global
 * garbage collection on pilight shutdown.
 */
static struct lua_state_t lua_state[NRLUASTATES+1];
static struct plua_module_t *modules = NULL;

/* LCOV_EXCL_START */
void plua_stack_dump(lua_State *L) {
	int i = 0;
	int top = lua_gettop(L);
	for(i = 1; i <= top; i++) {  /* repeat for each level */
		int t = lua_type(L, i);
		switch(t) {
			case LUA_TSTRING:  /* strings */
				printf("%d: '%s'", i, lua_tostring(L, i));
			break;
			case LUA_TBOOLEAN:  /* booleans */
				printf("%d: %s", i, lua_toboolean(L, i) ? "true" : "false");
			break;
			case LUA_TNUMBER:  /* numbers */
				printf("%d: %g", i, lua_tonumber(L, i));
			break;
			default:  /* other values */
				printf("%d: %s", i, lua_typename(L, t));
			break;
		}
		printf("  ");  /* put a separator */
	}
	printf("\n");  /* end the listing */
}
/* LCOV_EXCL_STOP */

void plua_metatable_free(struct plua_metatable_t *table) {
	int x = 0;
	for(x=0;x<table->nrvar;x++) {
		if(table->table[x].val.type_ == LUA_TSTRING) {
			FREE(table->table[x].val.string_);
		}
		if(table->table[x].key.type_ == LUA_TSTRING) {
			FREE(table->table[x].key.string_);
		}
		if(table->table[x].val.type_ == LUA_TTABLE) {
			plua_metatable_free(table->table[x].val.void_);
		}
	}
	if(table->nrvar > 0) {
		FREE(table->table);
	}
	FREE(table);
}

int plua_metatable_call(lua_State *L) {
	struct plua_metatable_t *node = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(node == NULL) {
		logprintf(LOG_ERR, "internal error: table object not passed");
		return 0;
	}

	lua_pushlightuserdata(L, node);

	return 1;
}

void plua_metatable_clone(struct plua_metatable_t **src, struct plua_metatable_t **dst) {
	int i = 0;
	struct plua_metatable_t *a = *src;

	if((*dst) != NULL) {
		plua_metatable_free((*dst));
	}
	if(((*dst) = MALLOC(sizeof(struct plua_metatable_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset((*dst), 0, sizeof(struct plua_metatable_t));
	if(((*dst)->table = MALLOC(sizeof(*a->table)*(a->nrvar))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset((*dst)->table, 0, sizeof(*a->table)*(a->nrvar));
	for(i=0;i<a->nrvar;i++) {
		(*dst)->table[i].key.type_ = a->table[i].key.type_;
		(*dst)->table[i].val.type_ = a->table[i].val.type_;

		if(a->table[i].key.type_ == LUA_TSTRING) {
			if(((*dst)->table[i].key.string_ = STRDUP(a->table[i].key.string_)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
		}
		if(a->table[i].key.type_ == LUA_TNUMBER) {
			(*dst)->table[i].key.number_ = a->table[i].key.number_;
		}

		if(a->table[i].val.type_ == LUA_TSTRING) {
			if(((*dst)->table[i].val.string_ = STRDUP(a->table[i].val.string_)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
		}
		if(a->table[i].val.type_ == LUA_TNUMBER) {
			(*dst)->table[i].val.number_ = a->table[i].val.number_;
		}
	}
	(*dst)->nrvar = a->nrvar;
}

int plua_metatable_pairs(lua_State *L) {
	logprintf(LOG_NOTICE, "pilight lua metatables do not support pairs");
  return 0;
}

int plua_metatable_next(lua_State *L) {
	logprintf(LOG_NOTICE, "pilight lua metatables do not support next");
  return 0;
}

int plua_metatable_get(lua_State *L) {
	struct plua_metatable_t *node = (void *)lua_topointer(L, lua_upvalueindex(1));
	char buf[128] = { '\0' }, *p = buf;
	char *error = "string or number expected, got %s";
	int x = 0, match = 0;

	if(node == NULL) {
		logprintf(LOG_ERR, "internal error: table object not passed");
		return 0;
	}

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		((lua_type(L, -1) == LUA_TSTRING) || (lua_type(L, -1) == LUA_TNUMBER)),
		1, buf);

	for(x=0;x<node->nrvar;x++) {
		match = 0;
		switch(lua_type(L, -1)) {
			case LUA_TNUMBER: {
				if(node->table[x].key.type_ == LUA_TNUMBER &&
					node->table[x].key.number_ == (int)lua_tonumber(L, -1)) {
					match = 1;
				}
			} break;
			case LUA_TSTRING: {
				if(node->table[x].key.type_ == LUA_TSTRING &&
				 strcmp(node->table[x].key.string_, lua_tostring(L, -1)) == 0) {
					match = 1;
				}
			}
		}
		if(match == 1) {
			switch(node->table[x].val.type_) {
				case LUA_TNUMBER: {
					lua_pushnumber(L, node->table[x].val.number_);
				} break;
				case LUA_TSTRING: {
					lua_pushstring(L, node->table[x].val.string_);
				} break;
				case LUA_TTABLE: {
					lua_newtable(L);
					lua_newtable(L);

					lua_pushstring(L, "__index");
					lua_pushlightuserdata(L, node->table[x].val.void_);
					lua_pushcclosure(L, plua_metatable_get, 1);
					lua_settable(L, -3);

					lua_pushstring(L, "__newindex");
					lua_pushlightuserdata(L, node->table[x].val.void_);
					lua_pushcclosure(L, plua_metatable_set, 1);
					lua_settable(L, -3);

					lua_pushstring(L, "__gc");
					lua_pushlightuserdata(L, node->table[x].val.void_);
					lua_pushcclosure(L, plua_metatable_gc, 1);
					lua_settable(L, -3);

					lua_pushstring(L, "__pairs");
					lua_pushlightuserdata(L, node->table[x].val.void_);
					lua_pushcclosure(L, plua_metatable_pairs, 1);
					lua_settable(L, -3);

					lua_pushstring(L, "__next");
					lua_pushlightuserdata(L, node->table[x].val.void_);
					lua_pushcclosure(L, plua_metatable_next, 1);
					lua_settable(L, -3);

					lua_pushstring(L, "__call");
					lua_pushlightuserdata(L, node->table[x].val.void_);
					lua_pushcclosure(L, plua_metatable_call, 1);
					lua_settable(L, -3);

					lua_setmetatable(L, -2);
				} break;
			}
			return 1;
		}
	}
	lua_pushnil(L);

	return 0;
}

void plua_metatable_parse_set(lua_State *L, void *data) {
	struct plua_metatable_t *node = data;
	char buf[128] = { '\0' }, *p = buf;
	char *error = "string or number expected, got %s";
	int match = 0, x = 0;

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		((lua_type(L, -1) == LUA_TSTRING) || (lua_type(L, -1) == LUA_TNUMBER) ||
		(lua_type(L, -1) == LUA_TNIL) || (lua_type(L, -1) == LUA_TBOOLEAN) || (lua_type(L, -1) == LUA_TTABLE)),
		1, buf);

	sprintf(p, error, lua_typename(L, lua_type(L, -1)));

	luaL_argcheck(L,
		((lua_type(L, -2) == LUA_TSTRING) || (lua_type(L, -2) == LUA_TNUMBER) || (lua_type(L, -1) == LUA_TBOOLEAN)),
		1, buf);

	for(x=0;x<node->nrvar;x++) {
		switch(lua_type(L, -2)) {
			case LUA_TNUMBER: {
				if(node->table[x].key.type_ == LUA_TNUMBER &&
					node->table[x].key.number_ == (int)lua_tonumber(L, -2)) {
					match = 1;
				}
			} break;
			case LUA_TSTRING: {
				if(node->table[x].key.type_ == LUA_TSTRING &&
					strcmp(node->table[x].key.string_, lua_tostring(L, -2)) == 0) {
					match = 1;
				}
			} break;
		}
		if(match == 1) {
			switch(lua_type(L, -1)) {
				case LUA_TNUMBER: {
					if(node->table[x].val.type_ == LUA_TSTRING) {
						FREE(node->table[x].val.string_);
					}
					if(node->table[x].val.type_ == LUA_TTABLE) {
						plua_metatable_free(node->table[x].val.void_);
					}
					node->table[x].val.number_ = lua_tonumber(L, -1);
					node->table[x].val.type_ = LUA_TNUMBER;
				} break;
				case LUA_TSTRING: {
					if(node->table[x].val.type_ == LUA_TSTRING) {
						FREE(node->table[x].val.string_);
					}
					if(node->table[x].val.type_ == LUA_TTABLE) {
						plua_metatable_free(node->table[x].val.void_);
					}
					if((node->table[x].val.string_ = STRDUP((char *)lua_tostring(L, -1))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}
					node->table[x].val.type_ = LUA_TSTRING;
				} break;
				case LUA_TTABLE: {
					if(node->table[x].val.type_ == LUA_TSTRING) {
						FREE(node->table[x].val.string_);
					}
					if(node->table[x].val.type_ == LUA_TTABLE) {
						plua_metatable_free(node->table[x].val.void_);
					}
					if((node->table[x].val.void_ = MALLOC(sizeof(struct plua_metatable_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}
					memset(node->table[x].val.void_, 0, sizeof(struct plua_metatable_t));
					node->table[x].val.type_ = LUA_TTABLE;
					lua_pushnil(L);
					while(lua_next(L, -2) != 0) {
						plua_metatable_parse_set(L, node->table[x].val.void_);
						lua_pop(L, 1);
					}
				} break;
				/*
				 * Remove key
				 */
				case LUA_TNIL: {
					int i = 0;
					match = 0;
					if(node->table[x].key.type_ == LUA_TSTRING) {
						FREE(node->table[x].key.string_);
					}
					if(node->table[x].val.type_ == LUA_TSTRING) {
						FREE(node->table[x].val.string_);
					}
					if(node->table[x].val.type_ == LUA_TTABLE) {
						plua_metatable_free(node->table[x].val.void_);
						FREE(node->table);
					} else {
						for(i=x;i<node->nrvar-1;i++) {
							switch(node->table[i+1].val.type_) {
								case LUA_TNUMBER: {
									node->table[i].val.number_ = node->table[i+1].val.number_;
									node->table[i].val.type_ = node->table[i+1].val.type_;
								} break;
								case LUA_TSTRING: {
									node->table[i].val.string_ = node->table[i+1].val.string_;
									node->table[i].val.type_ = node->table[i+1].val.type_;
								} break;
								case LUA_TTABLE: {
									node->table[i].val.void_ = node->table[i+1].val.void_;
									node->table[i].val.type_ = node->table[i+1].val.type_;
								} break;
							}
							switch(node->table[i+1].key.type_) {
								case LUA_TNUMBER: {
									node->table[i].key.number_ = node->table[i+1].key.number_;
									node->table[i].key.type_ = node->table[i+1].key.type_;
								} break;
								case LUA_TSTRING: {
									node->table[i].key.string_ = node->table[i+1].key.string_;
									node->table[i].key.type_ = node->table[i+1].key.type_;
								}
							}
						}
					}
					node->nrvar--;
				} break;
			}
			break;
		}
	}

	if(node != NULL) {
		if(match == 0 && lua_type(L, -1) != LUA_TNIL) {
			int idx = node->nrvar;
			if((node->table = REALLOC(node->table, sizeof(*node->table)*(idx+1))) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			switch(lua_type(L, -1)) {
				case LUA_TNUMBER: {
					node->table[idx].val.number_ = lua_tonumber(L, -1);
					node->table[idx].val.type_ = LUA_TNUMBER;
				} break;
				case LUA_TSTRING: {
					if((node->table[idx].val.string_ = STRDUP((char *)lua_tostring(L, -1))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					};
					node->table[idx].val.type_ = LUA_TSTRING;
				} break;
				case LUA_TTABLE: {
					if((node->table[idx].val.void_ = MALLOC(sizeof(struct plua_metatable_t))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}
					memset(node->table[idx].val.void_, 0, sizeof(struct plua_metatable_t));
					node->table[idx].val.type_ = LUA_TTABLE;
					lua_pushnil(L);
					while(lua_next(L, -2) != 0) {
						plua_metatable_parse_set(L, node->table[x].val.void_);
						lua_pop(L, 1);
					}
				} break;
			}
			switch(lua_type(L, -2)) {
				case LUA_TNUMBER: {
					node->table[idx].key.number_ = lua_tonumber(L, -2);
					node->table[idx].key.type_ = LUA_TNUMBER;
				} break;
				case LUA_TSTRING: {
					if((node->table[idx].key.string_ = STRDUP((char *)lua_tostring(L, -2))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					};
					node->table[idx].key.type_ = LUA_TSTRING;
				} break;
			}
			node->nrvar++;
		}
	}
}

int plua_metatable_set(lua_State *L) {
	struct plua_metatable_t *node = (void *)lua_topointer(L, lua_upvalueindex(1));

	if(node == NULL) {
		logprintf(LOG_ERR, "internal error: table object not passed");
		return 0;
	}

	plua_metatable_parse_set(L, node);

	return 1;
}

int plua_metatable_gc(lua_State *L){
	return 1;
}

struct lua_state_t *plua_get_free_state(void) {
	int i = 0;
	for(i=0;i<NRLUASTATES;i++) {
		if(uv_mutex_trylock(&lua_state[i].lock) == 0) {
			return &lua_state[i];
		}
	}
	return NULL;
}

struct lua_state_t *plua_get_current_state(lua_State *L) {
	int i = 0;
	for(i=0;i<NRLUASTATES;i++) {
		if(lua_state[i].L == L) {
			return &lua_state[i];
		}
	}
	return NULL;
}

void plua_clear_state(struct lua_state_t *state) {
	int i = 0;
	for(i=0;i<state->gc.nr;i++) {
		if(state->gc.list[i]->free == 0) {
			state->gc.list[i]->callback(state->gc.list[i]->ptr);
		}
		FREE(state->gc.list[i]);
	}
	if(state->gc.size > 0) {
		FREE(state->gc.list);
	}
	state->gc.nr = 0;
	state->gc.size = 0;

	assert(lua_gettop(state->L) == 0);
	uv_mutex_unlock(&state->lock);
}

static int plua_get_table_string_by_key(struct lua_State *L, const char *key, const char **ret) {
	/*
	 * Push the key we want to retrieve on the stack
	 *
	 * stack now contains: -1 => key -2 => table
	 */
	lua_pushstring(L, key);

	if(lua_type(L, -2) != LUA_TTABLE) {
		/*
		 * Remove the key from the stack again
		 *
		 * stack now contains: -1 => table
		 */
		lua_pop(L, 1);
		return 0;
	}

	/*
	 * Replace the key at -1 with it value in table -2
	 *
	 * stack now contains: -1 => value -2 => table
	 */
	lua_gettable(L, -2);

	/*
	 * Check if the first element is a number
	 */
	if(lua_type(L, -1) != LUA_TSTRING) {
		/*
		 * Remove the value from the stack again
		 *
		 * stack now contains: -1 => table
		 */
		lua_pop(L, 1);
		return 0;
	}

	*ret = lua_tostring(L, -1);

	/*
	 * stack now contains: -1 => table
	 */
	lua_pop(L, 1);
	return 1;
}

// static int plua_get_table_double_by_key(struct lua_State *L, const char *key, double *ret) {
	// /*
	 // * Push the key we want to retrieve on the stack
	 // *
	 // * stack now contains: -1 => key -2 => table
	 // */
	// lua_pushstring(L, key);

	// /*
	 // * Replace the key at -1 with it value in table -2
	 // *
	 // * stack now contains: -1 => value -2 => table
	 // */
	// if(lua_istable(L, -2) == 0) {
		// /*
		 // * Remove the key from the stack again
		 // *
		 // * stack now contains: -1 => table
		 // */
		// lua_pop(L, 1);
		// return 0;
	// }
	// /*
	 // * Replace the key at -1 with it value in table -2
	 // *
	 // * stack now contains: -1 => value -2 => table
	 // */
	// lua_gettable(L, -2);

	// /*
	 // * Check if the first element is a number
	 // */
	// if(lua_isnumber(L, -1) == 0) {
		// /*
		 // * Remove the value from the stack again
		 // *
		 // * stack now contains: -1 => table
		 // */
		// lua_pop(L, 1);
		// return 0;
	// }

	// *ret = lua_tonumber(L, -1);

	// /*
	 // * stack now contains: -1 => table
	 // */
	// lua_pop(L, 1);

	// return 1;
// }

static int plua_table_has_keys(lua_State *L, char **keys, int number) {
	if(lua_type(L, -1) != LUA_TTABLE) {
		return 0;
	}

	int i = 0;
	/*
	 * Push another reference to the table on top of the stack (so we know
	 * where it is), and this function can work for negative, positive and
	 * pseudo indices.
	 *
	 * stack now contains: -1 => table -2 => table
	 */
	lua_pushvalue(L, -1);

	/*
	 * stack now contains: -1 => nil -2 => table -3 => table
	 */
	lua_pushnil(L);

	int match = 0, nrkeys = 0;
	while(lua_next(L, -2)) {
		nrkeys++;

		/*
		 * stack now contains: -1 => value -2 => key -3 => table
		 *
		 * copy the key so that lua_tostring does not modify the original
		 *
		 * stack now contains: -1 => key -2 => value; -3 => key -4 => table -5 => table
		 */
		lua_pushvalue(L, -2);

		const char *k = lua_tostring(L, -1); // key

		for(i=0;i<number;i++) {
			if(strcmp(keys[i], k) == 0) {
				match++;
				break;
			}
		}

		/*
		 * pop value + copy of key, leaving original key
		 *
		 * stack now contains: -1 => key -2 => table -3 => table
		 */
		lua_pop(L, 2);
	}
	/*
	 * After the last lua_next call stack now contains:
	 * -1 => table -2 => table
	 */
	if(match != number || nrkeys != number) {
		lua_pop(L, 1);
		return 0;
	}

	/*
	 * Remove duplicated table from stack
	 *
	 * stack now contains -1 => table
	 */
	lua_pop(L, 1);

	return 1;
}

static int plua_module_init(struct lua_State *L, char *file, struct plua_module_t *mod) {
	/*
	 * Function info is at top of stack
	 *
	 * stack now contains -1 => function
	 */
#if LUA_VERSION_NUM <= 502
	lua_getfield(L, -1, "info");
	if(lua_type(L, -1) != LUA_TFUNCTION) {
#else
	if(lua_getfield(L, -1, "info") == 0) {
#endif
		logprintf(LOG_ERR, "%s: info function missing", file);
		return 0;
	}

	char *type = NULL;
	switch(mod->type) {
		case UNITTEST: {
			type = STRDUP("unittest");
		} break;
		case FUNCTION: {
			type = STRDUP("event function");
		} break;
		case OPERATOR: {
			type = STRDUP("event operator");
		} break;
		case ACTION: {
			type = STRDUP("event action");
		} break;
		case PROTOCOL: {
			type = STRDUP("protocol");
		} break;
		case STORAGE: {
			type = STRDUP("storage");
		} break;
	}
	if(type == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	/*
	 * Returned table (first argument) is at top of stack
	 *
	 * stack now contains -1 => function
	 */
	if(lua_pcall(L, 0, 1, 0) == LUA_ERRRUN) {
		if(lua_type(L, -1) != LUA_TSTRING) {
			logprintf(LOG_ERR, "%s", lua_tostring(L,  -1));
			lua_pop(L, 1);
			return 0;
		}
	}

	if(lua_type(L, -1) != LUA_TTABLE) {
		logprintf(LOG_ERR, "%s: the info function returned %s, table expected", file, lua_typename(L, lua_type(L, -1)));
		FREE(type);
		return 0;
	}

	char *keys[12] = {"name", "version", "reqversion", "reqcommit"};
	if(plua_table_has_keys(L, keys, 4) == 0) {
		logprintf(LOG_ERR, "%s: the info table has invalid keys", file);
		FREE(type);
		return 0;
	}

	const char *name = NULL, *version = NULL, *reqversion = NULL, *reqcommit = NULL;
	if(plua_get_table_string_by_key(L, "name", &name) == 0) {
		logprintf(LOG_ERR, "%s: the info table 'name' key is missing or invalid", file);
		FREE(type);
		return 0;
	}

	if(plua_get_table_string_by_key(L, "version", &version) == 0) {
		logprintf(LOG_ERR, "%s: the info table 'version' key is missing or invalid", file);
		FREE(type);
		return 0;
	}

	if(plua_get_table_string_by_key(L, "reqversion", &reqversion) == 0) {
		logprintf(LOG_ERR, "%s: the info table 'reqversion' key is missing or invalid", file);
		FREE(type);
		return 0;
	}
	if(plua_get_table_string_by_key(L, "reqcommit", &reqcommit) == 0) {
		logprintf(LOG_ERR, "%s: the info table 'reqcommit' key is missing or invalid", file);
		FREE(type);
		return 0;
	}

	strcpy(mod->name, name);
	strcpy(mod->version, version);
	strcpy(mod->reqversion, reqversion);
	strcpy(mod->reqcommit, reqcommit);

	char pilight_version[strlen(PILIGHT_VERSION)+1];
	char pilight_commit[3], *v = (char *)reqversion, *r = (char *)reqcommit;
	int valid = 1, check = 1;
	strcpy(pilight_version, PILIGHT_VERSION);

	if((check = vercmp(v, pilight_version)) > 0) {
		valid = 0;
	}

	if(check == 0 && strlen(mod->reqcommit) > 0) {
		sscanf(HASH, "v%*[0-9].%*[0-9]-%[0-9]-%*[0-9a-zA-Z\n\r]", pilight_commit);

		if(strlen(pilight_commit) > 0 && (vercmp(r, pilight_commit)) > 0) {
			valid = 0;
		}
	}
	if(valid == 1) {
		logprintf(LOG_DEBUG, "loaded %s %s v%s", type, file, version);
	} else {
		if(strlen(mod->reqcommit) > 0) {
			logprintf(LOG_ERR, "%s %s requires at least pilight v%s (commit %s)", type, file, mod->reqversion, mod->reqcommit);
		} else {
			logprintf(LOG_ERR, "%s %s requires at least pilight v%s", type, file, mod->reqversion);
		}
		/*
		 * Pop function from stack
		 *
		 * The stack now contains: nothing
		 */
		lua_pop(L, 1);
		FREE(type);
		return 0;
	}

	/*
	 * Pop function from stack
	 *
	 * The stack now contains: nothing
	 */
	lua_pop(L, 1);
	FREE(type);
	return 1;
}

static int plua_writer(lua_State *L, const void* p, size_t sz, void* ud) {
	struct plua_module_t *module = ud;
	if((module->bytecode = REALLOC(module->bytecode, module->size+sz)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memcpy(&module->bytecode[module->size], p, sz);
	module->size += sz;
	return 0;
}

void plua_module_load(char *file, int type) {
	struct plua_module_t *module = MALLOC(sizeof(struct plua_module_t));
	lua_State *L = lua_state[0].L;
	char name[255] = { '\0' }, *p = name;
	int i = 0;
	if(module == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(module, 0, sizeof(struct plua_module_t));

	if(luaL_loadfile(L, file) != 0) {
		logprintf(LOG_ERR, "cannot load lua file: %s", file);
		lua_pop(L, 1);
		FREE(module);
		return;
	}
	strcpy(module->file, file);
	if(lua_dump(L, plua_writer, module) != 0) {
		logprintf(LOG_ERR, "cannot dump lua file: %s", file);
		lua_pop(L, 1);
		FREE(module->bytecode);
		FREE(module);
		return;
	}

	lua_pcall(L, 0, LUA_MULTRET, 0);

	if(lua_type(L, -1) != LUA_TTABLE) {
		logprintf(LOG_ERR, "%s: does not return a table", file);
		lua_pop(L, 1);
		FREE(module->bytecode);
		FREE(module);
		return;
	}

	module->type = type;
	strcpy(module->file, file);
	if(plua_module_init(L, file, module) != 0) {
		memset(p, '\0', sizeof(name));
		switch(module->type) {
			case UNITTEST:
				sprintf(p, "unittest.%s", module->name);
			break;
			case OPERATOR:
				sprintf(p, "operator.%s", module->name);
			break;
			case FUNCTION:
				sprintf(p, "function.%s", module->name);
			break;
			case ACTION:
				sprintf(p, "action.%s", module->name);
			break;
			case PROTOCOL:
				sprintf(p, "protocol.%s", module->name);
			break;
			case STORAGE:
				sprintf(p, "storage.%s", module->name);
			break;
		}

		module->next = modules;
		modules = module;
	} else {
		FREE(module->bytecode);
		FREE(module);
		return;
	}
	lua_setglobal(L, name);

	for(i=1;i<NRLUASTATES;i++) {
		L = lua_state[i].L;
		luaL_loadbuffer(L, module->bytecode, module->size, module->name);
		lua_pcall(L, 0, LUA_MULTRET, 0);
		assert(lua_type(L, -1) == LUA_TTABLE);
		lua_setglobal(L, name);
	}

	lua_getglobal(L, name);
	if(lua_type(L, -1) == LUA_TNIL) {
		lua_pop(L, -1);
		return;
	}
	if(lua_type(L, -1) != LUA_TTABLE) {
		lua_pop(L, -1);
		return;
	}

#ifdef PILIGHT_UNITTEST
	if(module->type != UNITTEST) {
		if(lua_type(L, -1) == LUA_TTABLE) {
			lua_pushvalue(L, -1);
			lua_pushnil(L);

			while(lua_next(L, -2)) {
				lua_pushvalue(L, -2);

				strcpy(info.module, name);
				strcpy(info.function, lua_tostring(L, -1));
				lua_pop(L, 1);

				if(lua_type(L, -1) == LUA_TFUNCTION) {
					lua_pcall(L, 0, LUA_MULTRET, 0);
				}

				while(lua_gettop(L) > 3) {
					lua_remove(L, -1);
				}
			}

			lua_pop(L, 1);
		}
		lua_pop(L, -1);
	}

	memset(&info.module, 0, 255);
	memset(&info.function, 0, 255);
#endif
}

struct plua_module_t *plua_get_modules(void) {
	return modules;
}

// static void *plua_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
	// if(nsize == 0) {
		// if(ptr != NULL) {
			// FREE(ptr);
		// }
		// return 0;
	// } else {
		// return REALLOC(ptr, nsize);
	// }
// }

#ifdef PILIGHT_UNITTEST
static void hook(lua_State *L, lua_Debug *ar) {
	if(covfile == NULL) {
		return;
	}

	lua_getinfo(L, "nSfLl", ar);

	int i = 0, match = -1;
	for(i=0;i<nrcoverage;i++) {
		if(strcmp(coverage[i]->file, ar->source) == 0) {
			match = i;
			break;
		}
	}
	if(match == -1) {
		if((coverage = REALLOC(coverage, sizeof(struct coverage_t *)*(nrcoverage+1))) == NULL) {
			OUT_OF_MEMORY
		}
		if((coverage[nrcoverage] = MALLOC(sizeof(struct coverage_t))) == NULL) {
			OUT_OF_MEMORY
		}
		memset(coverage[nrcoverage], 0, sizeof(struct coverage_t));

		if((coverage[nrcoverage]->file = STRDUP((char *)ar->source)) == NULL) {
			OUT_OF_MEMORY
		}

		match = nrcoverage;
		nrcoverage++;
	}

	if(ar->currentline >= coverage[match]->nrlstat || ar->linedefined >= coverage[match]->nrlstat) {
		if((coverage[match]->lstat = REALLOC(coverage[match]->lstat, sizeof(int)*(coverage[match]->nrlstat+512))) == NULL) {
			OUT_OF_MEMORY
		}
		memset(&coverage[match]->lstat[coverage[match]->nrlstat], -1, sizeof(int)*(coverage[match]->nrlstat+512));
		coverage[match]->nrlstat += 512;
	}

	if(ar->currentline >= coverage[match]->nrfstat || ar->linedefined >= coverage[match]->nrfstat) {
		if((coverage[match]->fstat = REALLOC(coverage[match]->fstat, sizeof(int)*(coverage[match]->nrfstat+512))) == NULL) {
			OUT_OF_MEMORY
		}
		memset(&coverage[match]->fstat[coverage[match]->nrfstat], -1, sizeof(int)*(coverage[match]->nrfstat+512));
		coverage[match]->nrfstat += 512;
	}

	if(ar->currentline >= coverage[match]->nrfunctions || ar->linedefined >= coverage[match]->nrfunctions) {
		if((coverage[match]->functions = REALLOC(coverage[match]->functions, sizeof(char *)*(coverage[match]->nrfunctions+12))) == NULL) {
			OUT_OF_MEMORY
		}
		for(i=coverage[match]->nrfunctions;i<coverage[match]->nrfunctions+12;i++) {
			coverage[match]->functions[i] = NULL;
		}
		coverage[match]->nrfunctions += 12;
	}

	if(coverage[match]->lstat[ar->currentline] == -1) {
		coverage[match]->lstat[ar->currentline] = 0;
	}

	if(coverage[match]->lstat[ar->linedefined] == -1) {
		coverage[match]->lstat[ar->linedefined] = 0;
	}

	if(coverage[match]->fstat[ar->linedefined] == -1) {
		coverage[match]->fstat[ar->linedefined] = 0;
	}

	coverage[match]->lstat[ar->currentline]++;
	coverage[match]->fstat[ar->linedefined]++;

	if(strlen(info.module) > 0 && coverage[match]->module == NULL) {
		if((coverage[match]->module = STRDUP(info.module)) == NULL) {
			OUT_OF_MEMORY
		}
	}
	if(strlen(info.function) > 0 && coverage[match]->functions[ar->linedefined] == NULL) {
		if((coverage[match]->functions[ar->linedefined] = STRDUP(info.function)) == NULL) {
			OUT_OF_MEMORY
		}
	}

	if(lua_type(L, -1) == LUA_TTABLE) {
		lua_pushvalue(L, -1);
		lua_pushnil(L);

		while(lua_next(L, -2)) {
			lua_pushvalue(L, -2);

			int line = lua_tonumber(L, -1);

			if(line >= coverage[match]->nrlstat) {
				if((coverage[match]->lstat = REALLOC(coverage[match]->lstat, sizeof(int)*(coverage[match]->nrlstat+512))) == NULL) {
					OUT_OF_MEMORY
				}
				memset(&coverage[match]->lstat[coverage[match]->nrlstat], -1, sizeof(int)*(coverage[match]->nrlstat+512));
				coverage[match]->nrlstat += 512;
			}

			if(coverage[match]->lstat[line] == -1) {
				coverage[match]->lstat[line] = 0;
			}

			lua_pop(L, 2);
		}

		lua_pop(L, 1);
	}
}

void plua_coverage_output(const char *file) {
	covfile = file;
}
#endif

void plua_init(void) {
	if(init == 1) {
		return;
	}
	init = 1;

	int i = 0;
	for(i=0;i<NRLUASTATES;i++) {
		memset(&lua_state[i], 0, sizeof(struct lua_state_t));
		uv_mutex_init(&lua_state[i].lock);
		uv_mutex_init(&lua_state[i].gc.lock);

		lua_State *L = luaL_newstate();

		luaL_openlibs(L);
		plua_register_library(L);
		lua_state[i].L = L;
		lua_state[i].idx = i;

#ifdef PILIGHT_UNITTEST
		lua_sethook(L, hook, LUA_MASKLINE, 0);
#endif
	}
	/*
	 * Initialize global state garbage collector
	 */
	i++;
	memset(&lua_state[i], 0, sizeof(struct lua_state_t));
	uv_mutex_init(&lua_state[i].gc.lock);
}

int plua_module_exists(char *module, int type) {
	struct lua_state_t *state = plua_get_free_state();
	struct lua_State *L = NULL;

	if(state == NULL) {
		return 1;
	}
	if((L = state->L) == NULL) {
		plua_clear_state(state);
		return 1;
	}

	char name[255], *p = name;
	memset(name, '\0', 255);

	switch(type) {
		case UNITTEST: {
			sprintf(p, "unittest.%s", module);
		} break;
		case OPERATOR: {
			sprintf(p, "operator.%s", module);
		} break;
		case FUNCTION:
			sprintf(p, "function.%s", module);
		break;
		case ACTION:
			sprintf(p, "action.%s", module);
		break;
		case PROTOCOL:
			sprintf(p, "protocol.%s", module);
		break;
		case STORAGE:
			sprintf(p, "storage.%s", module);
		break;
	}

	lua_getglobal(L, name);
	if(lua_type(L, -1) == LUA_TNIL) {
		lua_pop(L, -1);
		return -1;
	}
	if(lua_type(L, -1) != LUA_TTABLE) {
		lua_pop(L, -1);
		return -1;
	}
	lua_pop(L, -1);

	assert(lua_gettop(L) == 0);
	plua_clear_state(state);
	return 0;
}

void plua_gc_unreg(lua_State *L, void *ptr) {
	struct lua_state_t *state = NULL;

	if(L == NULL) {
		state = &lua_state[NRLUASTATES];
	} else {
		state = plua_get_current_state(L);
	}
	assert(state != NULL);

	uv_mutex_lock(&state->gc.lock);
	int i = 0;
	for(i=0;i<state->gc.nr;i++) {
		if(state->gc.list[i]->ptr == ptr) {
			memset(state->gc.list[i], 0, sizeof(**state->gc.list));
			state->gc.list[i]->free = 1;
			break;
		}
	}
	uv_mutex_unlock(&state->gc.lock);
}

void plua_gc_reg(lua_State *L, void *ptr, void (*callback)(void *ptr)) {
	struct lua_state_t *state = NULL;

	if(L == NULL) {
		state = &lua_state[NRLUASTATES];
	} else {
		state = plua_get_current_state(L);
	}
	assert(state != NULL);

	uv_mutex_lock(&state->gc.lock);
	int slot = -1, i = 0;
	for(i=0;i<state->gc.nr;i++) {
		if(state->gc.list[i]->free == 1) {
			state->gc.list[i]->free = 0;
			slot = i;
			break;
		}
	}

	if(slot == -1) {
		if(state->gc.size <= state->gc.nr) {
			if((state->gc.list = REALLOC(state->gc.list, sizeof(**state->gc.list)*(state->gc.size+12))) == NULL) {
				OUT_OF_MEMORY
			}
			memset(&state->gc.list[state->gc.size], 0, sizeof(**state->gc.list)*12);
			state->gc.size += 12;
		}
		if(state->gc.list[state->gc.nr] == NULL) {
			if((state->gc.list[state->gc.nr] = MALLOC(sizeof(**state->gc.list))) == NULL) {
				OUT_OF_MEMORY
			}
		}
		slot = state->gc.nr++;
	}
	memset(state->gc.list[slot], 0, sizeof(**state->gc.list));

	state->gc.list[slot]->ptr = ptr;
	state->gc.list[slot]->callback = callback;

	uv_mutex_unlock(&state->gc.lock);
}

void plua_ret_true(lua_State *L) {
	while(lua_gettop(L) > 0) {
		lua_pop(L, 1);
	}
	assert(lua_gettop(L) == 0);
	lua_pushboolean(L, 1);
}

void plua_ret_false(lua_State *L) {
	while(lua_gettop(L) > 0) {
		lua_pop(L, 1);
	}
	assert(lua_gettop(L) == 0);
	lua_pushboolean(L, 0);
}

#ifdef PILIGHT_UNITTEST
void plua_override_global(char *name, int (*func)(lua_State *L)) {
	int i = 0;
	for(i=0;i<NRLUASTATES;i++) {
		uv_mutex_lock(&lua_state[i].lock);
		lua_getglobal(lua_state[i].L, "_G");
		lua_pushcfunction(lua_state[i].L, func);
		lua_setfield(lua_state[i].L, -2, name);
		lua_remove(lua_state[i].L, -1);
		uv_mutex_unlock(&lua_state[i].lock);
	}
}
#endif

int plua_gc(void) {
	struct plua_module_t *tmp = NULL;
	while(modules) {
		tmp = modules;
		FREE(tmp->bytecode);
		modules = modules->next;
		FREE(tmp);
	}
	int i = 0, x = 0;
	for(i=0;i<NRLUASTATES+1;i++) {
		if(lua_state[i].L != NULL) {
			lua_gc(lua_state[i].L, LUA_GCCOLLECT, 0);
			lua_close(lua_state[i].L);
			lua_state[i].L = NULL;
		}
		for(x=0;x<lua_state[i].gc.nr;x++) {
			if(lua_state[i].gc.list[x]->free == 0) {
				lua_state[i].gc.list[x]->callback(lua_state[i].gc.list[x]->ptr);
			}
			FREE(lua_state[i].gc.list[x]);
		}
		if(lua_state[i].gc.size > 0) {
			FREE(lua_state[i].gc.list);
		}
		lua_state[i].gc.nr = 0;
		lua_state[i].gc.size = 0;
	}

#ifdef PILIGHT_UNITTEST
	if(covfile != NULL) {
		FILE *fp = NULL;
		char *name = MALLOC(strlen(covfile)+6);
		strcpy(name, covfile);
		strcat(name, ".info");

		/* Overwrite config file with proper format */
		if((fp = fopen(name, "w")) == NULL) {
			logprintf(LOG_ERR, "cannot write coverage output: %s", name);
			return -1;
		}
		fseek(fp, 0L, SEEK_SET);

		for(i=0;i<nrcoverage;i++) {
			char resolved_path[PATH_MAX];
			realpath(&coverage[i]->file[1], resolved_path);

			fprintf(fp, "TN:\n");
			fprintf(fp, "SF:%s\n", resolved_path);
			for(x=1;x<coverage[i]->nrfstat;x++) {
				if(coverage[i]->fstat[x] >= 0) {
					if(coverage[i]->functions[x] != NULL) {
						fprintf(fp, "FN:%d,%s\n", x, coverage[i]->functions[x]);
					} else {
						fprintf(fp, "FNDA:%d,%d\n", coverage[i]->fstat[x], x);
					}
				}
			}
			for(x=1;x<coverage[i]->nrfstat;x++) {
				if(coverage[i]->fstat[x] >= 0) {
					if(coverage[i]->functions[x] != NULL) {
						fprintf(fp, "FNDA:%d,%s\n", coverage[i]->fstat[x], coverage[i]->functions[x]);
						FREE(coverage[i]->functions[x]);
					} else {
						fprintf(fp, "FNDA:%d,%d\n", coverage[i]->fstat[x], x);
					}
				}
			}
			for(x=1;x<coverage[i]->nrlstat;x++) {
				if(coverage[i]->lstat[x] >= 0) {
					fprintf(fp, "DA:%d,%d\n", x, coverage[i]->lstat[x]);
				}
			}
			fprintf(fp, "end_of_record\n");

			if(coverage[i]->nrfunctions > 0) {
				FREE(coverage[i]->functions);
			}
			if(coverage[i]->nrfstat > 0) {
				FREE(coverage[i]->fstat);
			}
			if(coverage[i]->nrlstat > 0) {
				FREE(coverage[i]->lstat);
			}
			FREE(coverage[i]->file);
			if(coverage[i]->module != NULL) {
				FREE(coverage[i]->module);
			}
			FREE(coverage[i]);
		}
		if(coverage != NULL) {
			FREE(coverage);
		}
		fclose(fp);
		FREE(name);
		nrcoverage = 0;
	}
#endif

	init = 0;
	logprintf(LOG_DEBUG, "garbage collected lua library");
	return 0;
}
