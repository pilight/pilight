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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "raw.h"

static int rawCreateCode(JsonNode *code) {
	char *rcode = NULL;
	char *ncode = NULL;
	char *pch = NULL;
	int i=0;

	if(json_find_string(code, "code", &rcode) != 0) {
		logprintf(LOG_ERR, "raw: insufficient number of arguments");
		return EXIT_FAILURE;
	}

	ncode = malloc(strlen(rcode)+1);
	if(!ncode) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(ncode, rcode);
	pch = strtok(ncode, " ");
	while(pch != NULL) {
		raw->raw[i]=atoi(pch);
		pch = strtok(NULL, " ");
		i++;
	}
	sfree((void *)&ncode);
	raw->rawlen=i;
	return EXIT_SUCCESS;
}

static void rawPrintHelp(void) {
	printf("\t -c --code=\"raw\"\t\traw code devided by spaces\n\t\t\t\t\t(just like the output of debug)\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void rawInit(void) {

	protocol_register(&raw);
	protocol_set_id(raw, "raw");
	protocol_device_add(raw, "raw", "Raw Codes");
	raw->devtype = RAW;
	raw->config = 0;

	options_add(&raw->options, 'c', "code", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);

	raw->createCode=&rawCreateCode;
	raw->printHelp=&rawPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "raw";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	rawInit();
}
#endif
