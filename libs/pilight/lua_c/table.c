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
#include "../core/common.h"
#include "table.h"

static void plua_table_gc(void *ptr) {
	struct plua_metatable_t *table = ptr;

	plua_metatable_free(table);
}

int plua_metatable_get(struct plua_metatable_t *table, char *key, struct varcont_t *val) {
	char *ptr = strstr(key, ".");
	unsigned int pos = ptr-key;
	int len = strlen(key);
	int nlen = len-(pos+1);

	if(ptr != NULL) {
		key[pos] = '\0';
	} else {
		pos = len;
	}

	struct varcont_t var;

	if(isNumeric(key) == 0) {
		var.number_ = atof(key);
		var.type_ = LUA_TNUMBER;
	} else {
		var.string_ = key;
		var.type_ = LUA_TSTRING;
	}

	// val->type_ = -1;

	uv_mutex_lock(&table->lock);

	int x = 0, match = 0;
	for(x=0;x<table->nrvar;x++) {
		uv_mutex_lock(&table->table[x].lock);
		if(table->table[x].key.type_ == var.type_) {
			if(
					(var.type_ == LUA_TSTRING && strcmp(table->table[x].key.string_, var.string_) == 0) ||
					(var.type_ == LUA_TNUMBER && table->table[x].key.number_ == var.number_)
				) {
				match = 1;
				switch(table->table[x].val.type_) {
					case LUA_TNUMBER: {
						val->number_ = table->table[x].val.number_;
						val->type_ = LUA_TNUMBER;
					} break;
					case LUA_TSTRING: {
						val->string_ = table->table[x].val.string_;
						val->type_ = LUA_TSTRING;
					} break;
					case LUA_TBOOLEAN: {
						val->bool_ = (int)table->table[x].val.number_;
						val->type_ = LUA_TBOOLEAN;
					} break;
					case LUA_TTABLE: {
						val->void_ = table->table[x].val.void_;
						val->type_ = LUA_TTABLE;
					} break;
					default: {
						val->type_ = -1;
					} break;
				}
				uv_mutex_unlock(&table->table[x].lock);
				break;
			}
		}
		uv_mutex_unlock(&table->table[x].lock);
	}

	uv_mutex_unlock(&table->lock);

	if(ptr != NULL && (len-(pos+1)) > 0) {
		memmove(&key[0], &key[pos+1], len-(pos+1));
		key[nlen] = '\0';
	}

	if(match == 1) {
		if(ptr == NULL) {
			return val->type_;
		} else if(strlen(key) > 0 && val->type_ == LUA_TTABLE) {
			return plua_metatable_get(val->void_, key, val);
		} else {
			return -1;
		}
	} else {
		return -1;
	}
}

int plua_metatable_get_number(struct plua_metatable_t *table, char *a, double *b) {
	struct varcont_t val;
	char *tmp = STRDUP(a);
	if(tmp == NULL) {
		OUT_OF_MEMORY
	}

	if(plua_metatable_get(table, tmp, &val) == LUA_TNUMBER) {
		*b = val.number_;
		FREE(tmp);
		return 0;
	}
	FREE(tmp);
	return -1;
}

int plua_metatable_get_string(struct plua_metatable_t *table, char *a, char **b) {
	struct varcont_t val;
	char *tmp = STRDUP(a);
	if(tmp == NULL) {
		OUT_OF_MEMORY
	}

	if(plua_metatable_get(table, tmp, &val) == LUA_TSTRING) {
		*b = val.string_;
		FREE(tmp);
		return 0;
	}
	FREE(tmp);
	return -1;
}

int plua_metatable_get_boolean(struct plua_metatable_t *table, char *a, int *b) {
	struct varcont_t val;
	char *tmp = STRDUP(a);
	if(tmp == NULL) {
		OUT_OF_MEMORY
	}

	if(plua_metatable_get(table, tmp, &val) == LUA_TBOOLEAN) {
		*b = val.bool_;
		FREE(tmp);
		return 0;
	}
	FREE(tmp);
	return -1;
}

int plua_metatable_get_table(struct plua_metatable_t *table, char *a, struct plua_metatable_t **b) {
	struct varcont_t val;
	char *tmp = STRDUP(a);
	if(tmp == NULL) {
		OUT_OF_MEMORY
	}

	if(plua_metatable_get(table, tmp, &val) == LUA_TTABLE) {
		*b = val.void_;
		FREE(tmp);
		return 0;
	}
	FREE(tmp);
	return -1;
}

int plua_metatable_set(struct plua_metatable_t *table, char *key, struct varcont_t *val) {
	char *ptr = strstr(key, ".");
	unsigned int pos = ptr-key;
	int len = strlen(key);
	int nlen = len-(pos+1);
	int oritype = -1;

	if(ptr != NULL) {
		key[pos] = '\0';
	}

	struct varcont_t var;

	if(ptr != NULL && (len-(pos+1)) > 0) {
		oritype = val->type_;
		val->type_ = LUA_TTABLE;
	}
	if(isNumeric(key) == 0) {
		var.number_ = atof(key);
		var.type_ = LUA_TNUMBER;
	} else {
		var.string_ = key;
		var.type_ = LUA_TSTRING;
	}

	uv_mutex_lock(&table->lock);

	int x = 0, match = 0;
	for(x=0;x<table->nrvar;x++) {
		uv_mutex_lock(&table->table[x].lock);
		if(table->table[x].key.type_ == var.type_) {
			if(
					(var.type_ == LUA_TSTRING && strcmp(table->table[x].key.string_, var.string_) == 0) ||
					(var.type_ == LUA_TNUMBER && table->table[x].key.number_ == var.number_)
				) {
				match = 1;
				switch(val->type_) {
					case LUA_TBOOLEAN: {
						if(table->table[x].val.type_ == LUA_TSTRING) {
							FREE(table->table[x].val.string_);
						}
						if(table->table[x].val.type_ == LUA_TTABLE) {
							plua_metatable_free(table->table[x].val.void_);
						}
						table->table[x].val.number_ = val->number_;
						table->table[x].val.type_ = LUA_TBOOLEAN;
					} break;
					case LUA_TNUMBER: {
						if(table->table[x].val.type_ == LUA_TSTRING) {
							FREE(table->table[x].val.string_);
						}
						if(table->table[x].val.type_ == LUA_TTABLE) {
							plua_metatable_free(table->table[x].val.void_);
						}
						table->table[x].val.number_ = val->number_;
						table->table[x].val.type_ = LUA_TNUMBER;
					} break;
					case LUA_TSTRING: {
						if(table->table[x].val.type_ == LUA_TSTRING) {
							FREE(table->table[x].val.string_);
						}
						if(table->table[x].val.type_ == LUA_TTABLE) {
							plua_metatable_free(table->table[x].val.void_);
						}
						if((table->table[x].val.string_ = STRDUP((char *)val->string_)) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}
						table->table[x].val.type_ = LUA_TSTRING;
					} break;
					case LUA_TTABLE: {
						if(table->table[x].val.type_ == LUA_TSTRING) {
							FREE(table->table[x].val.string_);
						}

						if(table->table[x].val.type_ != LUA_TTABLE) {
							plua_metatable_init((struct plua_metatable_t **)&table->table[x].val.void_);
							table->table[x].val.type_ = LUA_TTABLE;
						}

						if(ptr != NULL && len-(pos+1) > 0) {
							memmove(&key[0], &key[pos+1], len-(pos+1));
							key[nlen] = '\0';
						}

						if(strlen(key) > 0) {
							val->type_ = oritype;
							plua_metatable_set(table->table[x].val.void_, key, val);
						}
					} break;
				}
				uv_mutex_unlock(&table->table[x].lock);
				break;
			}
		}
		uv_mutex_unlock(&table->table[x].lock);
	}

	if(match == 0) {
		int idx = table->nrvar;
		if((table->table = REALLOC(table->table, sizeof(*table->table)*(idx+1))) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		uv_mutex_init(&table->table[idx].lock);
		switch(var.type_) {
			case LUA_TNUMBER: {
				table->table[idx].key.number_ = var.number_;
				table->table[idx].key.type_ = LUA_TNUMBER;
			} break;
			case LUA_TSTRING: {
				if((table->table[idx].key.string_ = STRDUP((char *)var.string_)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				};
				table->table[idx].key.type_ = LUA_TSTRING;
			} break;
		}
		switch(val->type_) {
			case LUA_TBOOLEAN: {
				table->table[idx].val.number_ = val->number_;
				table->table[idx].val.type_ = LUA_TBOOLEAN;
			} break;
			case LUA_TNUMBER: {
				table->table[idx].val.number_ = val->number_;
				table->table[idx].val.type_ = LUA_TNUMBER;
			} break;
			case LUA_TSTRING: {
				if((table->table[idx].val.string_ = STRDUP((char *)val->string_)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				};
				table->table[idx].val.type_ = LUA_TSTRING;
			} break;
			case LUA_TTABLE: {
				plua_metatable_init((struct plua_metatable_t **)&table->table[idx].val.void_);
				table->table[idx].val.type_ = LUA_TTABLE;

				if(ptr != NULL && len-(pos+1) > 0) {
					memmove(&key[0], &key[pos+1], len-(pos+1));
					key[nlen] = '\0';
				}

				if(strlen(key) > 0) {
					val->type_ = oritype;
					plua_metatable_set(table->table[idx].val.void_, key, val);
				}
			} break;
		}
		table->nrvar++;
	}

	uv_mutex_unlock(&table->lock);

	return val->type_;
}

int plua_metatable_set_number(struct plua_metatable_t *table, char *a, double b) {
	struct varcont_t val;
	char *tmp = STRDUP(a);
	if(tmp == NULL) {
		OUT_OF_MEMORY
	}

	val.type_ = LUA_TNUMBER;
	val.number_ = b;

	if(plua_metatable_set(table, tmp, &val) == LUA_TNUMBER) {
		FREE(tmp);
		return 0;
	}
	FREE(tmp);
	return -1;
}

int plua_metatable_set_string(struct plua_metatable_t *table, char *a, char *b) {
	struct varcont_t val;
	char *tmp = STRDUP(a);
	if(tmp == NULL) {
		OUT_OF_MEMORY
	}

	val.type_ = LUA_TSTRING;
	val.string_ = b;

	if(plua_metatable_set(table, tmp, &val) == LUA_TSTRING) {
		FREE(tmp);
		return 0;
	}
	FREE(tmp);
	return -1;
}

int plua_metatable_set_boolean(struct plua_metatable_t *table, char *a, int b) {
	struct varcont_t val;
	char *tmp = STRDUP(a);
	if(tmp == NULL) {
		OUT_OF_MEMORY
	}

	val.type_ = LUA_TBOOLEAN;
	val.number_ = b;

	if(plua_metatable_set(table, tmp, &val) == LUA_TBOOLEAN) {
		FREE(tmp);
		return 0;
	}
	FREE(tmp);
	return -1;
}

void plua_metatable_init(struct plua_metatable_t **table) {
	if((*table = MALLOC(sizeof(struct plua_metatable_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(*table, 0, sizeof(struct plua_metatable_t));
	if(((*table)->ref = MALLOC(sizeof(uv_sem_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	uv_sem_init((*table)->ref, 0);

	uv_mutex_init(&(*table)->lock);
}

int plua_table(struct lua_State *L) {
	if(lua_gettop(L) != 0) {
		pluaL_error(L, "table requires 0 arguments, %d given", lua_gettop(L));
	}

	struct lua_state_t *state = plua_get_current_state(L);
	if(state == NULL) {
		assert(plua_check_stack(L, 0) == 0);
		return 0;
	}

	struct plua_metatable_t *table = NULL;
	plua_metatable_init(&table);

	push_plua_metatable(L, table);

	plua_gc_reg(L, table, plua_table_gc);

	assert(plua_check_stack(L, 1, PLUA_TTABLE) == 0);

	return 1;
}
