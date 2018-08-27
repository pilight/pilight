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

#include "../core/log.h"
#include "table.h"

static void plua_table_gc(void *ptr) {
	struct plua_metatable_t *table = ptr;

	plua_metatable_free(table);
}


int plua_table(struct lua_State *L) {
	if(lua_gettop(L) != 0) {
		luaL_error(L, "table requires 0 arguments, %d given", lua_gettop(L));
		return 0;
	}

	struct lua_state_t *state = plua_get_current_state(L);
	if(state == NULL) {
		return 0;
	}

	struct plua_metatable_t *table = MALLOC(sizeof(struct plua_metatable_t));
	if(table == NULL) {
		OUT_OF_MEMORY
	}
	memset(table, 0, sizeof(struct plua_metatable_t));

	plua_metatable_push(L, table);

	plua_gc_reg(L, table, plua_table_gc);

	lua_assert(lua_gettop(L) == 1);

	return 1;
}
