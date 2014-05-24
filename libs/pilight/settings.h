/*
	Copyright (C) 2013 CurlyMo

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

#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include "json.h"

int settings_find_number(const char *name, int *out);
int settings_find_string(const char *name, char **out);
int settings_path_exists(char *fil);
int settings_parse(JsonNode *root);
int settings_write(char *content);
int settings_read(void);
int settings_set_file(char *settfile);
int settings_gc(void);

#endif
