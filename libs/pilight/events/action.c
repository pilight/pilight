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
#include <time.h>
#include <assert.h>
#ifndef _WIN32
	#include <dlfcn.h>
	#include <unistd.h>
	#include <sys/time.h>
	#include <libgen.h>
	#include <dirent.h>
#endif

#include "../core/pilight.h"
#include "../core/common.h"
#include "../core/options.h"
#include "../core/dso.h"
#include "../core/log.h"
#include "../lua/lua.h"

#include "action.h"

#ifdef _WIN32
	#include <windows.h>
#endif

static int init = 0;

typedef struct execution_t {
	char *name;
	unsigned long id;

	struct execution_t *next;
} execution_t;

static struct execution_t *executions = NULL;

void event_action_init(void) {
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
	char *actions_root = ACTION_ROOT;

	if(f == NULL) {
		OUT_OF_MEMORY
	}

	settings_select_string(ORIGIN_MASTER, "actions-root", &actions_root);

	if((d = opendir(actions_root))) {
		while((file = readdir(d)) != NULL) {
			memset(path, '\0', PATH_MAX);
			sprintf(path, "%s%s", actions_root, file->d_name);
			if(stat(path, &s) == 0) {
				/* Check if file */
				if(S_ISREG(s.st_mode)) {
					if(strstr(file->d_name, ".lua") != NULL) {
						plua_module_load(path, ACTION);
					}
				}
			}
		}
	}
	closedir(d);
	FREE(f);
}

unsigned long event_action_set_execution_id(char *name) {
	struct timeval tv;
#ifdef _WIN32
	SleepEx(1, TRUE);
#else
	usleep(1);
#endif
	gettimeofday(&tv, NULL);

	int match = 0;
	struct execution_t *tmp = executions;
	while(tmp) {
		if(strcmp(name, tmp->name) == 0) {
			tmp->id = (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;
			match = 1;
			break;
		}
		tmp = tmp->next;
	}
	if(match == 0) {
		if((tmp = MALLOC(sizeof(struct execution_t))) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		if((tmp->name = MALLOC(strlen(name)+1)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		strcpy(tmp->name, name);
		tmp->id = (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

		tmp->next = executions;
		executions = tmp;
	}
	return tmp->id;
}

int event_action_get_execution_id(char *name, unsigned long *ret) {
	struct execution_t *tmp = executions;
	while(tmp) {
		if(strcmp(tmp->name, name) == 0) {
			*ret = tmp->id;
			return 0;
			break;
		}
		tmp = tmp->next;
	}
	return -1;
}

struct event_action_args_t *event_action_add_argument(struct event_action_args_t *head, char *key, struct varcont_t *var) {
	struct event_action_args_t *node = head, *cpy = NULL;
	while(node) {
		if(strcmp(node->key, key) == 0) {
			break;
		}
		node = node->next;
	}

	cpy = node;
	if(node == NULL) {
		if((node = MALLOC(sizeof(struct event_action_args_t))) == NULL) {
			OUT_OF_MEMORY
		}
		memset(node, 0, sizeof(struct event_action_args_t));

		cpy = node;
		if((node->key = STRDUP(key)) == NULL) {
			OUT_OF_MEMORY
		}
		node->var = NULL;
		if(head != NULL) {
			struct event_action_args_t *tmp = head;
			while(tmp->next != NULL) {
				tmp = tmp->next;
			}
			tmp->next = node;
			node = tmp;
		} else {
			node->next = head;
			head = node;
		}
	}

	if(var != NULL) {
		if((cpy->var = REALLOC(cpy->var, sizeof(struct varcont_t *)*(cpy->nrvalues+1))) == NULL) {
			OUT_OF_MEMORY
		}
		if((cpy->var[cpy->nrvalues] = MALLOC(sizeof(struct varcont_t))) == NULL) {
			OUT_OF_MEMORY
		}
		switch(var->type_) {
			case JSON_NUMBER: {
				cpy->var[cpy->nrvalues]->type_ = JSON_NUMBER;
				cpy->var[cpy->nrvalues]->number_ = var->number_;
				cpy->var[cpy->nrvalues]->decimals_ = var->decimals_;
			} break;
			case JSON_STRING: {
				cpy->var[cpy->nrvalues]->type_ = JSON_STRING;
				if((cpy->var[cpy->nrvalues]->string_ = STRDUP(var->string_)) == NULL) {
					OUT_OF_MEMORY
				}
			} break;
			case JSON_BOOL: {
				cpy->var[cpy->nrvalues]->type_ = JSON_BOOL;
				cpy->var[cpy->nrvalues]->bool_ = var->bool_;
			} break;
		}
		cpy->nrvalues++;
	}

	return head;
}

static int plua_action_module_call(struct lua_State *L, char *file, char *func, struct event_action_args_t *args) {
#if LUA_VERSION_NUM <= 502
	lua_getfield(L, -1, func);
	if(lua_type(L, -1) != LUA_TFUNCTION) {
#else
	if(lua_getfield(L, -1, func) == 0) {
#endif
		logprintf(LOG_ERR, "%s: %s action missing", file, func);
		return 0;
	}

	int nrargs = 0, i = 0;
	struct event_action_args_t *tmp1 = NULL;

	lua_newtable(L);

	while(args) {
		tmp1 = args;

		lua_pushstring(L, tmp1->key);
		FREE(tmp1->key);

		lua_createtable(L, 0, 2);
		lua_pushstring(L, "value");

		nrargs++;
		lua_createtable(L, tmp1->nrvalues, 0);
		for(i=0;i<tmp1->nrvalues;i++) {
			lua_pushnumber(L, i+1);

			switch(tmp1->var[i]->type_) {
				case JSON_NUMBER: {
					char *tmp = NULL;
					int len = snprintf(NULL, 0, "%.*f", tmp1->var[i]->decimals_, tmp1->var[i]->number_);
					if((tmp = MALLOC(len+1)) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}
					memset(tmp, 0, len+1);
					sprintf(tmp, "%.*f", tmp1->var[i]->decimals_, tmp1->var[i]->number_);
					lua_pushstring(L, tmp);
					FREE(tmp);
				} break;
				case JSON_STRING:
					lua_pushstring(L, tmp1->var[i]->string_);
					FREE(tmp1->var[i]->string_);
				break;
				case JSON_BOOL:
					lua_pushboolean(L, tmp1->var[i]->bool_);
				break;
			}
			FREE(tmp1->var[i]);
			lua_settable(L, -3);
		}

		lua_settable(L, -3);

		lua_pushstring(L, "order");
		lua_pushnumber(L, nrargs);
		lua_settable(L, -3);
		if(tmp1->nrvalues > 0) {
			FREE(tmp1->var);
		}

		args = args->next;
		FREE(tmp1);
		lua_settable(L, -3);
	}
	args = NULL;

	if(lua_pcall(L, 1, 1, 0) == LUA_ERRRUN) {
		if(lua_type(L, -1) == LUA_TNIL) {
			logprintf(LOG_ERR, "%s: syntax error", file);
			return 0;
		}
		if(lua_type(L, -1) == LUA_TSTRING) {
			logprintf(LOG_ERR, "%s", lua_tostring(L,  -1));
			lua_pop(L, 1);
			return 0;
		}
	}

	if(lua_isnumber(L, -1) == 0) {
		logprintf(LOG_ERR, "%s: the %s function returned %s, number expected", file, func, lua_typename(L, lua_type(L, -1)));
		return 0;
	}

	int ret = (int)lua_tonumber(L, -1);

	lua_remove(L, -1);

	return ret;
}

void event_action_free_argument(struct event_action_args_t *args) {
	struct event_action_args_t *tmp1 = NULL;
	int i = 0;

	while(args) {
		tmp1 = args;
		FREE(tmp1->key);
		for(i=0;i<args->nrvalues;i++) {
			switch(tmp1->var[i]->type_) {
				case JSON_STRING:
					FREE(tmp1->var[i]->string_);
				break;
			}
		}
		if(tmp1->nrvalues > 0) {
			FREE(tmp1->var);
		}
		args = args->next;
		FREE(tmp1);
	}
}

static int event_action_prepare_call(char *module, char *func, struct event_action_args_t *args) {
	struct lua_state_t *state = plua_get_free_state();
	struct lua_State *L = NULL;

	if(state == NULL) {
		return -1;
	}
	if((L = state->L) == NULL) {
		uv_mutex_unlock(&state->lock);
		return -1;
	}

	char name[255], *p = name;
	memset(name, '\0', 255);

	sprintf(p, "action.%s", module);

	lua_getglobal(L, name);

	if(lua_isnil(L, -1) != 0) {
		event_action_free_argument(args);
		lua_remove(L, -1);
		assert(lua_gettop(L) == 0);
		uv_mutex_unlock(&state->lock);
		return -1;
	}
	if(lua_istable(L, -1) != 0) {
		char *file = NULL;
		struct plua_module_t *tmp = plua_get_modules();
		while(tmp) {
			if(strcmp(module, tmp->name) == 0) {
				file = tmp->file;
				state->module = tmp;
				break;
			}
			tmp = tmp->next;
		}
		if(file != NULL) {
			if(plua_action_module_call(L, file, func, args) == 0) {
				lua_pop(L, -1);
				assert(lua_gettop(L) == 0);
				uv_mutex_unlock(&state->lock);
				return -1;
			}
		} else {
			event_action_free_argument(args);
			assert(lua_gettop(L) == 0);
			uv_mutex_unlock(&state->lock);
			return -1;
		}
	}
	lua_remove(L, -1);

	assert(lua_gettop(L) == 0);
	uv_mutex_unlock(&state->lock);

	return 0;
}

static int event_action_parameters_run(struct lua_State *L, char *file, int *nr, char ***ret) {
#if LUA_VERSION_NUM <= 502
	lua_getfield(L, -1, "parameters");
	if(lua_type(L, -1) != LUA_TFUNCTION) {
#else
	if(lua_getfield(L, -1, "parameters") == 0) {
#endif
		logprintf(LOG_ERR, "%s: parameters function missing", file);
		return 0;
	}

	if(lua_pcall(L, 0, LUA_MULTRET, 0) == LUA_ERRRUN) {
		if(lua_type(L, -1) == LUA_TNIL) {
			logprintf(LOG_ERR, "%s: syntax error", file);
			return 0;
		}
		if(lua_type(L, -1) == LUA_TSTRING) {
			logprintf(LOG_ERR, "%s", lua_tostring(L,  -1));
			lua_pop(L, 1);
			return 0;
		}
	}

	int i = 0, err = 0;
	*nr = lua_gettop(L)-1;
	for(i=0;i<*nr;i++) {
		if(lua_type(L, -1) != LUA_TSTRING && err == 0) {
			logprintf(LOG_ERR, "%s: the parameters function returned %s, (a list of) string(s) expected", file, lua_typename(L, lua_type(L, -1)));
			err = 1;
		}
	}
	if(err == 1) {
		for(i=0;i<*nr;i++) {
			lua_remove(L, -1);
		}
		lua_remove(L, -1);
		return 0;
	}

	if(((*ret) = MALLOC((*nr)*sizeof(char *))) == NULL) {
		OUT_OF_MEMORY
	}
	for(i=1;i<=*nr;i++) {
		if(((*ret)[i-1] = STRDUP((char *)lua_tostring(L, -1))) == NULL) {
			OUT_OF_MEMORY
		}
		lua_remove(L, -1);
	}

	lua_remove(L, -1);

	return 1;
}

int event_action_get_parameters(char *module, int *nr, char ***ret) {
	struct lua_state_t *state = plua_get_free_state();
	struct lua_State *L = NULL;

	if(state == NULL) {
		return -1;
	}

	if((L = state->L) == NULL) {
		uv_mutex_unlock(&state->lock);
		return -1;
	}

	char name[255], *p = name;
	memset(name, '\0', 255);

	sprintf(p, "action.%s", module);

	lua_getglobal(L, name);
	if(lua_isnil(L, -1) != 0) {
		lua_remove(L, -1);
		lua_remove(L, -1);
		assert(lua_gettop(L) == 0);
		uv_mutex_unlock(&state->lock);
		return -1;
	}
	if(lua_istable(L, -1) != 0) {
		char *file = NULL;
		struct plua_module_t *tmp = plua_get_modules();
		while(tmp) {
			if(strcmp(module, tmp->name) == 0) {
				file = tmp->file;
				state->module = tmp;
				break;
			}
			tmp = tmp->next;
		}
		if(event_action_parameters_run(L, file, nr, ret) == 0) {
			lua_pop(L, -1);
			assert(lua_gettop(L) == 0);
			uv_mutex_unlock(&state->lock);
			return -1;
		}
	}
	lua_pop(L, -1);

	assert(lua_gettop(L) == 0);
	uv_mutex_unlock(&state->lock);

	return 0;
}

int event_action_check_arguments(char *module, struct event_action_args_t *args) {
	return event_action_prepare_call(module, "check", args);
}

int event_action_run(char *module, struct event_action_args_t *args) {
	return event_action_prepare_call(module, "run", args);
}

int event_action_gc(void) {
	struct execution_t *tmp = NULL;
	while(executions) {
		tmp = executions;
		FREE(tmp->name);
		executions = executions->next;
		FREE(tmp);
	}

	init = 0;

	logprintf(LOG_DEBUG, "garbage collected event action library");
	return 0;
}