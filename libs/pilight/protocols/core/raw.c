/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "raw.h"

static int createCode(struct JsonNode *code, char **message) {
	char *rcode = NULL;
	double repeats = 10;
	char **array = NULL;
	unsigned int i = 0, n = 0;

	if(json_find_string(code, "code", &rcode) != 0) {
		logprintf(LOG_ERR, "raw: insufficient number of arguments");
		return EXIT_FAILURE;
	}

	json_find_number(code, "repeats", &repeats);

	n = explode(rcode, " ", &array);
	for(i=0;i<n;i++) {
		raw->raw[i]=atoi(array[i]);
	}
	array_free(&array, n);

	raw->rawlen=(int)i;
	raw->txrpt = repeats;

	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -c --code=\"raw\"\t\traw code devided by spaces\n\t\t\t\t\t(just like the output of debug)\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void rawInit(void) {

	protocol_register(&raw);
	protocol_set_id(raw, "raw");
	protocol_device_add(raw, "raw", "Raw Codes");
	raw->devtype = RAW;
	raw->hwtype = RF433;
	raw->config = 0;

	options_add(&raw->options, "c", "code", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&raw->options, "r", "repeats", OPTION_OPT_VALUE, 0, JSON_NUMBER, NULL, NULL);

	raw->createCode=&createCode;
	raw->printHelp=&printHelp;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "raw";
	module->version = "2.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	rawInit();
}
#endif
