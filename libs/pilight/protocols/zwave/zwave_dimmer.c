/*
	Copyright (C) 2015 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#
#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../../hardware/zwave.h"
#include "../protocol.h"
#include "zwave_dimmer.h"

static void createDimMessage(char *message, unsigned int homeId, int nodeId, int dimlevel) {
	int x = snprintf(message, 255, "{\"homeId\":%d,", homeId);
	x += snprintf(&message[x], 255-x, "\"nodeId\":%d,", nodeId);
	if(dimlevel > 0) {
		x += snprintf(&message[x], 255-x, "\"state\":\"on\",");
	}	else {
		x += snprintf(&message[x], 255-x, "\"state\":\"off\",");
	}
	x += snprintf(&message[x], 255-x, "\"dimlevel\":%d", dimlevel);
	x += snprintf(&message[x], 255-x, "}");
}

static void createConfigMessage(char *message, unsigned int homeId, int nodeId, char *label, int value) {
	int x = snprintf(message, 255, "{\"homeId\":%d,", homeId);
	x += snprintf(&message[x], 255-x, "\"nodeId\":%d,", nodeId);
	x += snprintf(&message[x], 255-x, "\"value\":%d", value);
	x += snprintf(&message[x], 255-x, "\"value\":\"%s\"", label);
	x += snprintf(&message[x], 255-x, "}");
}

static void parseCommand(struct JsonNode *code, char *message) {
	struct JsonNode *jmsg = NULL;
	char *label = NULL;
	double itmp = 0.0;
	int nodeId = 0, value = 0, cmdId = 0;
	unsigned int homeId = 0;

	if((jmsg = json_find_member(code, "message")) != NULL) {
		if(json_find_number(jmsg, "nodeId", &itmp) == 0) {
			nodeId = (int)round(itmp);
		} else {
			return;
		}
		if(json_find_number(jmsg, "homeId", &itmp) == 0) {
			homeId = (unsigned int)round(itmp);
		} else {
			return;
		}
		if(json_find_number(jmsg, "cmdId", &itmp) == 0) {
			cmdId = (int)round(itmp);
		} else {
			return;
		}
		if(json_find_number(jmsg, "value", &itmp) == 0) {
			value = (int)round(itmp);
		} else {
			return;
		}
		json_find_string(jmsg, "label", &label);
	}

	if(cmdId == COMMAND_CLASS_SWITCH_MULTILEVEL) {
		createDimMessage(message, homeId, nodeId, value);
	}
	if(cmdId == COMMAND_CLASS_CONFIGURATION && label != NULL) {
		createConfigMessage(message, homeId, nodeId, label, value);
	}
}

static int createCode(struct JsonNode *code, char *message) {
	char out[255];
	unsigned int homeId = 0;
	int nodeId = 0;
	int dimlevel = -1;
	char state = 0;
	double itmp = 0.0;

	if(json_find_number(code, "homeId", &itmp) == 0)
		homeId = (unsigned int)round(itmp);
	if(json_find_number(code, "nodeId", &itmp) == 0)
		nodeId = (int)round(itmp);
	if(json_find_number(code, "dimlevel", &itmp) == 0)
		dimlevel = (int)round(itmp);

	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;

	if(dimlevel > -1) {
		state = 1;
		if(dimlevel > 100) {
			dimlevel = 99;
		}
	} else if(state == 0) {
		dimlevel = 0;
	} else if(state == 1) {
		dimlevel = 255;
	}

	if(dimlevel > -1) {
		snprintf(out, 255, "%d", dimlevel);
		zwaveSetValue(nodeId, COMMAND_CLASS_SWITCH_MULTILEVEL, "Level", out);
		createDimMessage(message, homeId, nodeId, dimlevel);
	}

	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -h --homeid=id\t\t\tcontrol a device with this home id\n");
	printf("\t -i --unitid=id\t\t\tcontrol a device with this unit id\n");
	printf("\t -d --dimlevel=dimlevel\t\tsend a specific dimlevel\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void zwaveDimmerInit(void) {

	protocol_register(&zwave_dimmer);
	protocol_set_id(zwave_dimmer, "zwave_dimmer");
	protocol_device_add(zwave_dimmer, "zwave_dimmer", "Z-Wave Dimmers");
	zwave_dimmer->devtype = DIMMER;
	zwave_dimmer->hwtype = ZWAVE;

	options_add(&zwave_dimmer->options, "h", "homeId", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&zwave_dimmer->options, "i", "nodeId", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&zwave_dimmer->options, "t", "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&zwave_dimmer->options, "f", "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&zwave_dimmer->options, "d", "dimlevel", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);

	options_add(&zwave_dimmer->options, "0", "dimlevel-minimum", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, NULL);
	options_add(&zwave_dimmer->options, "0", "dimlevel-maximum", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)99, NULL);

	zwave_dimmer->createCode=&createCode;
	zwave_dimmer->parseCommand=&parseCommand;
	zwave_dimmer->printHelp=&printHelp;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "zwave_dimmer";
	module->version = "2.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	zwaveDimmerInit();
}
#endif
