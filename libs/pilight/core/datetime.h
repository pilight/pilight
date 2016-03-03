/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
