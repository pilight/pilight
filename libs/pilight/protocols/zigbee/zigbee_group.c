/*
 * zigbee_group.c
 *
 *  Created on: Jan 8, 2018
 *      Author: ma-ca
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/pilight.h"
#include "../../core/dso.h"
#include "../../core/common.h"
#include "../../core/log.h"
#include "../../hardware/zigbee.h"
#include "../protocol.h"
#include "zigbee_group.h"

static struct zigbee_data_t *zigbee_data = NULL;
static struct zigbee_cmd_t *zigbee_commands = NULL;

static void *addDevice(int reason, void *param) {
	zigbee_data = addZigbeeDevice(param, zigbee_data, zigbee_commands, zigbee_group->id);

	return NULL;
}

static void parseCommand(struct JsonNode *code, char **pmessage) {

}

static int createCode(struct JsonNode *code, char **pmessage) {
	char *message = *pmessage;
	char command[MAX_STRLENGTH];
	char *stmp = NULL;
	double itmp = 0.0;
	int shortAddr = -1;
	int endpoint = -1;
	int dimlevel = -1;
    snprintf(message, 255, "{}");

    if (json_find_string(code, "shortaddr", &stmp) == 0) {
    	sscanf(stmp, "%x", &shortAddr);
    }
    if (json_find_number(code, "shortaddr", &itmp) == 0) {
    	shortAddr = (int) itmp;
    }
    if (json_find_number(code, "endpoint", &itmp) == 0) {
    	endpoint = (int) itmp;
    }

	if(json_find_number(code, "dimlevel", &itmp) == 0) {
		dimlevel = (int) itmp;
	}

	if (shortAddr == -1 || endpoint == -1) {
        logprintf(LOG_ERR, "zigbee_group: insufficient number of arguments");
        printf("zigbee_group: insufficient number of arguments\n");
        printf("code %s stmp = %s, shortAddr = %x, endpoint = %d\n", json_stringify(code, "\t"), stmp, shortAddr, endpoint);
        return EXIT_FAILURE;
    }

    /*
     * zclcmdgroup  <shortaddr> <endpoint> <clusterId> <commandId>[payload]
     *
     * 0x0006 = ONOFF_CLUSTER_ID
     * 0x0008 = LEVEL_CONTROL_CLUSTER_ID
     * 0x0005 = SCENES_CLUSTER
     *
     *
     * Example:
     * zclcmdgrp 0x0001 255 0x0005 05010001
     *
     * on group 0x0001, all endpoints 255=0xFF, cluster 0x0005, command 05=recall scene, sceneid 01,
     *
     */

    if (pilight.process == PROCESS_DAEMON) {

        if (dimlevel > -1) {  // recall sceneid = dimlevel on cluster 0x0005
        		int groupaddr = shortAddr;
        		char *a = (char *) &groupaddr;
        		char b = a[0]; // payload is little endian
        		a[0] = a[1];   // swap bytes order
        		a[1] = b;
        		snprintf(command, MAX_STRLENGTH, "zclcmdgrp 0x%04X %d 0x0005 05%04X%02X",
        				shortAddr, endpoint, groupaddr, dimlevel);
        		zigbeeSendCommand(command);   // recall scene on group command
        } else {
        	logprintf(LOG_ERR, "[ZigBee] zigbee_dimmer insufficient number of arguments");
        	printf("zigbee_dimmer: insufficient number of arguments\n");
        	return EXIT_FAILURE;
        }

    }

    return EXIT_SUCCESS;
}

static void gc(void) {
	struct zigbee_cmd_t *ctmp = NULL;
	while(zigbee_commands) {
		ctmp = zigbee_commands;
		if (ctmp->command != NULL) {
			FREE(ctmp->command);
		}
		zigbee_commands = zigbee_commands->next;
		FREE(ctmp);
	}
	if (zigbee_commands != NULL) {
		FREE(zigbee_commands);
	}
}

static int checkValues(JsonNode *code) {
	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -d --dimlevel=dimlevel/sceneid\t\trecall a sceneid on group\n");
	printf("\t -s --shortaddr=id\t\tgroup address\n");
	printf("\t -e --endpoint=id\t\tendpoint id (use 255=0xFF for all endpoints\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void zigbeeGroupInit(void) {
    protocol_register(&zigbee_group);
    protocol_set_id(zigbee_group, "zigbee_group");
    protocol_device_add(zigbee_group, "zigbee_group", "ZigBee Group Cluster");
    zigbee_group->devtype = DIMMER;
    zigbee_group->hwtype = ZIGBEE;

	options_add(&zigbee_group->options, 's', "shortaddr", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, NULL);
	options_add(&zigbee_group->options, 'e', "endpoint", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_group->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_group->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_group->options, 'd', "dimlevel", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);


	options_add(&zigbee_group->options, 0, "dimlevel-minimum", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, NULL);
	options_add(&zigbee_group->options, 0, "dimlevel-maximum", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)15, NULL);

	options_add(&zigbee_group->options, 0, "extaddr", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, "", NULL);
	options_add(&zigbee_group->options, 0, "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]{1,5}"); // allow 0 - 99999 seconds

	zigbee_group->parseCommand=&parseCommand;
	zigbee_group->createCode=&createCode;
	zigbee_group->printHelp=&printHelp;
	zigbee_group->gc=&gc;
	zigbee_group->checkValues=&checkValues;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice);
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
    module->name = "zigbee_group";
    module->version = "0.1";
    module->reqversion = "7.0";
    module->reqcommit = NULL;
}

void init(void) {
	zigbeeGroupInit();
}
#endif
