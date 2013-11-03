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
#include "log.h"
#include "protocol.h"
#include "binary.h"
#include "gc.h"
#include "arctech_screen.h"

void arctechScrCreateMessage(int id, int unit, int state, int all) {
	arctech_screen->message = json_mkobject();
	json_append_member(arctech_screen->message, "id", json_mknumber(id));
	if(all == 1) {
		json_append_member(arctech_screen->message, "all", json_mknumber(all));	
	} else {
		json_append_member(arctech_screen->message, "unit", json_mknumber(unit));
	}
		
	if(state == 1) {
		json_append_member(arctech_screen->message, "state", json_mkstring("up"));
	} else {
		json_append_member(arctech_screen->message, "state", json_mkstring("down"));
	}
}

void arctechScrParseBinary(void) {
	int unit = binToDecRev(arctech_screen->binary, 28, 31);
	int state = arctech_screen->binary[27];
	int all = arctech_screen->binary[26];
	int id = binToDecRev(arctech_screen->binary, 0, 25);

	arctechScrCreateMessage(id, unit, state, all);
}

void arctechScrCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		arctech_screen->raw[i]=(arctech_screen->plslen->length);
		arctech_screen->raw[i+1]=(arctech_screen->plslen->length);
		arctech_screen->raw[i+2]=(arctech_screen->plslen->length);
		arctech_screen->raw[i+3]=(arctech_screen->pulse*arctech_screen->plslen->length);
	}
}

void arctechScrCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		arctech_screen->raw[i]=(arctech_screen->plslen->length);
		arctech_screen->raw[i+1]=(arctech_screen->pulse*arctech_screen->plslen->length);
		arctech_screen->raw[i+2]=(arctech_screen->plslen->length);
		arctech_screen->raw[i+3]=(arctech_screen->plslen->length);
	}
}

void arctechScrClearCode(void) {
	arctechScrCreateLow(2, 132);
}

void arctechScrCreateStart(void) {
	arctech_screen->raw[0]=(arctech_screen->plslen->length);
	arctech_screen->raw[1]=(9*arctech_screen->plslen->length);
}

void arctechScrCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=((length-i)+1)*4;
			arctechScrCreateHigh(106-x, 106-(x-3));
		}
	}
}

void arctechScrCreateAll(int all) {
	if(all == 1) {
		arctechScrCreateHigh(106, 109);
	}
}

void arctechScrCreateState(int state) {
	if(state == 1) {
		arctechScrCreateHigh(110, 113);
	}
}

void arctechScrCreateUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=((length-i)+1)*4;
			arctechScrCreateHigh(130-x, 130-(x-3));
		}
	}
}

void arctechScrCreateFooter(void) {
	arctech_screen->raw[131]=(PULSE_DIV*arctech_screen->plslen->length);
}

int arctechScrCreateCode(JsonNode *code) {
	int id = -1;
	int unit = -1;
	int state = -1;
	int all = 0;
	char *tmp;

	if(json_find_string(code, "id", &tmp) == 0)
		id=atoi(tmp);
	if(json_find_string(code, "down", &tmp) == 0)
		state=0;
	else if(json_find_string(code, "up", &tmp) == 0)
		state=1;
	if(json_find_string(code, "unit", &tmp) == 0)
		unit = atoi(tmp);
	if(json_find_string(code, "all", &tmp) == 0)
		all = 1;
		
	if(id == -1 || (unit == -1 && all == 0) || state == -1) {
		logprintf(LOG_ERR, "arctech_screen: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 67108863 || id < 1) {
		logprintf(LOG_ERR, "arctech_screen: invalid id range");
		return EXIT_FAILURE;
	} else if((unit > 15 || unit < 0) && all == 0) {
		logprintf(LOG_ERR, "arctech_screen: invalid unit range");
		return EXIT_FAILURE;
	} else {
		if(unit == -1 && all == 1) {
			unit = 0;
		}
		arctechScrCreateMessage(id, unit, state, all);
		arctechScrCreateStart();
		arctechScrClearCode();
		arctechScrCreateId(id);
		arctechScrCreateAll(all);
		arctechScrCreateState(state);
		arctechScrCreateUnit(unit);
		arctechScrCreateFooter();
	}
	return EXIT_SUCCESS;
}

void arctechScrPrintHelp(void) {
	printf("\t -t --up\t\t\tsend an up signal\n");
	printf("\t -f --down\t\t\tsend an down signal\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
	printf("\t -a --all\t\t\tsend command to all devices with this id\n");
}

void arctechScrInit(void) {

	protocol_register(&arctech_screen);
	arctech_screen->id = malloc(18);
	strcpy(arctech_screen->id, "archtech_screens");
	protocol_device_add(arctech_screen, "kaku_screen", "KlikAanKlikUit Switches");
	protocol_conflict_add(arctech_screen, "archtech_switches");
	protocol_plslen_add(arctech_screen, 303);
	protocol_plslen_add(arctech_screen, 251);
	arctech_screen->type = SCREEN;
	arctech_screen->pulse = 5;
	arctech_screen->rawlen = 132;
	arctech_screen->lsb = 3;

	options_add(&arctech_screen->options, 'a', "all", no_value, 0, NULL);
	options_add(&arctech_screen->options, 't', "up", no_value, config_state, NULL);
	options_add(&arctech_screen->options, 'f', "down", no_value, config_state, NULL);
	options_add(&arctech_screen->options, 'u', "unit", has_value, config_id, "^([0-9]{1}|[1][0-5])$");
	options_add(&arctech_screen->options, 'i', "id", has_value, config_id, "^([0-9]{1,7}|[1-5][0-9]{7}|6([0-6][0-9]{6}|7(0[0-9]{5}|10([0-7][0-9]{3}|8([0-7][0-9]{2}|8([0-5][0-9]|6[0-3]))))))$");

	protocol_setting_add_string(arctech_screen, "states", "up,down");
	protocol_setting_add_number(arctech_screen, "readonly", 0);
	
	arctech_screen->parseBinary=&arctechScrParseBinary;
	arctech_screen->createCode=&arctechScrCreateCode;
	arctech_screen->printHelp=&arctechScrPrintHelp;
}
