/*
	Copyright (C) 2014 CurlyMo & lvdp

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
#include "arctech_door.h"

void arctechDoorCreateMessage(int id, int unit, int state, int all) {
	arctech_door->message = json_mkobject();
	json_append_member(arctech_door->message, "id", json_mknumber(id));
	if(all == 1) {
		json_append_member(arctech_door->message, "all", json_mknumber(all));
	} else {
		json_append_member(arctech_door->message, "unit", json_mknumber(unit));
	}

	if(state == 1) {
		json_append_member(arctech_door->message, "state", json_mkstring("on"));
	} else {
		json_append_member(arctech_door->message, "state", json_mkstring("off"));
	}
}

void arctechDoorParseBinary(void) {
	int unit = binToDecRev(arctech_door->binary, 28, 31);
	int state = arctech_door->binary[27];
	int all = arctech_door->binary[26];
	int id = binToDecRev(arctech_door->binary, 0, 25);

	arctechDoorCreateMessage(id, unit, state, all);
}

void arctechDoorCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		arctech_door->raw[i]=(arctech_door->plslen->length);
		arctech_door->raw[i+1]=(arctech_door->plslen->length);
		arctech_door->raw[i+2]=(arctech_door->plslen->length);
		arctech_door->raw[i+3]=(arctech_door->pulse*arctech_door->plslen->length);
	}
}

void arctechDoorCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		arctech_door->raw[i]=(arctech_door->plslen->length);
		arctech_door->raw[i+1]=(arctech_door->pulse*arctech_door->plslen->length);
		arctech_door->raw[i+2]=(arctech_door->plslen->length);
		arctech_door->raw[i+3]=(arctech_door->plslen->length);
	}
}

void arctechDoorClearCode(void) {
	arctechDoorCreateLow(2,147);
}

void arctechDoorCreateStart(void) {
	arctech_door->raw[0]=arctech_door->plslen->length;
	arctech_door->raw[1]=(10*arctech_door->plslen->length);
}

void arctechDoorCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=((length-i)+1)*4;
			arctechDoorCreateHigh(106-x, 106-(x-3));
		}
	}
}

void arctechDoorCreateAll(int all) {
	if(all == 1) {
		arctechDoorCreateHigh(106, 109);
	}
}

void arctechDoorCreateState(int state) {
	if(state == 1) {
		arctechDoorCreateHigh(110, 113);
	} else if(state == -1) {
		arctech_door->raw[110]=(arctech_door->plslen->length);
		arctech_door->raw[111]=(arctech_door->plslen->length);
		arctech_door->raw[112]=(arctech_door->plslen->length);
		arctech_door->raw[113]=(arctech_door->plslen->length);
	}
}

void arctechDoorCreateUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=((length-i)+1)*4;
			arctechDoorCreateHigh(130-x, 130-(x-3));
		}
	}
}

void arctechDoorCreateFooter(void) {
	arctech_door->raw[147]=(PULSE_DIV*arctech_door->plslen->length);
}


int arctechDoorCreateCode(JsonNode *code) {
	int id = -1;
	int unit = -1;
	int state = -1;
	int all = 0;
	char *tmp;

	if(json_find_string(code, "id", &tmp) == 0)
		id=atoi(tmp);
	if(json_find_string(code, "off", &tmp) == 0)
		state=0;
	else if(json_find_string(code, "on", &tmp) == 0)
		state=1;
	if(json_find_string(code, "unit", &tmp) == 0)
		unit = atoi(tmp);
	if(json_find_string(code, "all", &tmp) == 0)
		all = 1;

	if(id == -1 || (unit == -1 && all == 0)) {
		logprintf(LOG_ERR, "arctech_door: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 67108863 || id < 1) {
		logprintf(LOG_ERR, "arctech_door: invalid id range");
		return EXIT_FAILURE;
	} else if((unit > 15 || unit < 0) && all == 0) {
		logprintf(LOG_ERR, "arctech_door: invalid unit range");
		return EXIT_FAILURE;
	} else {
		if(unit == -1 && all == 1) {
			unit = 0;
		}

		arctechDoorCreateMessage(id, unit, state, all);
		arctechDoorCreateStart();
		arctechDoorClearCode();
		arctechDoorCreateId(id);
		arctechDoorCreateAll(all);
		arctechDoorCreateState(state);
		arctechDoorCreateUnit(unit);
		arctechDoorCreateFooter();
	}
	return EXIT_SUCCESS;
}

void arctechDoorPrintHelp(void) {
  printf("\t -t --on\t\t\tsend an on signal\n");
  printf("\t -f --off\t\t\tsend an off signal\n");
  printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
  printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
  printf("\t -a --all\t\t\tsend command to all devices with this id\n");
}

void arctechDoorInit(void) {

	protocol_register(&arctech_door);
	protocol_set_id(arctech_door, "arctech_doors");
	protocol_device_add(arctech_door, "kaku_door", "KlikAanKlikUit Door Sensor");
	protocol_plslen_add(arctech_door, 294);

	arctech_door->devtype = SWITCH;
	arctech_door->hwtype = RF433;
	arctech_door->pulse = 4;
	arctech_door->rawlen = 148;
	arctech_door->lsb = 3;

  options_add(&arctech_door->options, 'a', "all", no_value, 0, NULL);
	options_add(&arctech_door->options, 'u', "unit", has_value, config_id, "^([0-9]{1}|[1][0-5])$");
	options_add(&arctech_door->options, 'i', "id", has_value, config_id, "^([0-9]{1,7}|[1-5][0-9]{7}|6([0-6][0-9]{6}|7(0[0-9]{5}|10([0-7][0-9]{3}|8([0-7][0-9]{2}|8([0-5][0-9]|6[0-3]))))))$");
	options_add(&arctech_door->options, 't', "on", no_value, config_state, NULL);
	options_add(&arctech_door->options, 'f', "off", no_value, config_state, NULL);


	protocol_setting_add_string(arctech_door, "states", "on,off");
	protocol_setting_add_number(arctech_door, "readonly", 1);

	arctech_door->parseBinary=&arctechDoorParseBinary;
	arctech_door->createCode=&arctechDoorCreateCode;
	arctech_door->printHelp=&arctechDoorPrintHelp;
}
