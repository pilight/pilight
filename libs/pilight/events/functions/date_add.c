/*
	Copyright (C) 2013 - 2015 CurlyMo

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#ifndef __USE_XOPEN
	#define __USE_XOPEN
#endif
#include <time.h>

#ifdef _WIN32
#include "../../core/strptime.h"
#endif

#include "../function.h"
#include "../events.h"
#include "../../config/devices.h"
#include "../../core/options.h"
#include "../../core/log.h"
#include "../../core/dso.h"
#include "../../core/pilight.h"
#include "../../core/datetime.h"
#include "date_add.h"

static struct units_t {
	char name[255];
	int id;
} units[] = {
	{ "SECOND", 1 },
	{ "MINUTE", 2 },
	{ "HOUR", 4 },
	{ "DAY", 8 },
	{ "MONTH", 16 },
	{ "YEAR", 32 }
};

static void add(struct tm *tm, int *values, int type) {
	if(type == -1) {
		return;
	}
	if(((1 << 0x5) & type) == (1 << 0x5)) {
		tm->tm_year += values[5];
	}
	if(((1 << 0x4) & type) == (1 << 0x4)) {
		tm->tm_mon += values[4];
	}
	if(((1 << 0x3) & type) == (1 << 0x3)) {
		tm->tm_mday += values[3];
		tm->tm_wday += values[3];
	}
	if(((1 << 0x2) & type) == (1 << 0x2)) {
		tm->tm_hour += values[2];
	}
	if(((1 << 0x1) & type) == (1 << 0x1)) {
		tm->tm_min += values[1];
	}
	if(((1 << 0x0) & type) == (1 << 0x0)) {
		tm->tm_sec += values[0];
	}
}

static int run(struct rules_t *obj, struct JsonNode *arguments, char **ret, enum origin_t origin) {
	struct JsonNode *childs = json_first_child(arguments);
	struct devices_t *dev = NULL;
	struct devices_settings_t *opt = NULL;
	struct protocols_t *protocol = NULL;
	struct tm tm;
	char *p = *ret, *datetime = NULL, *interval = NULL, **array = NULL;
	int nrunits = (sizeof(units)/sizeof(units[0])), values[nrunits], error = 0;
	int l = 0, i = 0, type = -1, match = 0;

	memset(&values, 0, nrunits);

	if(childs == NULL) {
		logprintf(LOG_ERR, "DATE_ADD requires two parameters e.g. DATE_ADD(datetime, 1 DAY)");
		error = -1;
		goto close;
	}

	if(devices_get(childs->string_, &dev) == 0) {
		if(origin == RULE) {
			event_cache_device(obj, childs->string_);
		}
		protocol = dev->protocols;
		if(protocol->listener->devtype == DATETIME) {
			opt = dev->settings;
			while(opt) {
				if(strcmp(opt->name, "year") == 0) {
					tm.tm_year = opt->values->number_-1900;
				}
				if(strcmp(opt->name, "month") == 0) {
					tm.tm_mon = opt->values->number_-1;
				}
				if(strcmp(opt->name, "day") == 0) {
					tm.tm_mday = opt->values->number_;
				}
				if(strcmp(opt->name, "hour") == 0) {
					tm.tm_hour = opt->values->number_;
				}
				if(strcmp(opt->name, "minute") == 0) {
					tm.tm_min = opt->values->number_;
				}
				if(strcmp(opt->name, "second") == 0) {
					tm.tm_sec = opt->values->number_;
				}
				if(strcmp(opt->name, "weekday") == 0) {
					tm.tm_wday = opt->values->number_-1;
				}
				if(strcmp(opt->name, "dst") == 0) {
					tm.tm_isdst = opt->values->number_;
				}
				opt = opt->next;
			}
		} else {
			logprintf(LOG_ERR, "device \"%s\" is not a datetime protocol", childs->string_);
			error = -1;
			goto close;
		}
	} else {
		datetime = childs->string_;
	}

	childs = childs->next;
	if(childs == NULL) {
		logprintf(LOG_ERR, "DATE_ADD requires two parameters e.g. DATE_ADD(datetime, 1 DAY)");
		error = -1;
		goto close;
	}
	interval = childs->string_;

	if(childs->next != NULL) {
		if(dev == NULL) {
			logprintf(LOG_ERR, "DATE_ADD requires two parameters e.g. DATE_ADD(2000-01-01 12:00:00, 1 DAY)");
		} else {
			logprintf(LOG_ERR, "DATE_ADD requires two parameters e.g. DATE_ADD(datetime, 1 DAY)");
		}
		error = -1;
		goto close;
	}

	l = explode(interval, " ", &array);
	if(l == 2) {
		if(isNumeric(array[0]) == 0) {
			for(i=0;i<nrunits;i++) {
				if(strcmp(array[1], units[i].name) == 0) {
					values[i] = atoi(array[0]);
					type = units[i].id;
					match = 1;
					break;
				}
			}
		} else {
			logprintf(LOG_ERR, "The DATE_ADD unit parameter requires a number and a unit e.g. \"1 DAY\" instead of \"%%Y-%%m-%%d %%H:%%M:%%S\"");
			error = -1;
			goto close;
		}
	} else {
		logprintf(LOG_ERR, "The DATE_ADD unit parameter is formatted as e.g. \"1 DAY\" instead of \"%%Y-%%m-%%d %%H:%%M:%%S\"");
		error = -1;
		goto close;
	}
	if(match == 0) {
		logprintf(LOG_ERR, "DATE_ADD does not accept \"%s\" as a unit", array[1]);
		error = -1;
		goto close;
	}
	if(dev == NULL) {
		if(strptime(datetime, "%Y-%m-%d %H:%M:%S", &tm) == NULL) {
			logprintf(LOG_ERR, "DATE_ADD requires the datetime parameter to be formatted as \"%%Y-%%m-%%d %%H:%%M:%%S\"");
			error = -1;
			goto close;
		}
	}
	add(&tm, values, type);

	int year = tm.tm_year+1900;
	int month = tm.tm_mon+1;
	int day = tm.tm_mday;
	int hour = tm.tm_hour;
	int minute = tm.tm_min;
	int second = tm.tm_sec;
	int weekday = 0;

	datefix(&year, &month, &day, &hour, &minute, &second, &weekday);

	snprintf(p, BUFFER_SIZE, "\"%04d-%02d-%02d %02d:%02d:%02d\"", year, month, day, hour, minute, second);

close:
	array_free(&array, l);
	return error;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void functionDateAddInit(void) {
	event_function_register(&function_date_add, "DATE_ADD");

	function_date_add->run = &run;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "DATE_ADD";
	module->version = "1.1";
	module->reqversion = "6.0";
	module->reqcommit = "94";
}

void init(void) {
	functionDateAddInit();
}
#endif
