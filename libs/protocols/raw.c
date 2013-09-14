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

#include "settings.h"
#include "log.h"
#include "protocol.h"
#include "binary.h"
#include "gc.h"
#include "raw.h"

int rawCreateCode(JsonNode *code) {
	char *rcode = NULL;
	char *rncode = NULL;
	char *pch = NULL;
	int i=0;

	if(json_find_string(code, "code", &rcode) != 0) {
		logprintf(LOG_ERR, "raw: insufficient number of arguments");
		return EXIT_FAILURE;
	}

	rncode = strdup(rcode);
	pch = strtok(rncode, " ");
	while(pch != NULL) {
		raw->raw[i]=atoi(pch);
		pch = strtok(NULL, " ");
		i++;
	}
	raw->rawLength=i;
	return EXIT_SUCCESS;
}

void rawPrintHelp(void) {
	printf("\t -c --code=\"raw\"\t\traw code devided by spaces\n\t\t\t\t\t(just like the output of debug)\n");
}

void rawInit(void) {

	protocol_register(&raw);
	raw->id = malloc(4);
	strcpy(raw->id, "raw");
	protocol_add_device(raw, "raw", "Raw codes");
	raw->type = RAW;

	options_add(&raw->options, 'c', "code", has_value, 0, NULL);

	raw->createCode=&rawCreateCode;
	raw->printHelp=&rawPrintHelp;
}
