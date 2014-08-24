/*


    This file is part of pilight.

    pilight is free software: you can redistribute it and/or modify it under the
    terms of the GNU General Public License as published by the Free Software
    Foundation, either version 3 of the License, or (at your option) any later
    version.

    pilight is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
    A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with pilight. If not, see    <http://www.gnu.org/licenses/>
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
#include "beamish_switch.h"


static void beamishSwCreateMessage(int id, int state, int systemcode, int unit, int all) {
	beamish_switch->message = json_mkobject();

 	json_append_member(beamish_switch->message, "systemcode", json_mknumber(systemcode));
        json_append_member (beamish_switch->message, "id", json_mknumber(id));

        if(all==1) {
                json_append_member(beamish_switch->message, "all", json_mknumber(all));
        } else {
                json_append_member(beamish_switch->message, "unit", json_mknumber(unit));
		if (state == 0) { //emulate one button state operation
			state = 1;
		} else {
			if (state == 1) state = 0;
		}
        }
        if (state==0)   json_append_member(beamish_switch->message, "state", json_mkstring("off"));
        if (state==1)   json_append_member(beamish_switch->message, "state", json_mkstring("on"));
}

static void beamishSwParseCode(void) {
	int x = 0;
	int state = -1;
	int all = 0;
	int lookup_code_button [16]={0,0,0,0,0,0,0,4,0,6,0,3,5,2,1,0}; // 12-All ON, 9-All OFF

	/* Convert the one's and zero's into binary */
	for(x=0; x<beamish_switch->rawlen; x+=4) {

		if(beamish_switch->code[x+1] == 1 && beamish_switch->code[x+3] == 1) {
			beamish_switch->binary[x/4]=1;
		} else {
			beamish_switch->binary[x/4]=0;
		}
	}

	int systemcode = binToDec(beamish_switch->binary, 0, 3);
	int id = binToDec(beamish_switch->binary, 4, 7);
	int unit = lookup_code_button[binToDec(beamish_switch->binary, 8, 11)];
	if (unit == 5) {
		state = 1; 
		all = 1;
	}
	if (unit == 6) {
		state = 0;
		all = 1;
	}
	beamishSwCreateMessage(id, state, systemcode, unit, all);
}

static void beamishSwCreateLow(int s, int e) {
    int i;

    for(i=s;i<=e;i+=4) {
        beamish_switch->raw[i]=beamish_switch->plslen->length;
        beamish_switch->raw[i+1]=(beamish_switch->pulse*beamish_switch->plslen->length);
        beamish_switch->raw[i+2]=(beamish_switch->pulse*beamish_switch->plslen->length);
        beamish_switch->raw[i+3]=beamish_switch->plslen->length;
    }
}

static void beamishSwCreateHigh(int s, int e) {
    int i;

    for(i=s;i<=e;i+=4) {
        beamish_switch->raw[i]=beamish_switch->plslen->length;
        beamish_switch->raw[i+1]=(beamish_switch->pulse*beamish_switch->plslen->length);
        beamish_switch->raw[i+2]=beamish_switch->plslen->length;
        beamish_switch->raw[i+3]=(beamish_switch->pulse*beamish_switch->plslen->length);
    }
}

static void beamishSwClearCode(void) {
	beamishSwCreateLow(0,47);
}

static void beamishSwCreateSystemCode(int systemcode) {
	beamishSwCreateHigh((systemcode-1)*4, (systemcode-1)*4+3);
}

static void beamishSwCreateIdCode(int id) {
	beamishSwCreateHigh(16+(id-1)*4, 16+(id-1)*4+3);
}

static void beamishSwCreateUnitCode(int unit) {
	beamishSwCreateHigh(32+(unit-1)*4, 32+(unit-1)*4+3);
}

static void beamishSwCreateFooter(void) {
	beamish_switch->raw[48]=(beamish_switch->plslen->length);
	beamish_switch->raw[49]=(PULSE_DIV*beamish_switch->plslen->length);
}

static int beamishSwCreateCode(JsonNode *code) {
	int systemcode = -1;
	int id = -1;
	int unit = -1;
	int state = -1;
	int all = -1;
	double itmp = -1;
	int lookup_button_code [7]={0,14,13,11,7,12,9}; // Button A-D, ALL ON, ALL OFF

	if(json_find_number(code, "on",  &itmp) == 0) state = 1;
	if(json_find_number(code, "off",  &itmp) == 0) state = 0;
	if(json_find_number(code, "all",  &itmp) == 0) all = 1;
	if(json_find_number(code, "systemcode",  &itmp) == 0) systemcode = (int)round(itmp);
	if(json_find_number(code, "id", &itmp) == 0) id = (int)round(itmp);
	if(json_find_number(code, "unit",  &itmp) == 0) unit = (int)round(itmp);

	if(id == -1 || (unit == -1 && all == -1) || systemcode == -1 || id == -1 || state == -1) {
		logprintf(LOG_ERR, "beamish_switch: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(systemcode > 15 || systemcode < 0) {
		logprintf(LOG_ERR, "beamish_switch: invalid systemcode range");
		return EXIT_FAILURE;
	} else if(id > 15 || id < 0) {
		logprintf(LOG_ERR, "beamish_switch: invalid id range");
		return EXIT_FAILURE;
	} else if((unit > 4 || unit < 1) && all == 0){
		logprintf(LOG_ERR, "beamish_switch: invalid unitcode range");
		return EXIT_FAILURE;
	} else {
		if (all == 1 && state == 1) unit = 5;
		if (all == 1 && state == 0) unit = 6;

		beamishSwCreateMessage(id, state, systemcode, unit, all);
		beamishSwClearCode();
		beamishSwCreateSystemCode(systemcode);
		beamishSwCreateIdCode(id);
		unit = lookup_button_code[unit]; // translate into switch device known format
		beamishSwCreateUnitCode(unit);
		beamishSwCreateFooter();
	}
	return EXIT_SUCCESS;
}

static void beamishSwPrintHelp(void) {
	printf("\t -t --on=on\t\t\tsend on to all units / toggle state command for unit\n");
	printf("\t -f --off=off\t\t\tsend off to all units / toggle state command for unit\n");
	printf("\t -a --all=all\t\t\tsend on/off command to all units\n");
	printf("\t -s --systemcode=systemcode\t\tcontrol devices members of this systemcode\n");
	printf("\t -i --id=id\t\t\tcontrol device members with this id code\n");
	printf("\t -u --unit=unit\t\t\tcontrol the individual device unit\n");

}

#ifndef MODULE
__attribute__((weak))
#endif
void beamishSwInit(void) {

	protocol_register(&beamish_switch);
	protocol_set_id(beamish_switch, "beamish_switch");
	protocol_device_add(beamish_switch, "beamish_switch", "Beamish Switches 4-A4E");
	protocol_plslen_add(beamish_switch, 232);
	beamish_switch->devtype = SWITCH;
	beamish_switch->hwtype = RF433;
	beamish_switch->pulse = 3;
	beamish_switch->rawlen = 50;
	beamish_switch->binlen = 12;

        options_add(&beamish_switch->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
        options_add(&beamish_switch->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
	options_add(&beamish_switch->options, 's', "systemcode", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([1-9]|1[0-5])$");
	options_add(&beamish_switch->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([1-9]|[1[0-5])$");
	options_add(&beamish_switch->options, 'u', "unit", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([1-4]{1})$");
        options_add(&beamish_switch->options, 'a', "all", OPTION_NO_VALUE, CONFIG_SETTING, JSON_NUMBER, NULL, NULL);

	options_add(&beamish_switch->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	beamish_switch->parseCode=&beamishSwParseCode;
	beamish_switch->createCode=&beamishSwCreateCode;
	beamish_switch->printHelp=&beamishSwPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "beamish_switch";
	module->version = "0.2";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	beamish_SwInit();
}
#endif
