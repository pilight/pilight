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
#include <dirent.h>
#ifndef _WIN32
	#include <libgen.h>
	#include <unistd.h>
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "../core/pilight.h"
#include "../core/common.h"
#include "../core/dso.h"
#include "../core/log.h"
#include "../config/settings.h"
#include "../lua/lua.h"

#include "operator.h"

static int init = 0;

void event_operator_init(void) {
	if(init == 1) {
		return;
	}
	init = 1;
	plua_init();

	char path[PATH_MAX];
	char *f = STRDUP(__FILE__);
	struct dirent *file = NULL;
	DIR *d = NULL;
	struct stat s;
	char *operator_root = OPERATOR_ROOT;

	if(f == NULL) {
		OUT_OF_MEMORY
	}

#ifdef PILIGHT_REWRITE
			settings_select_string(ORIGIN_MASTER, "operators-root", &operator_root);
#else
			settings_find_string("operators-root", &operator_root);
#endif

	if((d = opendir(operator_root))) {
		while((file = readdir(d)) != NULL) {
			memset(path, '\0', PATH_MAX);
			sprintf(path, "%s%s", operator_root, file->d_name);
			if(stat(path, &s) == 0) {
				/* Check if file */
				if(S_ISREG(s.st_mode)) {
					if(strstr(file->d_name, ".lua") != NULL) {
						plua_module_load(path, OPERATOR);
					}
				}
			}
		}
	}
	closedir(d);
	FREE(f);
}

static int plua_operator_module_run(struct lua_State *l, char *file, struct varcont_t *a, struct varcont_t *b, char **ret) {
#if LUA_VERSION_NUM <= 502
	lua_getfield(l, -1, "run");
	if(strcmp(lua_typename(l, lua_type(l, -1)), "function") != 0) {
#else
	if(lua_getfield(l, -1, "run") == 0) {
#endif
		logprintf(LOG_ERR, "%s: run function missing", file);
		return 0;
	}

	switch(a->type_) {
		case JSON_NUMBER: {
			char *tmp = NULL;
			int len = snprintf(NULL, 0, "%.*f", a->decimals_, a->number_);
			if((tmp = MALLOC(len+1)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			memset(tmp, 0, len+1);
			sprintf(tmp, "%.*f", a->decimals_, a->number_);
			lua_pushstring(l, tmp);
			FREE(tmp);
		} break;
		case JSON_STRING:
			lua_pushstring(l, a->string_);
		break;
		case JSON_BOOL:
			lua_pushboolean(l, a->bool_);
		break;
	}
	switch(b->type_) {
		case JSON_NUMBER: {
			char *tmp = NULL;
			int len = snprintf(NULL, 0, "%.*f", b->decimals_, b->number_);
			if((tmp = MALLOC(len+1)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			memset(tmp, 0, len+1);
			sprintf(tmp, "%.*f", b->decimals_, b->number_);
			lua_pushstring(l, tmp);
			FREE(tmp);
		} break;
		case JSON_STRING:
			lua_pushstring(l, b->string_);
		break;
		case JSON_BOOL:
			lua_pushboolean(l, b->bool_);
		break;
	}

	if(lua_pcall(l, 2, 1, 0) == LUA_ERRRUN) {
		if(strcmp(lua_typename(l, lua_type(l, -1)), "nil") == 0) {
			logprintf(LOG_ERR, "%s: syntax error", file);
			return 0;
		}
		if(strcmp(lua_typename(l, lua_type(l, -1)), "string") == 0) {
			logprintf(LOG_ERR, "%s", lua_tostring(l,  -1));
			lua_pop(l, 1);
			return 0;
		}
	}

	if(lua_isstring(l, -1) == 0) {
		logprintf(LOG_ERR, "%s: the run function returned %s, string expected", file, lua_typename(l, lua_type(l, -1)));
		return 0;
	}

	strcpy(*ret, lua_tostring(l, -1));

	lua_pop(l, 1);

	return 1;
}

int event_operator_exists(char *module) {
	return plua_module_exists(module, OPERATOR);
}

int event_operator_callback(char *module, struct varcont_t *a, struct varcont_t *b, char **ret) {
	struct lua_State *L = plua_get_state();
	struct lua_State *l = lua_newthread(L);

	char name[255], *p = name;
	memset(name, '\0', 255);

	sprintf(p, "operator.%s", module);

	lua_getglobal(l, name);
	if(lua_isnil(l, -1) != 0) {
		return -1;
	}
	if(lua_istable(l, -1) != 0) {
		char *file = NULL;
		struct plua_module_t *tmp = plua_get_modules();
		while(tmp) {
			if(strcmp(module, tmp->name) == 0) {
				file = tmp->file;
				break;
			}
			tmp = tmp->next;
		}
		if(plua_operator_module_run(l, file, a, b, ret) == 0) {
			lua_pop(l, -1);
			return -1;
		}
	}
	lua_pop(l, -1);

	return 0;
}

int event_operator_gc(void) {
	init = 0;
	logprintf(LOG_DEBUG, "garbage collected event operator library");
	return 0;
}
