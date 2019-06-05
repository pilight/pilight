/*
	Copyright (C) 2013 - 2014 CurlyMo

	This file is part of pilight.

	pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

	pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

#ifndef _HARDWARE_H_
#define _HARDWARE_H_

#include "../lua_c/lua.h"

typedef enum {
	HWINTERNAL = -1,
	RFNONE = 0,
	RF433,
	RF868,
	RFIR,
	SENSOR,
	HWRELAY,
	API,
	SHELLY
} hwtype_t;

void hardware_init(void);
int hardware_gc(void);
int config_hardware_get_type(lua_State *L, char *module);
int config_hardware_run(void);

#endif
