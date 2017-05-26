/*
 * zigbee_dimmer.c
 *
 *  Created on: Oct 28, 2016
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
#include "zigbee_dimmer.h"

static struct zigbee_data_t *zigbee_data = NULL;
static struct zigbee_cmd_t *zigbee_commands = NULL;

static void *addDevice(int reason, void *param) {
	zigbee_data = addZigbeeDevice(param, zigbee_data, zigbee_commands, zigbee_dimmer->id);

	return NULL;
}

static void createStateMessage(char *message, int shortAddr, int endpointId, int nstate, int level) {
	int state = nstate;
	if (nstate > -1 ) {
		state = newstate(shortAddr, endpointId, nstate, zigbee_data);
	}

	int x = snprintf(message, 255, "{\"shortaddr\":\"0x%04X\",", shortAddr);
	x += snprintf(&message[x], 255-x, "\"endpoint\":%d,", endpointId);
	if (state == 1) {
		x += snprintf(&message[x], 255-x, "\"state\":\"on\",");
	} else if (state == 0) {
		x += snprintf(&message[x], 255-x, "\"state\":\"off\",");
	}
	if (level > -1) {
		x += snprintf(&message[x], 255-x, "\"dimlevel\": %d,", level);
	}
	x += snprintf(&message[x-1], 255-x, "}");
}

static void parseCommand(struct JsonNode *code, char **pmessage) {
	char *message = *pmessage;
	struct zigbee_data_t *tmp = NULL;
	struct JsonNode *jmsg = NULL;
	char *stmp = NULL;
	char extaddr[19];  // "0x" + 64-Bit extended Address (16 Bytes hex String) + '\0'
	char profileid[7]; // ZigBee Home Automation 0x0104 // ZigBee Light Link 0xC05E
	int shortaddr = -1, endpoint = -1, state = -1, dimlevel = -1, cluster = -1;
	int attrid = -1, attrval = -1, oldstate = -1;

	memset(extaddr, '\0', sizeof(extaddr));
	memset(profileid, '\0', sizeof(profileid));

	if((jmsg = json_find_member(code, "message")) != NULL) {
		if (json_find_string(jmsg, "zigbee_response", &stmp) == 0) { // response from raspbee
			// OnOff attribute on Cluster 0x0006 // attribute id 0x0000 // attribute type 0x10 (bool)
			// Example stmp = "<-ZCL attribute report 0x84182600xxxxxxxx 0x0006 3 00 00 10 00 "
			if (strncmp(stmp, "<-ZCL attribute report ", strlen("<-On/Off Attr Report:")) == 0) {
				if (sscanf(stmp, "<-ZCL attribute report %s 0x0006 %d 00 00 10 %x", extaddr, &endpoint, &state) < 3) {
					return;
				}
				tmp = zigbee_data;
				while(tmp) {
					if (tmp->extaddr != NULL
							&& strcmp(extaddr, tmp->extaddr) == 0
							&& strcmp(tmp->protocol, zigbee_dimmer->id) == 0) {
						shortaddr = (int) tmp->shortaddr;
						break;
					}
					tmp = tmp->next;
				}
				if (shortaddr == -1) { // device not found
					return;
				}
			}
			//  Example stmp = "<-APS attr 0x00178801xxxxxxxx 11 0x0006 0x0000 0x10 00"  // OnOff
			//  Example stmp = "<-APS attr 0x00178801xxxxxxxx 11 0x0008 0x0000 0x20 10"  // Level_Control
			if (strncmp(stmp, "<-APS attr ", strlen("<-APS attr ")) == 0) {
				if (sscanf(stmp, "<-APS attr %s %d %x 0x0000 %*x %x", extaddr, &endpoint, &cluster, &attrval) < 4) {
					return;
				}
				tmp = zigbee_data;
				while(tmp) {
//					logprintf(LOG_DEBUG, "zigbee_dimmer %s", tmp->extaddr);
					if (tmp->extaddr != NULL
							&& strcmp(extaddr, tmp->extaddr) == 0
							&& tmp->endpoint == endpoint
							&& strcmp(tmp->protocol, zigbee_dimmer->id) == 0) {
						shortaddr = (int) tmp->shortaddr;
						oldstate = tmp->currentstate;
						break;
					}
					tmp = tmp->next;
				}
				if (shortaddr == -1) {  // device not found
					return;
				}
				if (cluster == 0x0006) {
					state = attrval;
				} else if (cluster == 0x0008) {
					dimlevel = attrval;
				}
			}
			// discovery response from Cluster 0x0008 - part 1
			// Example stmp = "<-ZDP match 0x84182600xxxxxxxx 0xD19C   3 0x0008 0xC05E"
			if (strncmp(stmp, "<-ZDP match ", strlen("<-ZDP match ")) == 0) {
				if (sscanf(stmp, "<-ZDP match %s %x %d 0x0008 %s", extaddr, &shortaddr, &endpoint, profileid) < 4) {
					return;
				}
				matchResponse(extaddr, shortaddr, endpoint, 0x0008 /* cluster */, profileid, zigbee_dimmer->id);
			}
			// discovery response from Cluster 0x0008 - part 2 --> read ZigBee Basic Cluster Attributes 4,5,6 and 0x4000
			// Example stmp = "<-ZCL attr 0xD19C 3 0x0005 Classic B40 TW - LIGHTIFY" // attribute 0x0005
			if (strncmp(stmp, "<-ZCL attr ", strlen("<-ZCL attr ")) == 0) {
				char attr[33];
				memset(attr, '\0', sizeof(attr));
				if (sscanf(stmp, "<-ZCL attr %x %d 0x0008 %x %[^\n]", &shortaddr, &endpoint, &attrid, attr) < 4) {
					return;
				}
				matchAttributeResponse(shortaddr, endpoint, 0x0008 /* cluster */, attrid, attr, zigbee_dimmer->id);
			}
		}      // end zigbee_response
	} else {   // end message
		return; // no message
	}

	if (shortaddr == -1 || endpoint == -1 || ( dimlevel == -1 && state == -1)) {
		return;
	}

	createStateMessage(message, shortaddr, endpoint, state, dimlevel);

	if (dimlevel == -1 && oldstate == newstate(shortaddr, endpoint, state, zigbee_data)) {
		return; // only broadcast if state has changed or dimlevel has changed
	}
	broadcast_message(message, zigbee_dimmer->id);
}

static int createCode(struct JsonNode *code, char **pmessage) {
	char *message = *pmessage;
	char command[MAX_STRLENGTH];
	char *stmp = NULL;
	double itmp = 0.0;
	int shortAddr = -1;
	int endpoint = -1, state = -1;
	int dimlevel = -1, colortemp = -1;
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
	if (json_find_number(code, "off", &itmp) == 0) {
		state = 0;
	}
	else if (json_find_number(code, "on", &itmp) == 0) {
		state = 1;
	}
	else if (json_find_number(code, "toggle", &itmp) == 0) {
		state = 2;
	}
	if(json_find_number(code, "dimlevel", &itmp) == 0) {
		dimlevel = (int) itmp;
	}
	if(json_find_number(code, "colortemp", &itmp) == 0) {
		colortemp = (int) itmp;
	}
	if (json_find_number(code, "autodiscover", &itmp) == 0) {
		if (pilight.process != PROCESS_DAEMON) {
			printf("ZigBee starting discovery\n");
			return EXIT_SUCCESS;
		}
	} else if (shortAddr == -1 || endpoint == -1) {
        logprintf(LOG_ERR, "zigbee_dimmer: insufficient number of arguments");
        printf("zigbee_dimmer: insufficient number of arguments\n");
        printf("code %s stmp = %s, shortAddr = %x, endpoint = %d\n", json_stringify(code, "\t"), stmp, shortAddr, endpoint);
        return EXIT_FAILURE;
    }

    /*
     * zclattr <shortaddr> <endpoint> <clusterid> <commandid><attributeid|payload>
     * zclcmd  <shortaddr> <endpoint> <clusterId> <attributeId|commandId>[payload]
     *
     * 0x0006 = ONOFF_CLUSTER_ID
     * 0x0008 = LEVEL_CONTROL_CLUSTER_ID
     *
     * ONOFF_CLUSTER command id: 00 = off, 01 = on, 02 = toggle
     *
     * LEVEL_CONTROL_CLUSTER command id:
     * 04 = moveto with onoff
     * payload: uint8_t level, uint16_t time
     * level - currentlevel 0 - 255
     * time  - transition time in 1/10 of second
     *
     * COLOR_CONTROL_CLUSTER command id
     * 0A = move to color temperature
     * payload: uint16_t colortemp, uint16_t time
     * colortemp - color temparature 1 - 65279
     * transition time - 1/10 s
     *
     * Example:
     * zclcmd 0x4157 1 0x0006 00 --> off
     * zclcmd 0x4157 1 0x0006 01 --> on
     * zclcmd 0x4157 1 0x0006 02 --> toggle
     *
     * zclcmd 0xBACE 11 0x0008 04100500 --> move to level 0x10 in 5/10 seconds
     *
     * Read Attribute id 0x0000 from cluster id 0x0006 on endpoint 0x01
     * 000000 = ( 0x00 = ZCL_READ_ATTRIBUTES_COMMAND_ID, payload attribute id = 0x0000)
     * Example: zclattr 0x4157 1 0x0006 000000 // zclattr 0xBACE 11 0x0008 000000
     *
     * Identify Cluster 0x0003
     * 000400 = (0x00 = ZCL_IDENTIFY_CLUSTER_IDENTIFY_COMMAND_ID, time = 0x0004 in seconds)
     * Example: zclcmd 0x4157 1 0x0003 000400
     *
     * Reporting:
     * zclattr 0x4157 1 0x0006 08000000 // read reporting config
     * setrep  0x9B0E 3 6 0 0x10 0 0  // set reporting on Cluster 6 Attribute 0 Type 0x10 minmum 0 max 0 seconds
     * setbind 0x9B0E 3 6             // set binding for endpoint 3 Cluster 6 (needed for reporting)
     * getbind 0x9B0E                 // get bindings setting
     *
     */

    if (pilight.process == PROCESS_DAEMON) {

        if (json_find_number(code, "read", &itmp) == 0) {
        	snprintf(command, MAX_STRLENGTH, "zclattr 0x%04X %d 0x0006 000000", shortAddr, endpoint);
        	addsinglethread(command, 0);  // read attribute OnOff
        	snprintf(command, MAX_STRLENGTH, "zclattr 0x%04X %d 0x0008 000000", shortAddr, endpoint);
        	addsinglethread(command, 2);  // read attribute Level_Control
        } else if (json_find_number(code, "identify", &itmp) == 0) {
        	snprintf(command, MAX_STRLENGTH, "zclcmd 0x%04X %d 0x0003 000400", shortAddr, endpoint);
        	zigbeeSendCommand(command);   // identify command 4 seconds
        } else if (json_find_number(code, "autodiscover", &itmp) == 0) {
        	matchtime = 1;
        	snprintf(command, MAX_STRLENGTH, "m 0xC05E 0x0008");  // match ZLL LEVEL_CONTROL_CLUSTER
        	addsinglethread(command, 0);  // send match request ZigBee HA
        	snprintf(command, MAX_STRLENGTH, "m 0x0104 0x0008");  // match ZHA LEVEL_CONTROL_CLUSTER
        	addsinglethread(command, 5);  // send match request ZigBee ZLL
        } else if (dimlevel > -1 || colortemp > -1) {
        	int sec = 0;
        	if (dimlevel > -1 ) {
        		snprintf(command, MAX_STRLENGTH, "zclcmd 0x%04X %d 0x0008 04%02X0500", shortAddr, endpoint, dimlevel);
        		zigbeeSendCommand(command);   // dimlevel command
        		createStateMessage(message, shortAddr, endpoint, state, dimlevel);
        		sec = 2;
        	}
        	if (colortemp > -1) {
        		char *a = (char *) &colortemp;
        		char b = a[0]; // payload is little endian
        		a[0] = a[1];   // swap bytes order
        		a[1] = b;
        		snprintf(command, MAX_STRLENGTH, "zclcmd 0x%04X %d 0x0300 0A%04X0500", shortAddr, endpoint, colortemp);
        		addsinglethread(command, sec);   // send colortemperature command
        	}
        } else if (state != -1) {
        	snprintf(command, MAX_STRLENGTH, "zclcmd 0x%04X %d 0x0006 %02X", shortAddr, endpoint, state);
        	zigbeeSendCommand(command);   // send command on/off/toggle
        	createStateMessage(message, shortAddr, endpoint, state, dimlevel);
        } else {
        	logprintf(LOG_ERR, "[ZigBee] zigbee_dimmer insufficient number of arguments");
        	printf("zigbee_dimmer: insufficient number of arguments\n");
        	return EXIT_FAILURE;
        }

    }

    return EXIT_SUCCESS;
}

static void gc(void) {
//	struct zigbee_data_t *tmp = NULL;
//	while(zigbee_data) {
//		tmp = zigbee_data;
//		FREE(tmp->dev);
//		if (tmp->extaddr != NULL) {
//			FREE(tmp->extaddr);
//		}
//		zigbee_data = zigbee_data->next;
//		FREE(tmp);
//	}
//	if (zigbee_data != NULL) {
//		FREE(zigbee_data);
//	}

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
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -m --toggle\t\t\tsend a toggle signal\n");
	printf("\t -d --dimlevel=dimlevel\t\tsend a specific dimlevel\n");
	printf("\t -c --colortemp=colortemparture\t\tset colortemperature on lights 1 - 65279 (warmwhite to cold, 0x00fa = 4000 K\n");
	printf("\t -s --shortaddr=id\t\tcontrol a device with this short address\n");
	printf("\t -e --endpoint=id\t\tcontrol a device with this endpoint id\n");
	printf("\t -r --read\t\t\tread attribute values from device\n");
	printf("\t -i --identify\t\t\tsend identify request\n");
	printf("\t -a --autodiscover\t\tdiscover ZigBee Level_Control devices\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void zigbeeDimmerInit(void) {
    protocol_register(&zigbee_dimmer);
    protocol_set_id(zigbee_dimmer, "zigbee_dimmer");
    protocol_device_add(zigbee_dimmer, "zigbee_dimmer", "ZigBee Level_Control");
    zigbee_dimmer->devtype = DIMMER;
    zigbee_dimmer->hwtype = ZIGBEE;

	options_add(&zigbee_dimmer->options, 's', "shortaddr", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, NULL);
	options_add(&zigbee_dimmer->options, 'e', "endpoint", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_dimmer->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_dimmer->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_dimmer->options, 'd', "dimlevel", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_dimmer->options, 'm', "toggle", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_dimmer->options, 'r', "read", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_dimmer->options, 'i', "identify", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_dimmer->options, 'a', "autodiscover", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_dimmer->options, 'c', "colortemp", OPTION_HAS_VALUE, DEVICES_OPTIONAL, JSON_NUMBER, NULL, NULL);

	options_add(&zigbee_dimmer->options, 0, "dimlevel-minimum", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, NULL);
	options_add(&zigbee_dimmer->options, 0, "dimlevel-maximum", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)255, NULL);

	options_add(&zigbee_dimmer->options, 0, "extaddr", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, "[0][xX][0-9a-fA-F]{16}");
	options_add(&zigbee_dimmer->options, 0, "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]{1,5}"); // allow 0 - 99999 seconds
	options_add(&zigbee_dimmer->options, 0, "ProfileId", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_dimmer->options, 0, "ManufacturerName", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_dimmer->options, 0, "ModelIdentifier", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_dimmer->options, 0, "DateCode", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_dimmer->options, 0, "Version", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);

	zigbee_dimmer->parseCommand=&parseCommand;
	zigbee_dimmer->createCode=&createCode;
	zigbee_dimmer->printHelp=&printHelp;
	zigbee_dimmer->gc=&gc;
	zigbee_dimmer->checkValues=&checkValues;

	// predefined commands because we need to execute two commands
	// to get both OnOff state and dimlevel from target ZigBee device
	zigbee_cmd_t *zcmd = NULL;
	if ((zcmd = MALLOC(sizeof(struct zigbee_cmd_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(zcmd, '\0', sizeof(struct zigbee_cmd_t));
	if ((zcmd->command = MALLOC(strlen("zclattr 0x%04X %d 0x0008 000000") + 1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(zcmd->command, "zclattr 0x%04X %d 0x0008 000000"); // read Cluster Level_Control attribute value
	zcmd->next = zigbee_commands;
	zigbee_commands = zcmd;

	if ((zcmd = MALLOC(sizeof(struct zigbee_cmd_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(zcmd, '\0', sizeof(struct zigbee_cmd_t));
	if ((zcmd->command = MALLOC(strlen("zclattr 0x%04X %d 0x0006 000000") + 1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(zcmd->command, "zclattr 0x%04X %d 0x0006 000000");  // read Cluster OnOff attribute value
	zcmd->next = zigbee_commands;
	zigbee_commands = zcmd;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice);
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
    module->name = "zigbee_dimmer";
    module->version = "0.1";
    module->reqversion = "7.0";
    module->reqcommit = NULL;
}

void init(void) {
	zigbeeDimmerInit();
}
#endif
