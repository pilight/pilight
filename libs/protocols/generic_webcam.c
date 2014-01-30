/*
	Copyright (C) 2014 1000io

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
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "generic_webcam.h"

void genWebcamCreateMessage(int id, char *url, int refresh) {
	generic_webcam->message = json_mkobject();
	json_append_member(generic_webcam->message, "id", json_mknumber(id));
	json_append_member(generic_webcam->message, "url", json_mkstring(url));
	if(refresh > -999) {
		json_append_member(generic_webcam->message, "refresh", json_mknumber(refresh));
	}
}

int genWebcamCreateCode(JsonNode *code) {
	int id = -999;
	char *url;
	int refresh = -999;
	char *tmp;

	if(json_find_string(code, "id", &tmp) == 0)
		id=atoi(tmp);
	if(json_find_string(code, "url", &tmp) == "")
		url = atoi(tmp);
	if(json_find_string(code, "refresh", &tmp) == 0)
		refresh = atoi(tmp);

	if(id == -999 && url == "" && refresh == -999) {
		logprintf(LOG_ERR, "generic_webcam: insufficient number of arguments");
		return EXIT_FAILURE;
	} else {
		genWebcamCreateMessage(id, url, refresh);
	}
	return EXIT_SUCCESS;
}

void genWebcamPrintHelp(void) {
	printf("\t -u --url=url\tset the camera url\n");
	printf("\t -r --refresh=refresh\t\tset the camera refresh\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

void genWebcamInit(void) {
	
	protocol_register(&generic_webcam);
	protocol_set_id(generic_webcam, "generic_webcam");
	protocol_device_add(generic_webcam, "generic_webcam", "Generic web camera");
	generic_webcam->devtype = WEBCAM;

	options_add(&generic_webcam->options, 'u', "url", has_value, config_value, "^[ABCDE](3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&generic_webcam->options, 'r', "refresh", has_value, config_value, "[0-9]");
	options_add(&generic_webcam->options, 'i', "id", has_value, config_id, "[0-9]");

	//protocol_setting_add_number(generic_webcam, "decimals", 2);	
	protocol_setting_add_string(generic_webcam, "url", "");
	protocol_setting_add_number(generic_webcam, "refresh", 1);

	generic_webcam->printHelp=&genWebcamPrintHelp;
	generic_webcam->createCode=&genWebcamCreateCode;
}
