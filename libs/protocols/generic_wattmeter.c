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
#include "generic_wattmeter.h"

//void genWattmeterCreateMessage(int id, int watt, int price, char *coin) {
void genWattmeterCreateMessage(int id, int watt) {
	generic_wattmeter->message = json_mkobject();
	json_append_member(generic_wattmeter->message, "id", json_mknumber(id));
	if(watt > -999) {
		json_append_member(generic_wattmeter->message, "watt", json_mknumber(watt));
	}
	
	/* no pasamos ni el precio ni el tipo de moneda porque son datos de configuracion fijos
	if(price > -999) {
		json_append_member(generic_wattmeter->message, "price", json_mknumber(price));
	}
	if(coin[0] != '\0') {
		json_append_member(generic_wattmeter->message, "coin", json_mkstring(coin));
	}*/
}

int genWattmeterCreateCode(JsonNode *code) {
	int id = -999;
	int watt = -999;
	//int price = -999;
	//int coin = -1;
        //char *tmp;
        
	json_find_number(code, "id", &id);
	json_find_number(code, "watt", &watt);
	//json_find_number(code, "price", &price);
	//if(json_find_string(code, "coin", &tmp) == 0)
	//	coin = atoi(tmp);

	//if(id == -999 && watt == -999 && price == -999 && coin == -1) {
	if(id == -999 && watt == -999) {
		logprintf(LOG_ERR, "generic_wattmeter: insufficient number of arguments");
		return EXIT_FAILURE;
	} else {
		genWattmeterCreateMessage(id, watt);
	}
	return EXIT_SUCCESS;
}

void genWattmeterPrintHelp(void) {
	printf("\t -w --watt=watt\tset the watts\n");
	//printf("\t -m --price=price\t\tset the price\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

void genWattmeterInit(void) {
	
	protocol_register(&generic_wattmeter);
	protocol_set_id(generic_wattmeter, "generic_wattmeter");
	protocol_device_add(generic_wattmeter, "generic_wattmeter", "Generic wattmeter");
	generic_wattmeter->devtype = WATTMETER;

	//Variables
	options_add(&generic_wattmeter->options, 'w', "watt", has_value, config_value, "[0-9]");
	options_add(&generic_wattmeter->options, 'm', "price", has_value, config_value, "^[0-9]*(?:\.[0-9]*)?$");
	options_add(&generic_wattmeter->options, 'c', "coin", has_value, config_value, "[^~,]");
	
	//Constantes
	options_add(&generic_wattmeter->options, 'i', "id", has_value, config_id, "[0-9]");

	protocol_setting_add_number(generic_wattmeter, "decimals", 2);	
	//protocol_setting_add_number(generic_wattmeter, "watt", 1);
	//protocol_setting_add_number(generic_wattmeter, "price", 1);

	generic_wattmeter->printHelp=&genWattmeterPrintHelp;
	generic_wattmeter->createCode=&genWattmeterCreateCode;
}
