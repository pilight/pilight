/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef __USE_XOPEN
	#define __USE_XOPEN
#endif
#include <time.h>
#ifndef _WIN32
	#include <sys/time.h>
	#include <unistd.h>
#endif

#ifdef _WIN32
#include "../../core/strptime.h"
#endif

#include "../function.h"
#include "../events.h"
#include "../../protocols/protocol.h"
#include "../../core/options.h"
#include "../../core/log.h"
#include "../../core/dso.h"
#include "../../core/pilight.h"
#include "../../core/datetime.h"
#include "date_format.h"

static int run(struct rules_t *obj, struct JsonNode *arguments, char **ret, enum origin_t origin) {
	struct JsonNode *childs = json_first_child(arguments);
	struct protocol_t *protocol = NULL;
	struct tm tm;
	char *p = *ret, *datetime = NULL, *format = NULL;
	int i = 0, is_dev = 0, do_free = 0;

	memset(&tm, 0, sizeof(struct tm));

	if(childs == NULL) {
		logprintf(LOG_ERR, "DATE_FORMAT requires at least two parameters e.g. DATE_FORMAT(datetime, %Y-%m-%d)");
		return -1;
	}

	/*
	 * TESTME
	 */
	if(childs->tag == JSON_STRING) {
		if(devices_select(origin, childs->string_, NULL) == 0) {
			is_dev = 1;
			if(origin == ORIGIN_RULE) {
				event_cache_device(obj, childs->string_);
			}
			if(devices_select_protocol(origin, childs->string_, 0, &protocol) == 0) {
				if(protocol->devtype == DATETIME) {
					char *setting = NULL;
					struct varcont_t val;
					i = 0;
					while(devices_select_settings(origin, childs->string_, i++, &setting, &val) == 0) {
						if(strcmp(setting, "year") == 0) {
							tm.tm_year = val.number_-1900;
						}
						if(strcmp(setting, "month") == 0) {
							tm.tm_mon = val.number_-1;
						}
						if(strcmp(setting, "day") == 0) {
							tm.tm_mday = val.number_;
						}
						if(strcmp(setting, "hour") == 0) {
							tm.tm_hour = val.number_;
						}
						if(strcmp(setting, "minute") == 0) {
							tm.tm_min = val.number_;
						}
						if(strcmp(setting, "second") == 0) {
							tm.tm_sec = val.number_;
						}
						if(strcmp(setting, "weekday") == 0) {
							tm.tm_wday = val.number_-1;
						}
						if(strcmp(setting, "dst") == 0) {
							tm.tm_isdst = val.number_;
						}
					}
				} else {
					logprintf(LOG_ERR, "device \"%s\" is not a datetime protocol", childs->string_);
					return -1;
				}
			}
		} else {
			datetime = childs->string_;
		}
	} else if(childs->tag == JSON_NUMBER) {
		size_t len = snprintf(NULL, 0, "%.*f", childs->decimals_, childs->number_);
		if((datetime = MALLOC(len+1)) == NULL) {
			OUT_OF_MEMORY
		}
		snprintf(datetime, len+1, "%.*f", childs->decimals_, childs->number_);
		do_free = 1;
	} else {
		/* Internal error */
		return -1;		
	}

	childs = childs->next;
	if(childs == NULL || childs->tag != JSON_STRING) {
		logprintf(LOG_ERR, "DATE_FORMAT requires at least two parameters e.g. DATE_FORMAT(datetime, %%Y-%%m-%%d)");
		if(do_free == 1) {
			FREE(datetime);
		}
		return -1;
	}
	format = childs->string_;

	if(childs->next == NULL && is_dev == 0) {
		logprintf(LOG_ERR, "DATE_FORMAT requires at least three parameters when passing a datetime string e.g. DATE_FORMAT(01-01-2015, %%d-%%m-%%Y, %%Y-%%m-%%d)");
		if(do_free == 1) {
			FREE(datetime);
		}
		return -1;
	}

	if(is_dev == 0 && childs->next->tag != JSON_STRING) {
		logprintf(LOG_ERR, "DATE_FORMAT requires at least three parameters when passing a datetime string e.g. DATE_FORMAT(01-01-2015, %%d-%%m-%%Y, %%Y-%%m-%%d)");
		if(do_free == 1) {
			FREE(datetime);
		}
		return -1;
	}

	/*
	 * TESTME
	 */
	if(childs->next != NULL && is_dev == 1) {
		logprintf(LOG_ERR, "DATE_FORMAT requires at least two parameters e.g. DATE_FORMAT(datetime, %%Y-%%m-%%d)");
		if(do_free == 1) {
			FREE(datetime);
		}
		return -1;
	}

	if(is_dev == 0) {
		childs = childs->next;
		if(childs != NULL && childs->tag != JSON_STRING) {
			logprintf(LOG_ERR, "DATE_FORMAT requires at least two parameters e.g. DATE_FORMAT(datetime, %%Y-%%m-%%d)");
			if(do_free == 1) {
				FREE(datetime);
			}
			return -1;
		}
		if(strptime(datetime, format, &tm) == NULL) {
			logprintf(LOG_ERR, "DATE_FORMAT is unable to parse \"%s\" as \"%s\" ", datetime, format);
			if(do_free == 1) {
				FREE(datetime);
			}
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

	struct tm tmcpy;
	memcpy(&tmcpy, &tm, sizeof(struct tm));

	if(mktime(&tmcpy) < 0) {
		logprintf(LOG_ERR, "mktime: %s", strerror(errno));
		if(do_free == 1) {
			FREE(datetime);
		}
		return -1;
	}

	strftime(p, BUFFER_SIZE, childs->string_, &tm);

	if(do_free == 1) {
		FREE(datetime);
	}	
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
	module->version = "2.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	functionDateFormatInit();
}
#endif
