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
#include "zwave_ctrl.h"

static void createCommandMessage(char *message, int state) {
	if(state == 1) {
		snprintf(message, 255, "{\"command\":\"inclusion\"}");
	} else if(state == 0) {
		snprintf(message, 255, "{\"command\":\"exclusion\"}");
	} else if(state == 2) {
		snprintf(message, 255, "{\"command\":\"stop\"}");
	} else if(state == 3) {
		snprintf(message, 255, "{\"command\":\"soft-reset\"}");
	}
}

static void createParameterMessage(char *message, int nodeid, int parameter, int value) {
	int x = snprintf(message, 255, "{\"nodeid\":%d,", nodeid);
	x += snprintf(&message[x], 255-x, "\"parameter\":%d,", parameter);
	if(value >= 0) {
		x += snprintf(&message[x], 255-x, "\"value\":%d,", value);
	}
	x += snprintf(&message[x-1], 255-x, "}");
}

static int createCode(struct JsonNode *code, char *message) {
	printf("%s\n", json_stringify(code, "\t"));
	int state = -1;
	int parameter = -1;
	int value = -1;
	int nodeid = -1;
	double itmp = 0.0;

	if(json_find_number(code, "inclusion", &itmp) == 0)
		state=1;
	else if(json_find_number(code, "exclusion", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "stop", &itmp) == 0)
		state=2;
	else if(json_find_number(code, "soft-reset", &itmp) == 0)
		state=3;

	if(json_find_number(code, "parameter", &itmp) == 0)
		parameter = (int)itmp;
	if(json_find_number(code, "nodeid", &itmp) == 0)
		nodeid = (int)itmp;
	if(json_find_number(code, "value", &itmp) == 0)
		value = (int)itmp;

	if(parameter >= 0 && nodeid == -1) {
		logprintf(LOG_ERR, "zwave_ctrl: parameter needs a nodeid");
		return EXIT_FAILURE;
	}
	if(nodeid >= 0 && parameter == -1) {
		logprintf(LOG_ERR, "zwave_ctrl: nodeid needs a parameter");
		return EXIT_FAILURE;
	}
	if(value >= 0 && (parameter == -1 || nodeid == -1)) {
		logprintf(LOG_ERR, "zwave_ctrl: value needs a parameter and nodeid");
		return EXIT_FAILURE;
	}
	if(state > -1 && (parameter > -1 || nodeid > -1)) {
		logprintf(LOG_ERR, "zwave_ctrl: state and parameter and node cannot be combined");
		return EXIT_FAILURE;
	}

	if(pilight.process == PROCESS_DAEMON) {
		if(parameter >= 0 && nodeid >= 0) {
			if(value >= 0) {
				int x = snprintf(NULL, 0, "%d", value);
				zwaveSetConfigParam(nodeid, parameter, value, x);
			} else {
				zwaveGetConfigParam(nodeid, parameter);
			}
		}
		if(state == 1) {
			zwaveStartInclusion();
		} else if(state == 0) {
			zwaveStartExclusion();
		} else if(state == 2) {
			zwaveStopCommand();
		} else if(state == 3) {
			zwaveSoftReset();
		}
	}
	if(parameter >= 0 && nodeid >= 0) {
		createParameterMessage(message, nodeid, parameter, value);
	} else if(state > 0) {
		createCommandMessage(message, state);
	}

	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -i --inclusion\t\t\tsend an inclusion command\n");
	printf("\t -e --exclusion\t\t\tsend an exclusion command\n");
	printf("\t -s --stop\t\t\tstop previous command\n");
	printf("\t -r --soft-reset\t\tsend a soft-reset command\n");
	printf("\t -c --parameter\t\tset or retrieve parameter\n");
	printf("\t -v --value=X\t\tset parameter to this value\n");
	printf("\t -n --nodeid=X\t\tset parameter to this value\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void zwaveCtrlInit(void) {

	protocol_register(&zwave_ctrl);
	protocol_set_id(zwave_ctrl, "zwave_ctrl");
	protocol_device_add(zwave_ctrl, "zwave_ctrl", "Z-Wave Controller");
	zwave_ctrl->devtype = SWITCH;
	zwave_ctrl->hwtype = ZWAVE;
	zwave_ctrl->config = 0;

	options_add(&zwave_ctrl->options, "i", "inclusion", OPTION_NO_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&zwave_ctrl->options, "e", "exclusion", OPTION_NO_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&zwave_ctrl->options, "s", "stop", OPTION_NO_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&zwave_ctrl->options, "r", "soft-reset", OPTION_NO_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&zwave_ctrl->options, "c", "parameter", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zwave_ctrl->options, "v", "value", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zwave_ctrl->options, "n", "nodeid", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);

	zwave_ctrl->createCode=&createCode;
	zwave_ctrl->printHelp=&printHelp;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "zwave_ctrl";
	module->version = "2.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	zwaveCtrlInit();
}
#endif
