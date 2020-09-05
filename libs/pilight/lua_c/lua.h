/*
  Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _LUA_H_
#define _LUA_H_

#include <setjmp.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

// #include "ctrace.h"

#include "../libs/pilight/core/common.h"

#define NRLUASTATES	4

#define UNITTEST	0
#define OPERATOR	1
#define FUNCTION	2
#define ACTION 		3
#define PROTOCOL 	4
#define STORAGE		5
#define HARDWARE	6

#define PLUA_TNONE               (-1)

#define PLUA_TNIL                1
#define PLUA_TBOOLEAN            2
#define PLUA_TLIGHTUSERDATA      4
#define PLUA_TNUMBER             8
#define PLUA_TSTRING             16
#define PLUA_TTABLE              32
#define PLUA_TFUNCTION           64
#define PLUA_TUSERDATA           128
#define PLUA_TTHREAD             256

#define PLUA_TABLE   0
#define PLUA_TIMER   1
#define PLUA_THREAD  2
#define PLUA_EVENT   3
#define PLUA_MQTT    4

typedef struct plua_metatable_t {
	int type;

	struct {
		struct varcont_t val;
		struct varcont_t key;
		uv_mutex_t lock;
	} *table;
	int nrvar;
	int iter[NRLUASTATES];

	uv_mutex_t lock;
	volatile int ref;
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

	int btline;
	const char *btfile;
	// struct plua_metatable_t *table;

	struct plua_module_t *next;
} plua_module_t;

typedef struct lua_state_t {
	lua_State *L;
	// uv_mutex_t lock;
	volatile int claimed;
	struct plua_module_t *module;
	struct plua_module_t *oldmod;
	struct plua_metatable_t *table;
	int idx;

	struct {
		struct {
			int free;
			volatile int ref;
			void *ptr;
			void (*callback)(void *ptr);
		} **list;
		int nr;
		int size;
		int threadid;
		uv_mutex_t lock;
	} gc;

	struct {
		int set;
		int level;
		char *file;
		int line;
	} error;
	jmp_buf jmp;

	char *file;
	int line;
	uv_thread_t thread_id;
} lua_state_t;

#define PLUA_INTERFACE_FIELDS			\
  /* public */										\
	int type;                       \
	lua_State *L;										\
	struct plua_metatable_t *table;	\
	struct plua_module_t *module;		\
	volatile int ref;               \
	uv_mutex_t lock;                \
	void (*gc)(void *data);         \
	int sigterm;                    \

typedef struct plua_interface_t {
  PLUA_INTERFACE_FIELDS
} plua_interface_t;

void plua_set_file_line(lua_State *L, char *file, int line);
void plua_metatable_to_json(struct plua_metatable_t *table, struct JsonNode **jnode);
int plua_json_to_table(struct plua_metatable_t *table, struct JsonNode *jnode);
int plua_pcall(struct lua_State *L, char *file, int args, int ret);
int plua_get_method(struct lua_State *L, char *file, char *method);
void plua_gc_unreg(lua_State *L, void *ptr);
void plua_gc_reg(lua_State *L, void *ptr, void (*callback)(void *ptr));
void plua_metatable_free(struct plua_metatable_t *table);
int plua_metatable_get_keys(lua_State *L);
void plua_metatable_parse_set(lua_State *L, void *table);
void push__plua_metatable(lua_State *L, struct plua_interface_t *module);
void push_plua_metatable(lua_State *L, struct plua_metatable_t *table);
void plua_stack_dump(lua_State *L);
void plua_module_load(char *, int);
int plua_module_exists(char *, int);
void plua_metatable_clone(struct plua_metatable_t **, struct plua_metatable_t **);
struct lua_state_t *plua_get_free_state(void);
void _plua_clear_state(struct lua_state_t *state, char *file, int line);
struct lua_state_t *plua_get_current_state(lua_State *L);
struct plua_module_t *plua_get_modules(void);
void plua_init(void);
int plua_check_stack(lua_State *L, int numargs, ...);
#ifdef PILIGHT_UNITTEST
void plua_pause_coverage(int status);
void plua_coverage_output(const char *);
int plua_flush_coverage(void);
#endif
int plua_namespace(struct plua_module_t *module, char *p);
void plua_package_path(const char *path);
void plua_override_global(char *name, int (*func)(lua_State *L));
char *plua_interface_to_string(int i);
int plua_gc(void);

int plua_set_userdata(lua_State *L, struct plua_interface_t *interface, char *type);
int plua_get_userdata(lua_State *L, struct plua_interface_t *interface, char *type);

#define pluaL_error(a, b, ...) \
	do { \
		logprintf(LOG_DEBUG, "(%s #%d) pluaL_error", __FILE__, __LINE__); \
		plua_set_file_line(a, __FILE__, __LINE__); \
		luaL_error(a, b, ##__VA_ARGS__); \
	} while(0)

#define plua_clear_state(a) _plua_clear_state(a, __FILE__, __LINE__);

#endif
