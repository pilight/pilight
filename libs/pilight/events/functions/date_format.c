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
#include "date_format.h"

static int run(struct rules_t *obj, struct JsonNode *arguments, char **ret, enum origin_t origin) {
	struct JsonNode *childs = json_first_child(arguments);
	struct devices_t *dev = NULL;
	struct devices_settings_t *opt = NULL;
	struct protocols_t *protocol = NULL;
	struct tm tm;
	char *p = *ret, *datetime = NULL, *format = NULL;

	memset(&tm, 0, sizeof(struct tm));

	if(childs == NULL) {
		logprintf(LOG_ERR, "DATE_FORMAT requires at least two parameters e.g. DATE_FORMAT(datetime, %Y-%m-%d)");
		return -1;
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
			return -1;
		}
	} else {
		datetime = childs->string_;
	}

	childs = childs->next;
	if(childs == NULL) {
		logprintf(LOG_ERR, "DATE_FORMAT requires at least two parameters e.g. DATE_FORMAT(datetime, %%Y-%%m-%%d)");
		return -1;
	}
	format = childs->string_;

	if(childs->next == NULL && dev == NULL) {
		logprintf(LOG_ERR, "DATE_FORMAT requires at least three parameters when passing a datetime string e.g. DATE_FORMAT(01-01-2015, %%d-%%m-%%Y, %%Y-%%m-%%d)");
		return -1;
	}
	if(childs->next != NULL && dev != NULL) {
		logprintf(LOG_ERR, "DATE_FORMAT requires at least two parameters e.g. DATE_FORMAT(datetime, %%Y-%%m-%%d)");
		return -1;
	}

	if(dev == NULL) {
		childs = childs->next;
		if(strptime(datetime, format, &tm) == NULL) {
			logprintf(LOG_ERR, "DATE_FORMAT is unable to parse \"%s\" as \"%s\" ", datetime, format);
			return -1;
		}
	}
	int year = tm.tm_year+1900;
	int month = tm.tm_mon+1;
	int day = tm.tm_mday;
	int hour = tm.tm_hour;
	int minute = tm.tm_min;
	int second = tm.tm_sec;
	int weekday = tm.tm_wday;

	datefix(&year, &month, &day, &hour, &minute, &second, &weekday);

	tm.tm_year = year-1900;
	tm.tm_mon = month-1;
	tm.tm_mday = day;
	tm.tm_hour = hour;
	tm.tm_min = minute;
	tm.tm_sec = second;
	tm.tm_wday = weekday;
	tm.tm_isdst = -1;

	if(mktime(&tm) < 0) {
		logprintf(LOG_ERR, "DATE_FORMAT error on mktime()");
		return -1;
	}

	strftime(p, BUFFER_SIZE, childs->string_, &tm);

	return 0;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void functionDateFormatInit(void) {
	event_function_register(&function_date_format, "DATE_FORMAT");

	function_date_format->run = &run;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "date_format";
	module->version = "1.1";
	module->reqversion = "6.0";
	module->reqcommit = "94";
}

void init(void) {
	functionDateFormatInit();
}
#endif
