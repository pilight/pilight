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
#include <math.h>

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "arctech_dimmer.h"

static void arctechDimCreateMessage(int id, int unit, int state, int all, int dimlevel) {
	arctech_dimmer->message = json_mkobject();
	json_append_member(arctech_dimmer->message, "id", json_mknumber(id));
	if(all == 1) {
		json_append_member(arctech_dimmer->message, "all", json_mknumber(all));
	} else {
		json_append_member(arctech_dimmer->message, "unit", json_mknumber(unit));
	}
	if(dimlevel >= 0) {
		state = 1;
		json_append_member(arctech_dimmer->message, "dimlevel", json_mknumber(dimlevel));
	}
	if(state == 1) {
		json_append_member(arctech_dimmer->message, "state", json_mkstring("on"));
	} else {
		json_append_member(arctech_dimmer->message, "state", json_mkstring("off"));
	}
}

static void arctechDimParseBinary(void) {
	int dimlevel = binToDecRev(arctech_dimmer->binary, 32, 35);
	int unit = binToDecRev(arctech_dimmer->binary, 28, 31);
	int state = arctech_dimmer->binary[27];
	int all = arctech_dimmer->binary[26];
	int id = binToDecRev(arctech_dimmer->binary, 0, 25);

	arctechDimCreateMessage(id, unit, state, all, dimlevel);
}

static void arctechDimCreateLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		arctech_dimmer->raw[i]=(arctech_dimmer->plslen->length);
		arctech_dimmer->raw[i+1]=(arctech_dimmer->plslen->length);
		arctech_dimmer->raw[i+2]=(arctech_dimmer->plslen->length);
		arctech_dimmer->raw[i+3]=(arctech_dimmer->pulse*arctech_dimmer->plslen->length);
	}
}

static void arctechDimCreateHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		arctech_dimmer->raw[i]=(arctech_dimmer->plslen->length);
		arctech_dimmer->raw[i+1]=(arctech_dimmer->pulse*arctech_dimmer->plslen->length);
		arctech_dimmer->raw[i+2]=(arctech_dimmer->plslen->length);
		arctech_dimmer->raw[i+3]=(arctech_dimmer->plslen->length);
	}
}

static void arctechDimClearCode(void) {
	arctechDimCreateLow(2,147);
}

static void arctechDimCreateStart(void) {
	arctech_dimmer->raw[0]=arctech_dimmer->plslen->length;
	arctech_dimmer->raw[1]=(10*arctech_dimmer->plslen->length);
}

static void arctechDimCreateId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=((length-i)+1)*4;
			arctechDimCreateHigh(106-x, 106-(x-3));
		}
	}
}

static void arctechDimCreateAll(int all) {
	if(all == 1) {
		arctechDimCreateHigh(106, 109);
	}
}

static void arctechDimCreateState(int state) {
	if(state == 1) {
		arctechDimCreateHigh(110, 113);
	} else if(state == -1) {
		arctech_dimmer->raw[110]=(arctech_dimmer->plslen->length);
		arctech_dimmer->raw[111]=(arctech_dimmer->plslen->length);
		arctech_dimmer->raw[112]=(arctech_dimmer->plslen->length);
		arctech_dimmer->raw[113]=(arctech_dimmer->plslen->length);
	}
}

static void arctechDimCreateUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=((length-i)+1)*4;
			arctechDimCreateHigh(130-x, 130-(x-3));
		}
	}
}

static void arctechDimCreateDimlevel(int dimlevel) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(dimlevel, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=((length-i)+1)*4;
			arctechDimCreateHigh(146-x, 146-(x-3));
		}
	}
}

static void arctechDimCreateFooter(void) {
	arctech_dimmer->raw[147]=(PULSE_DIV*arctech_dimmer->plslen->length);
}

static int arctechDimCheckValues(JsonNode *code) {
	int dimlevel = -1;
	int max = 15;
	int min = 0;
	double itmp = -1;

	if(json_find_number(code, "dimlevel-maximum", &itmp) == 0)
		max = (int)round(itmp);
	if(json_find_number(code, "dimlevel-minimum", &itmp) == 0)
		min = (int)round(itmp);
	if(json_find_number(code, "dimlevel", &itmp) == 0)
		dimlevel = (int)round(itmp);

	if(min > max) {
		return 1;
	}

	if(dimlevel != -1) {
		if(dimlevel < min || dimlevel > max) {
			return 1;
		} else {
			return 0;
		}
	}
	return 0;
}

static int arctechDimCreateCode(JsonNode *code) {
	int id = -1;
	int unit = -1;
	int state = -1;
	int all = 0;
	int dimlevel = -1;
	int max = 15;
	int min = 0;
	double itmp = -1;

	if(json_find_number(code, "dimlevel-maximum", &itmp) == 0)
		max = (int)round(itmp);
	if(json_find_number(code, "dimlevel-minimum", &itmp) == 0)
		min = (int)round(itmp);

	if(json_find_number(code, "id", &itmp) == 0)
		id = (int)round(itmp);
	if(json_find_number(code, "unit", &itmp) == 0)
		unit = (int)round(itmp);
	if(json_find_number(code, "dimlevel", &itmp) == 0)
		dimlevel = (int)round(itmp);
	if(json_find_number(code, "all", &itmp) == 0)
		all = (int)round(itmp);

	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;

	if(id == -1 || (unit == -1 && all == 0) || (dimlevel == -1 && state == -1)) {
		logprintf(LOG_ERR, "arctech_dimmer: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 67108863 || id < 1) {
		logprintf(LOG_ERR, "arctech_dimmer: invalid id range");
		return EXIT_FAILURE;
	} else if((unit > 15 || unit < 0) && all == 0) {
		logprintf(LOG_ERR, "arctech_dimmer: invalid unit range");
		return EXIT_FAILURE;
	} else if(dimlevel != -1 && (dimlevel > max || dimlevel < min) ) {
		logprintf(LOG_ERR, "arctech_dimmer: invalid dimlevel range");
		return EXIT_FAILURE;
	} else if(dimlevel >= 0 && state == 0) {
		logprintf(LOG_ERR, "arctech_dimmer: dimlevel and off state cannot be combined");
		return EXIT_FAILURE;
	} else {
		if(unit == -1 && all == 1) {
			unit = 0;
		}
		if(dimlevel >= 0) {
			state = -1;
		}
		arctechDimCreateMessage(id, unit, state, all, dimlevel);
		arctechDimCreateStart();
		arctechDimClearCode();
		arctechDimCreateId(id);
		arctechDimCreateAll(all);
		arctechDimCreateState(state);
		arctechDimCreateUnit(unit);
		if(dimlevel > -1) {
			arctechDimCreateDimlevel(dimlevel);
		}
		arctechDimCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void arctechDimPrintHelp(void) {
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
	printf("\t -a --all\t\t\tsend command to all devices with this id\n");
	printf("\t -d --dimlevel=dimlevel\t\tsend a specific dimlevel\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void arctechDimInit(void) {

	protocol_register(&arctech_dimmer);
	protocol_set_id(arctech_dimmer, "arctech_dimmer");
	protocol_device_add(arctech_dimmer, "kaku_dimmer", "KlikAanKlikUit Dimmers");
	protocol_plslen_add(arctech_dimmer, 300);
	arctech_dimmer->devtype = DIMMER;
	arctech_dimmer->hwtype = RF433;
	arctech_dimmer->pulse = 5;
	arctech_dimmer->rawlen = 148;
	arctech_dimmer->lsb = 3;

	options_add(&arctech_dimmer->options, 'd', "dimlevel", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^([0-9]{1}|[1][0-5])$");
	options_add(&arctech_dimmer->options, 'u', "unit", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]{1}|[1][0-5])$");
	options_add(&arctech_dimmer->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]{1,7}|[1-5][0-9]{7}|6([0-6][0-9]{6}|7(0[0-9]{5}|10([0-7][0-9]{3}|8([0-7][0-9]{2}|8([0-5][0-9]|6[0-3]))))))$");
	options_add(&arctech_dimmer->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&arctech_dimmer->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&arctech_dimmer->options, 'a', "all", OPTION_OPT_VALUE, CONFIG_OPTIONAL, JSON_NUMBER, NULL, NULL);

	options_add(&arctech_dimmer->options, 0, "dimlevel-minimum", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^([0-9]{1}|[1][0-5])$");
	options_add(&arctech_dimmer->options, 0, "dimlevel-maximum", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)15, "^([0-9]{1}|[1][0-5])$");
	options_add(&arctech_dimmer->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	arctech_dimmer->parseBinary=&arctechDimParseBinary;
	arctech_dimmer->createCode=&arctechDimCreateCode;
	arctech_dimmer->printHelp=&arctechDimPrintHelp;
	arctech_dimmer->checkValues=&arctechDimCheckValues;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "arctech_dimmer";
	module->version = "1.1";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	arctechDimInit();
}
#endif
