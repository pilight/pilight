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

#ifndef _LOG_H_
#define _LOG_H_

#include <syslog.h>
#include "settings.h"

void logprintf(int prio, const char *format_str, ...);
void logperror(int prio, const char *s);
void log_file_enable(void);
void log_file_disable(void);
void log_shell_enable(void);
void log_shell_disable(void);
void log_file_set(char *file);
void log_level_set(int level);
int log_level_get(void);

#endif
