/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _LUA_IO_H_
#define _LUA_IO_H_

#include "lua.h"

#include "io/file.h"
#include "io/dir.h"
#include "io/spi.h"
#include "io/serial.h"

static const luaL_Reg pilight_io_lib[] = {
	{"file", plua_io_file},
	{"dir", plua_io_dir},
	{"spi", plua_io_spi},
	{"serial", plua_io_serial},
	{NULL, NULL}
};

#endif