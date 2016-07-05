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

#ifndef _DATETIME_H_
#define _DATETIME_H_

#include <sys/time.h>

int datetime_gc(void);
char *coord2tz(double longitude, double latitude);
time_t datetime2ts(int year, int month, int day, int hour, int minutes, int seconds, char *tz);
struct tm *localtztime(char *tz, time_t t);
int tzoffset(char *tz1, char *tz2);
int ctzoffset(char *tz);
int isdst(time_t t, char *tz);
void datefix(int *year, int *month, int *day, int *hour, int *minute, int *second);
void datetime_init(void);

#endif
