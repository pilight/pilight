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

#include "../core/pilight.h"
#include "../core/common.h"
#include "../core/dso.h"
#include "../core/log.h"
#include "../lua_c/lua.h"

#include "config.h"
#include "settings.h"

static int init = 0;
static char *string = NULL;
static struct plua_metatable_t *table = NULL;
static char root[PATH_MAX] = { 0 };
static char type[255] = "json";

int config_root(char *path) {
	if(strlen(path) > PATH_MAX) {
		return -1;
	}
	strncpy(root, path, PATH_MAX);
	return 0;
}

void config_init(void) {
	if(init == 1) {
		return;
	}
	init = 1;
	plua_init();

	char path[PATH_MAX], path1[PATH_MAX];
	char *f = STRDUP(__FILE__);
	struct dirent *file = NULL;
	DIR *d = NULL;
	struct stat s;
	char *storage_root = STORAGE_ROOT;

	if(strlen(root) > 0) {
		storage_root = root;
	}

	if(f == NULL) {
		OUT_OF_MEMORY
	}

	snprintf(path1, PATH_MAX, "%s%s/", storage_root, type);

	if((d = opendir(path1))) {
		while((file = readdir(d)) != NULL) {
			memset(path, '\0', PATH_MAX);
			snprintf(path, PATH_MAX, "%s%s", path1, file->d_name);
			if(stat(path, &s) == 0) {
				/* Check if file */
				if(S_ISREG(s.st_mode)) {
					if(strstr(file->d_name, ".lua") != NULL) {
						plua_module_load(path, STORAGE);
					}
				}
			}
		}
	}
	closedir(d);
	FREE(f);

	if((table = MALLOC(sizeof(struct plua_metatable_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(table, 0, sizeof(struct plua_metatable_t));
}

int config_exists(char *module) {
	return plua_module_exists(module, STORAGE);
}

struct lua_state_t *plua_get_module(char *namespace, char *module) {
	struct lua_state_t *state = NULL;
	struct lua_State *L = NULL;
	int match = 0;

	state = plua_get_free_state();

	if(state == NULL) {
		return NULL;
	}

	if((L = state->L) == NULL) {
		plua_clear_state(state);
		return NULL;
	}

	char name[255], *p = name;
	memset(name, '\0', 255);

	sprintf(p, "%s.%s", namespace, module);
	lua_getglobal(L, name);
	if(lua_isnil(L, -1) != 0) {
		lua_pop(L, -1);
		assert(lua_gettop(L) == 0);
		plua_clear_state(state);
		return NULL;
	}
	if(lua_istable(L, -1) != 0) {
		struct plua_module_t *tmp = plua_get_modules();
		while(tmp) {
			if(strcmp(module, tmp->name) == 0) {
				state->module = tmp;
				match = 1;
				break;
			}
			tmp = tmp->next;
		}
		if(match == 1) {
			return state;
		}
	}

	lua_pop(L, -1);

	assert(lua_gettop(L) == 0);
	plua_clear_state(state);

	return NULL;
}

int config_callback_read(char *module, char *string) {
	struct lua_state_t *state = plua_get_module("storage", module);
	int x = 0;

	if(state == NULL) {
		return -1;
	}

	plua_get_method(state->L, state->module->file, "read");

	lua_pushstring(state->L, string);

	if((x = plua_pcall(state->L, state->module->file, 1, 1)) == 0) {
		if(lua_isnumber(state->L, -1) == 0) {
			logprintf(LOG_ERR, "%s: the read function returned %s, number expected", state->module->file, lua_typename(state->L, lua_type(state->L, -1)));
			return 0;
		}

		x = (int)lua_tonumber(state->L, -1);

		lua_pop(state->L, 1);
		lua_pop(state->L, -1);
	}

	assert(lua_gettop(state->L) == 0);
	plua_clear_state(state);

	return x;
}

char *config_callback_write(char *module) {
	struct lua_state_t *state = plua_get_module("storage", module);
	char *out = NULL;
	int x = 0;

	if(state == NULL) {
		return NULL;
	}

	plua_get_method(state->L, state->module->file, "write");

	lua_pushstring(state->L, string);

	if((x = plua_pcall(state->L, state->module->file, 1, 1)) == 0) {
		if(lua_type(state->L, -1) != LUA_TSTRING && lua_type(state->L, -1) != LUA_TNIL) {
			logprintf(LOG_ERR, "%s: the write function returned %s, string or nil expected", state->module->file, lua_typename(state->L, lua_type(state->L, -1)));
			return NULL;
		}

		if(lua_type(state->L, -1) == LUA_TSTRING) {
			out = STRDUP((char *)lua_tostring(state->L, -1));
		} else if(lua_type(state->L, -1) == LUA_TNIL) {
			out = NULL;
		}

		lua_pop(state->L, 1);
		lua_pop(state->L, -1);
	}

	assert(lua_gettop(state->L) == 0);
	plua_clear_state(state);

	return out;
}

int config_read(char *str, unsigned short objects) {
	if((string = STRDUP(str)) == NULL) {
		OUT_OF_MEMORY
	}

	if((objects & CONFIG_SETTINGS) == CONFIG_SETTINGS) {
		if(config_callback_read("settings", string) != 1) {
			FREE(string);
			return -1;
		}
	}

	if((objects & CONFIG_HARDWARE) == CONFIG_HARDWARE) {
		if(config_callback_read("hardware", string) != 1) {
			FREE(string);
			return -1;
		}
	}

	if((objects & CONFIG_REGISTRY) == CONFIG_REGISTRY) {
		if(config_callback_read("registry", string) != 1) {
			FREE(string);
			return -1;
		}
	}

	FREE(string);
	return 0;
}

// char *config_print(int level, const char *media) {
	// logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	// struct JsonNode *root = json_mkobject();

	// sort_list(0);
	// struct config_t *listeners = config;
	// while(listeners) {
		// if(listeners->sync) {
			// struct JsonNode *child = listeners->sync(level, media);
			// if(child != NULL) {
				// json_append_member(root, listeners->name, child);
			// }
		// }
		// listeners = listeners->next;
	// }

	// return root;
// }

struct plua_metatable_t *config_get_metatable(void) {
	return table;
}

char *config_write(void) {
	char *settings = config_callback_write("settings");
	if(settings != NULL) {
		FREE(settings);
	}

	char *registry = config_callback_write("registry");
	if(registry != NULL) {
		FREE(registry);
	}

	return 0;
}

int config_gc(void) {
	init = 0;

	if(string != NULL) {
		FREE(string);
		string = NULL;
	}

	if(table != NULL) {
		plua_metatable_free(table);
		table = NULL;
	}
	logprintf(LOG_DEBUG, "garbage collected storage library");
	return 0;
}
