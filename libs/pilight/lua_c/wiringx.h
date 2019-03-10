/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _LUA_WIRINGX_H_
#define _LUA_WIRINGX_H_

#include <wiringx.h>
#include "lua.h"

extern int plua_wiringx_setup(struct lua_State *L);

static const struct {
	char *name;
	union {
		char *string_;
		int number_;
	} value;
	int type;
} wiringx_globals[] = {
	{ "PINMODE_NOT_SET", { .number_ = PINMODE_NOT_SET }, LUA_TNUMBER },
	{ "PINMODE_INPUT", { .number_ = PINMODE_INPUT }, LUA_TNUMBER },
	{ "PINMODE_OUTPUT", { .number_ = PINMODE_OUTPUT }, LUA_TNUMBER },
	{ "PINMODE_INTERRUPT", { .number_ = PINMODE_INTERRUPT }, LUA_TNUMBER },
	{ "ISR_MODE_UNKNOWN", { .number_ = ISR_MODE_UNKNOWN }, LUA_TNUMBER },
	{ "ISR_MODE_RISING", { .number_ = ISR_MODE_RISING }, LUA_TNUMBER },
	{ "ISR_MODE_FALLING", { .number_ = ISR_MODE_FALLING }, LUA_TNUMBER },
	{ "ISR_MODE_BOTH", { .number_ = ISR_MODE_BOTH }, LUA_TNUMBER },
	{ "ISR_MODE_NONE", { .number_ = ISR_MODE_NONE }, LUA_TNUMBER },
	{ "LOW", { .number_ = LOW }, LUA_TNUMBER },
	{ "HIGH", { .number_ = HIGH }, LUA_TNUMBER },
};

static const luaL_Reg wiringx_lib[] = {
	{"setup", plua_wiringx_setup},
	{NULL, NULL}
};

#endif