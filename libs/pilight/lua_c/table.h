/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _LUA_TABLE_H_
#define _LUA_TABLE_H_

#include "lua.h"

extern int plua_table(struct lua_State *L);
void plua_metatable_init(struct plua_metatable_t **table);
int plua_metatable_get(struct plua_metatable_t *table, char *a, struct varcont_t *b);
int plua_metatable_get_number(struct plua_metatable_t *table, char *a, double *b);
int plua_metatable_get_boolean(struct plua_metatable_t *table, char *a, int *b);
int plua_metatable_get_string(struct plua_metatable_t *table, char *a, char **b);
int plua_metatable_set_number(struct plua_metatable_t *table, char *a, double b);
int plua_metatable_set_boolean(struct plua_metatable_t *table, char *a, int b);
int plua_metatable_set_string(struct plua_metatable_t *table, char *a, char *b);

#endif