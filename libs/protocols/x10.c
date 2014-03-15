/*
	Copyright (C) 2014 CurlyMo

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
#include "x10.h"

void x10ParseCode(void) {
	int x = 0;
	int y = 0;
	char letters[18] = {"MNOPCDABEFGHKL IJ"};
	for(x=1;x<x10->rawlen-1;x+=2) {
		x10->binary[y++] = x10->code[x];
	}
	char id[3];
	int l = letters[binToDecRev(x10->binary, 0, 3)];
	int s = x10->binary[18];
	int i = 1;
	int c1 = (binToDec(x10->binary, 0, 7)+binToDec(x10->binary, 8, 15));
	int c2 = (binToDec(x10->binary, 16, 23)+binToDec(x10->binary, 24, 31));
	if(x10->binary[5] == 1) {
		i += 8;
	}
	if(x10->binary[17] == 1) {
		i += 5;
	}
	i += binToDec(x10->binary, 19, 20);
	if(c1 == 255 && c2 == 255) {
		sprintf(id, "%c%d", l, i);
		x10->message = json_mkobject();
		json_append_member(x10->message, "id", json_mkstring(id));
		if(s == 0) {
			json_append_member(x10->message, "state", json_mkstring("on"));
		} else {
			json_append_member(x10->message, "state", json_mkstring("off"));
		}
	}
}

void x10Init(void) {
	
	protocol_register(&x10);
	protocol_set_id(x10, "x10");
	protocol_device_add(x10, "x10", "x10 based devices");
	protocol_plslen_add(x10, 150);
	x10->devtype = SWITCH;
	x10->hwtype = RF433;
	x10->pulse = 10;
	x10->rawlen = 68;

	options_add(&x10->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&x10->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&x10->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_STRING, NULL, NULL);	
	
	x10->parseCode=&x10ParseCode;
}
