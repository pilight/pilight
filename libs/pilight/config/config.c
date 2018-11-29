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
#include "registry.h"
#include "hardware.h"
#include "rules.h"
#include "gui.h"

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

	hardware_init();

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
		logprintf(LOG_ERR, "could not lua storage settings module");
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

int config_set_file(char *settfile) {
	if(access(settfile, R_OK | W_OK) != -1) {
		if((string = STRDUP(settfile)) == NULL) {
			OUT_OF_MEMORY
		}
	} else {
		logprintf(LOG_ERR, "the config file %s does not exist", settfile);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

char *config_get_file(void) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	return string;
}

int config_parse(struct JsonNode *root, unsigned short objects) {

	if(((objects & CONFIG_DEVICES) == CONFIG_DEVICES) || ((objects & CONFIG_ALL) == CONFIG_ALL)) {
		struct JsonNode *jnode = json_find_member(root, "devices");
		if(jnode == NULL) {
			return -1;
		}
		if(config_devices_parse(jnode) != 0) {
			return -1;
		}
	}

	if(((objects & CONFIG_GUI) == CONFIG_GUI) || ((objects & CONFIG_ALL) == CONFIG_ALL)) {
		struct JsonNode *jnode = json_find_member(root, "gui");
		if(jnode == NULL) {
			return -1;
		}
		if(config_gui_parse(jnode) != 0) {
			return -1;
		}
	}

	if(((objects & CONFIG_HARDWARE) == CONFIG_HARDWARE) || ((objects & CONFIG_ALL) == CONFIG_ALL)) {
		struct JsonNode *jnode = json_find_member(root, "hardware");
		if(jnode == NULL) {
			return -1;
		}
		if(config_hardware_parse(jnode) != 0) {
			return -1;
		}
	}

#ifdef EVENTS
	if(((objects & CONFIG_RULES) == CONFIG_RULES) || ((objects & CONFIG_ALL) == CONFIG_ALL)) {
		struct JsonNode *jnode = json_find_member(root, "rules");
		if(jnode == NULL) {
			return -1;
		}
		if(config_rules_parse(jnode) != 0) {
			return -1;
		}
	}
#endif

	return 0;
}

int config_read(unsigned short objects) {
	if(string != NULL) {
		if(((objects & CONFIG_SETTINGS) == CONFIG_SETTINGS) || ((objects & CONFIG_ALL) == CONFIG_ALL)) {
			if(config_callback_read("settings", string) != 1) {
				return -1;
			}
		}

		if(((objects & CONFIG_REGISTRY) == CONFIG_REGISTRY) || ((objects & CONFIG_ALL) == CONFIG_ALL)) {
			if(config_callback_read("registry", string) != 1) {
				return -1;
			}
		}
	}

	char *content = NULL;
	/* Read JSON config file */
	if(file_get_contents(string, &content) == 0) {
		/* Validate JSON and turn into JSON object */
		if(json_validate(content) == false) {
			logprintf(LOG_ERR, "config is not in a valid json format");
			FREE(content);
			return EXIT_FAILURE;
		}

		struct JsonNode *root = json_decode(content);

		if(config_parse(root, objects) == -1) {
			json_delete(root);
			FREE(content);
			return -1;
		}

		json_delete(root);
		config_write(1, "all");
		FREE(content);
	}

	return 0;
}

struct JsonNode *config_print(int level, const char *media) {
	struct JsonNode *root = NULL;
	char *content = NULL;
	/* Read JSON config file */
	if(file_get_contents(string, &content) == 0) {
		/* Validate JSON and turn into JSON object */
		if(json_validate(content) == false) {
			logprintf(LOG_ERR, "config is not in a valid json format");
			FREE(content);
			return NULL;
		}

		root = json_decode(content);

		struct JsonNode *jchild1 = config_devices_sync(level, media);
		struct JsonNode *jchild = json_find_member(root, "devices");
		json_remove_from_parent(jchild);
		char *check = json_stringify(jchild1, NULL);
		if(check != NULL) {
			if(jchild1 != NULL && strcmp(check, "{}") != 0) {
				json_append_member(root, "devices", jchild1);
				json_delete(jchild);
			} else {
				json_append_member(root, "devices", jchild);
				json_delete(jchild1);
			}
			json_free(check);
		} else {
			json_append_member(root, "devices", jchild);
			json_delete(jchild1);
		}

		struct JsonNode *jchild2 = config_rules_sync(level, media);
		jchild = json_find_member(root, "rules");
		json_remove_from_parent(jchild);
		check = json_stringify(jchild2, NULL);
		if(check != NULL) {
			if(jchild2 != NULL && strcmp(check, "{}") != 0) {
				json_append_member(root, "rules", jchild2);
				json_delete(jchild);
			} else {
				json_append_member(root, "rules", jchild);
				json_delete(jchild2);
			}
			json_free(check);
		} else {
			json_append_member(root, "rules", jchild);
			json_delete(jchild2);
		}

		struct JsonNode *jchild3 = config_gui_sync(level, media);
		jchild = json_find_member(root, "gui");
		json_remove_from_parent(jchild);
		check = json_stringify(jchild3, NULL);
		if(check != NULL) {
			if(jchild3 != NULL && strcmp(check, "{}") != 0) {
				json_append_member(root, "gui", jchild3);
				json_delete(jchild);
			} else {
				json_append_member(root, "gui", jchild);
				json_delete(jchild3);
			}
			json_free(check);
		} else {
			json_append_member(root, "gui", jchild);
			json_delete(jchild3);
		}

		char *settings = config_callback_write("settings");
		if(settings != NULL) {
			struct JsonNode *jchild = json_find_member(root, "settings");
			json_remove_from_parent(jchild);
			json_delete(jchild);
			json_append_member(root, "settings", json_decode(settings));
			FREE(settings);
		}

		struct JsonNode *jchild4 = config_hardware_sync(level, media);
		jchild = json_find_member(root, "hardware");
		json_remove_from_parent(jchild);
		check = json_stringify(jchild4, NULL);
		if(check != NULL) {
			if(jchild4 != NULL && strcmp(check, "{}") != 0) {
				json_append_member(root, "hardware", jchild4);
				json_delete(jchild);
			} else {
				json_append_member(root, "hardware", jchild);
				json_delete(jchild4);
			}
			json_free(check);
		} else {
			json_append_member(root, "hardware", jchild);
			json_delete(jchild4);
		}

		char *registry = config_callback_write("registry");
		if(registry != NULL) {
			struct JsonNode *jchild = json_find_member(root, "registry");
			json_remove_from_parent(jchild);
			json_delete(jchild);
			json_append_member(root, "registry", json_decode(registry));
			FREE(registry);
		}
	}
	if(content != NULL) {
		FREE(content);
	}

	return root;
}

struct plua_metatable_t *config_get_metatable(void) {
	return table;
}

int config_write(int level, char *media) {
	FILE *fp = NULL;

	struct JsonNode *root = config_print(level, media);
	/* Overwrite config file with proper format */
	if((fp = fopen(string, "w+")) == NULL) {
		logprintf(LOG_ERR, "cannot write config file: %s", string);
		json_delete(root);
		return EXIT_FAILURE;
	}
	fseek(fp, 0L, SEEK_SET);
	char *content = NULL;
	if((content = json_stringify(root, "\t")) != NULL) {
		fwrite(content, sizeof(char), strlen(content), fp);
		json_free(content);
	}
	json_delete(root);
	fclose(fp);

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
	hardware_gc();
	devices_gc();
	rules_gc();
	gui_gc();

	logprintf(LOG_DEBUG, "garbage collected storage library");
	return 0;
}
