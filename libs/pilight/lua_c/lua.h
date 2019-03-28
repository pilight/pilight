/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _LUA_H_
#define _LUA_H_

#include <luajit-2.0/lua.h>
#include <luajit-2.0/lualib.h>
#include <luajit-2.0/lauxlib.h>

#include "../libs/pilight/core/common.h"

#define NRLUASTATES	4

#define UNITTEST	0
#define OPERATOR	1
#define FUNCTION	2
#define ACTION 		3
#define PROTOCOL 	4
#define STORAGE		5
#define HARDWARE	6

typedef struct plua_metatable_t {
	struct {
		struct varcont_t val;
		struct varcont_t key;
	} *table;
	int nrvar;
	int iter[NRLUASTATES];

	uv_mutex_t lock;
	uv_sem_t *ref;
} plua_metatable_t;

typedef struct plua_module_t {
	char name[255];
	char file[PATH_MAX];
	char version[12];
	char reqversion[12];
	char reqcommit[12];
	char *bytecode;
	int size;
	int type;
	// struct plua_metatable_t *table;

	struct plua_module_t *next;
} plua_module_t;

typedef struct lua_state_t {
	lua_State *L;
	uv_mutex_t lock;
	struct plua_module_t *module;
	struct plua_metatable_t *table;
	int idx;

	struct {
		struct {
			int free;
			void *ptr;
			void (*callback)(void *ptr);
		} **list;
		int nr;
		int size;
		uv_mutex_t lock;
	} gc;

} lua_state_t;

#define PLUA_INTERFACE_FIELDS			\
  /* public */										\
	lua_State *L;										\
	struct plua_metatable_t *table;	\
	struct plua_module_t *module;		\

typedef struct plua_interface_t {
  PLUA_INTERFACE_FIELDS
} plua_interface_t;

int plua_json_to_table(struct plua_metatable_t *table, struct JsonNode *jnode);
int plua_pcall(struct lua_State *L, char *file, int args, int ret);
int plua_get_method(struct lua_State *L, char *file, char *method);
void plua_gc_unreg(lua_State *L, void *ptr);
void plua_gc_reg(lua_State *L, void *ptr, void (*callback)(void *ptr));
void plua_metatable_free(struct plua_metatable_t *table);
int plua_metatable_get_keys(lua_State *L);
void plua_metatable_parse_set(lua_State *L, void *table);
void plua_metatable__push(lua_State *L, struct plua_interface_t *module);
void plua_metatable_push(lua_State *L, struct plua_metatable_t *table);
void plua_stack_dump(lua_State *L);
void plua_module_load(char *, int);
int plua_module_exists(char *, int);
void plua_metatable_clone(struct plua_metatable_t **, struct plua_metatable_t **);
struct lua_state_t *plua_get_free_state(void);
void plua_clear_state(struct lua_state_t *state);
struct lua_state_t *plua_get_current_state(lua_State *L);
struct plua_module_t *plua_get_modules(void);
void plua_init(void);
void plua_ret_true(lua_State *L);
void plua_ret_false(lua_State *L);
#ifdef PILIGHT_UNITTEST
void plua_pause_coverage(int status);
void plua_coverage_output(const char *);
int plua_flush_coverage(void);
#endif
int plua_namespace(struct plua_module_t *module, char *p);
void plua_package_path(const char *path);
void plua_override_global(char *name, int (*func)(lua_State *L));
int plua_gc(void);

#endif