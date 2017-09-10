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

#ifndef _WIN32
	#include <libgen.h>
	#include <dirent.h>
	#include <unistd.h>
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "lua.h"
#include "lualibrary.h"
#include "../core/log.h"
#include "../core/json.h"
#include "../core/mem.h"
#include "../core/common.h"

static struct lua_State *L = NULL;

struct plua_module_t *modules = NULL;

// static void stackDump(lua_State *L) {
	// int i = 0;
	// int top = lua_gettop(L);
	// for(i = 1; i <= top; i++) {  /* repeat for each level */
		// int t = lua_type(L, i);
		// switch(t) {
			// case LUA_TSTRING:  /* strings */
				// printf("%d: '%s'", i, lua_tostring(L, i));
			// break;
			// case LUA_TBOOLEAN:  /* booleans */
				// printf("%d: %s", i, lua_toboolean(L, i) ? "true" : "false");
			// break;
			// case LUA_TNUMBER:  /* numbers */
				// printf("%d: %g", i, lua_tonumber(L, i));
			// break;
			// default:  /* other values */
				// printf("%d: %s", i, lua_typename(L, t));
			// break;
		// }
		// printf("  ");  /* put a separator */
	// }
	// printf("\n");  /* end the listing */
// }

static int plua_get_table_string_by_key(struct lua_State *L, const char *key, const char **ret) {
	/*
	 * Push the key we want to retrieve on the stack
	 *
	 * stack now contains: -1 => key -2 => table
	 */
	lua_pushstring(L, key);

	if(lua_istable(L, -2) == 0) {
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
	if(lua_isstring(L, -1) == 0) {
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
	if(lua_istable(L, -1) == 0) {
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
	if(strcmp(lua_typename(L, lua_type(L, -1)), "function") != 0) {
#else
	if(lua_getfield(L, -1, "info") == 0) {
#endif
		logprintf(LOG_ERR, "%s: info function missing", file);
		return 0;
	}

	/*
	 * Returned table (first argument) is at top of stack
	 *
	 * stack now contains -1 => function
	 */
	if(lua_pcall(L, 0, 1, 0) == LUA_ERRRUN) {
		if(strcmp(lua_typename(L, lua_type(L, -1)), "string") == 0) {
			logprintf(LOG_ERR, "%s", lua_tostring(L,  -1));
			lua_pop(L, 1);
			return 0;
		}
	}

	if(lua_istable(L, -1) == 0) {
		logprintf(LOG_ERR, "%s: the info function returned %s, table expected", file, lua_typename(L, lua_type(L, -1)));
		return 0;
	}

	char *keys[12] = {"name", "version", "reqversion", "reqcommit"};
	if(plua_table_has_keys(L, keys, 4) == 0) {
		logprintf(LOG_ERR, "%s: the info table has invalid keys", file);
		return 0;
	}

	const char *name = NULL, *version = NULL, *reqversion = NULL, *reqcommit = NULL;
	if(plua_get_table_string_by_key(L, "name", &name) == 0) {
		logprintf(LOG_ERR, "%s: the info table 'name' key is missing or invalid", file);
		return 0;
	}

	if(plua_get_table_string_by_key(L, "version", &version) == 0) {
		logprintf(LOG_ERR, "%s: the info table 'version' key is missing or invalid", file);
		return 0;
	}

	if(plua_get_table_string_by_key(L, "reqversion", &reqversion) == 0) {
		logprintf(LOG_ERR, "%s: the info table 'reqversion' key is missing or invalid", file);
		return 0;
	}
	if(plua_get_table_string_by_key(L, "reqcommit", &reqcommit) == 0) {
		logprintf(LOG_ERR, "%s: the info table 'reqcommit' key is missing or invalid", file);
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
		logprintf(LOG_DEBUG, "loaded event action %s v%s", file, version);
	} else {
		if(strlen(mod->reqcommit) > 0) {
			logprintf(LOG_ERR, "event action %s requires at least pilight v%s (commit %s)", file, mod->reqversion, mod->reqcommit);
		} else {
			logprintf(LOG_ERR, "event action %s requires at least pilight v%s", file, mod->reqversion);
		}
		/*
		 * Pop function from stack
		 *
		 * The stack now contains: nothing
		 */
		lua_pop(L, 1);
		return 0;
	}

	/*
	 * Pop function from stack
	 *
	 * The stack now contains: nothing
	 */
	lua_pop(L, 1);
	return 1;
}

void plua_module_load(char *file, int type) {
	if(L == NULL) {
		return;
	}

	struct plua_module_t *module = MALLOC(sizeof(struct plua_module_t));
	char name[255] = { '\0' }, *p = name;

	if(module == NULL) {
		OUT_OF_MEMORY
	}

	if(luaL_dofile(L, file) != 0) {
		logprintf(LOG_ERR, "%s", lua_tostring(L,  1));
		lua_pop(L, 1);
	}

	if(lua_istable(L, -1) == 0) {
		logprintf(LOG_ERR, "%s: does not return a table");
		lua_pop(L, 1);
		FREE(module);
		return;
	}

	module->type = type;
	strcpy(module->file, file);
	if(plua_module_init(L, file, module) != 0) {
		memset(p, '\0', sizeof(name));
		switch(module->type) {
			case OPERATOR:
				sprintf(p, "operator.%s", module->name);
			break;
			case FUNCTION:
				sprintf(p, "function.%s", module->name);
			break;
		}
		lua_setglobal(L, name);

		module->next = modules;
		modules = module;
	} else {
		FREE(module);
	}
}

// static void plua_create_module_list(struct lua_State *L) {
	// char name[255], *p = name;
	// int nrmodules = 0;
	// struct plua_module_t *tmp = modules;
	// while(tmp) {
		// nrmodules++;
		// tmp = tmp->next;
	// }

	// /*
	 // * Create an internal hashtable with plugins
	 // */
	// lua_createtable(L, 0, nrmodules);
	// tmp = modules;
	// while(tmp) {
		// memset(p, '\0', sizeof(name));
		// switch(tmp->type) {
			// case OPERATOR:
				// sprintf(p, "operator.%s", tmp->name);
			// break;
		// }

		// lua_pushstring(L, name);
		// lua_pushnumber(L, 1);
		// lua_settable(L, -3);
		// tmp = tmp->next;
	// }
	// lua_setglobal(L, "modules");
// }

struct lua_State *plua_get_state(void) {
	return L;
}

struct plua_module_t *plua_get_modules(void) {
	return modules;
}

static void *plua_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
	if(nsize == 0) {
		if(ptr != NULL) {
			FREE(ptr);
		}
		return 0;
	} else {
		return REALLOC(ptr, nsize);
	}
}

void plua_init(void) {
	if(L == NULL) {
		L = lua_newstate(plua_alloc, NULL);
		luaL_openlibs(L);

		plua_register_library(L);
	}

	// lua_create_module_list(L);
	// lua_list_modules(L);
}

int plua_module_exists(char *module, int type) {
	if(L == NULL) {
		return 1;
	}
	char name[255], *p = name;
	memset(name, '\0', 255);

	switch(type) {
		case OPERATOR: {
			sprintf(p, "operator.%s", module);
		} break;
		case FUNCTION:
			sprintf(p, "function.%s", module);
		break;
	}

	lua_getglobal(L, name);
	if(lua_isnil(L, -1) != 0) {
		lua_pop(L, -1);
		return -1;
	}
	if(lua_istable(L, -1) == 0) {
		lua_pop(L, -1);
		return -1;
	}
	lua_pop(L, -1);

	return 0;
}

int plua_gc(void) {
	struct plua_module_t *tmp = NULL;
	while(modules) {
		tmp = modules;
		modules = modules->next;
		FREE(tmp);
	}
	if(L != NULL) {
		lua_close(L);
		L = NULL;
	}
	logprintf(LOG_DEBUG, "garbage collected lua library");
	return 0;
}
