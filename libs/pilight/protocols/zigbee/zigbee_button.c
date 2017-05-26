/*
 * zigbee_button.c
 *
 *  Created on: May 7, 2017
 *      Author: ma-ca
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "../../core/pilight.h"
#include "../../core/dso.h"
#include "../../core/common.h"
#include "../../core/log.h"
#include "../../hardware/zigbee.h"
#include "../protocol.h"
#include "zigbee_button.h"

static struct zigbee_data_t *zigbee_data = NULL;
static struct zigbee_cmd_t *zigbee_commands = NULL;

static void *addDevice(int reason, void *param) {
	zigbee_data = addZigbeeDevice(param, zigbee_data, zigbee_commands, zigbee_button->id);

	return NULL;
}

/*
 * This protocol handles ZigBee commands from Philips Hue Dimmer Switch
 * and Xiaomi Smart Wireless Switch
 * and Xiaomi Intelligent Door Window Sensor Control
 *
 * Philips Hue Dimmer Switch needs binding on endpoint 2 on cluster 0xFC00
 * ./pilight-send -p zigbee_ctrl -s 0x8701 -e 2 -b 0xFC00 -x " 0x00178801xxxxxxxx" -y 2 -d " 0x00212EFFxxxxxxxx"
 *
 * Pressing the buttons 1=on, 2=dimmer up, 3=dimmer down, 4=off results in cluster 0xFC00 command from the Philips Switch
 * <-ZCL serverToClient 0x00178801xxxxxxxx 2 for cluster 0xFC00 manufacturer 0x100B 01 00 00 30 00 21 00 00
 *                                                                                   ^           ^
 *                                                                                 buttonid     buttonstate
 *
 * Xiaomi Smart Wireless Switch does not need binding, it always sends ZCL attribute report to the coordinator
 *
 * Pressing the button repeatedly results in different attribute reports from cluster 0x0006
 *
 * <-ZCL attribute report 0x00158D00xxxxxxxx 0x0006 1 00 00 10 00
 * <-ZCL attribute report 0x00158D00xxxxxxxx 0x0006 1 00 00 10 01
 *                                                              ^
 * buttonstate 0 = start pressing, 1 = finished 1x pressing     |
 *             2 = finished 2x pressing, 3 = finished 3x pressing
 *             4 = finished 4x pressing, 80 = more than 4x
 *
 * Xiaomi Intelligent Door Window Sensor also does not need binding, it also sends ZCL attribute reports
 *
 * <-ZCL attribute report 0x00158D00xxxxxxxx 0x0006 1 00 00 10 00  (door/window closed)
 * <-ZCL attribute report 0x00158D00xxxxxxxx 0x0006 1 00 00 10 01  (door/window open)
 *
 *   when pressing the small button in the hole, the device sends a ZCL report from cluster 0x0000 attribute 0x0005
 * <-ZCL attribute report 0x00158D00xxxxxxxx 0x0000 1 05 00 42 12 6C 75 6D 69 2E 73 65 6E 73 6F 72 5F 6D 61 67 6E 65 74
 */
static void parseCommand(struct JsonNode *code, char **pmessage) {
	char *message = *pmessage;
	struct zigbee_data_t *tmp = NULL;
	struct JsonNode *jmsg = NULL;
	char *stmp = NULL, *execute = NULL/*, *script = NULL*/;
	char extaddr[19];  // "0x" + 64-Bit extended Address (16 Bytes hex String) + '\0'
	char profileid[7]; // ZigBee Home Automation 0x0104 // ZigBee Light Link 0xC05E
	long timenow = (long) time(NULL);
	long lasttime = 0;
	int shortaddr = -1, endpoint = -1, found = 0, match = 0;
	int buttonid = -1, buttonstate = -1, buttoncount = -1, oldbuttonid = -1;
	int state = -1, battery = -1, pid, status;

	memset(extaddr, '\0', sizeof(extaddr));
	memset(profileid, '\0', sizeof(profileid));

	if((jmsg = json_find_member(code, "message")) == NULL) {
		return;
	}
	if (json_find_string(jmsg, "zigbee_response", &stmp) != 0) {
		return;
	}

	if (sscanf(stmp, "<-ZCL serverToClient %s %d for cluster 0xFC00 manufacturer 0x100B", extaddr, &endpoint) != 2
			&& sscanf(stmp, "<-ZCL attribute report %s 0x0006 %*d %*x %*x %*x %*x",  extaddr) != 1
			&& sscanf(stmp, "<-POWER %s", extaddr) != 1) {
		return;  // only cluster 0xFC00 or 0x0006 or 0x0001 power
	}

	// Philips Hue Dimmer Switch
	// buttonid    1=on, 2=dim up, 3=dim down, 4=off
	// buttonstate 0=start pressing, 1=keep pressing, 2=finished pressing short, 3=finished pressing long
	//                                                                                              ||
	// <-ZCL serverToClient 0x00178801xxxxxxxx 2 for cluster 0xFC00 manufacturer 0x100B 01 00 00 30 00 21 00 00
	// <-ZCL serverToClient 0x00178801xxxxxxxx 2 for cluster 0xFC00 manufacturer 0x100B 01 00 00 30 02 21 01 00
	// <-ZCL serverToClient 0x00178801xxxxxxxx 2 for cluster 0xFC00 manufacturer 0x100B 01 00 00 30 01 21 08 00
	// <-ZCL serverToClient 0x00178801xxxxxxxx 2 for cluster 0xFC00 manufacturer 0x100B 01 00 00 30 03 21 22 00
	if (sscanf(stmp, "<-ZCL serverToClient %s %d for cluster 0xFC00 manufacturer 0x100B %x %*x %*x %*x %x %*x %*x %*x",
					extaddr, &endpoint, &buttonid, &buttonstate) == 4) {
		found = 1;
	}
	// Xiaomi Smart Wireless Switch or Xiaomi Intelligent Door Window Sensor
	// <-ZCL attribute report 0x00158D00xxxxxxxx 0x0006 1 00 00 10 00  (button start pressed)
	// <-ZCL attribute report 0x00158D00xxxxxxxx 0x0006 1 00 80 20 04  (button finished pressed)
	// <-ZCL attribute report 0x00158D00xxxxxxxx 0x0006 1 00 00 10 00  (door/window closed)
	// <-ZCL attribute report 0x00158D00xxxxxxxx 0x0006 1 00 00 10 01  (door/window open)
	if (found == 0 && sscanf(stmp, "<-ZCL attribute report %s 0x0006 %d %*x %*x %*x %x", extaddr, &endpoint, &buttonstate) == 3) {
		buttonid = buttonstate;
		found = 1;
	}
	// <-POWER 0x00178801xxxxxxxx 0x8701 1 7 4 100
	if (found == 0 && sscanf(stmp, "<-POWER %s %x %*d %*d %*d %d", extaddr, &shortaddr, &battery) != 1) {
		found = 1;
	}

	if (found == 0) {
		return;
	}

	tmp = zigbee_data;
	while(tmp) {
		if (tmp->extaddr != NULL
				&& strcmp(extaddr, tmp->extaddr) == 0
				&& strcmp(tmp->protocol, zigbee_button->id) == 0) {
			shortaddr = (int) tmp->shortaddr;
			oldbuttonid = tmp->currentstate;
			tmp->currentstate = buttonid;
			lasttime = tmp->lasttime;
			tmp->lasttime = timenow;
			if ((buttonstate == 0) && (buttonid == oldbuttonid) && (timenow < lasttime + 2)) {
				tmp->buttoncount++;
			} else if (buttonstate == 0) {
				tmp->buttoncount = 1;
			}
			buttoncount = tmp->buttoncount;
			execute = tmp->execute;
			endpoint = tmp->endpoint;
			match = 1;
			break;
		}
		tmp = tmp->next;
	}
	if (match == 0 && strlen(extaddr) > 0) {
		sscanf(extaddr, "%x", &shortaddr);  // sometimes extaddr has the shortaddr
		tmp = zigbee_data;
		while(tmp) {
			if (tmp->shortaddr == shortaddr
					&& tmp->endpoint == endpoint
					&& strcmp(tmp->protocol, zigbee_button->id) == 0) {
				oldbuttonid = tmp->currentstate;
				tmp->currentstate = buttonid;
				lasttime = tmp->lasttime;
				tmp->lasttime = timenow;
				if ((buttonstate == 0) && (buttonid == oldbuttonid) && (timenow < lasttime + 2)) {
					tmp->buttoncount++;
				} else if (buttonstate == 0) {
					tmp->buttoncount = 1;
				}
				buttoncount = tmp->buttoncount;
				execute = tmp->execute;
				match = 1;
				break;
			}
			tmp = tmp->next;
		}
	}

	if (match == 0) { // device not found
		return;
	}
	logprintf(LOG_DEBUG, "[ZigBee] button shortaddr = 0x%04X, ButtonId = %d, ButtonState = %d, ButtonCount = %d",
			shortaddr, buttonid, buttonstate, buttoncount);

	if (buttonid == 1) {
		state = 1;
	}
	if (buttonstate == 0 && buttonid > 1) {  // when starting pressing button 2,3,4
		sendPowerCmd(shortaddr);
	}

	int x = snprintf(message, 255, "{\"shortaddr\":\"0x%04X\",", shortaddr);
	x += snprintf(&message[x], 255-x, "\"endpoint\":%d,", endpoint);
	if (buttonstate > -1) {
		x += snprintf(&message[x], 255-x, "\"ButtonId\": %d,", buttonid);
		x += snprintf(&message[x], 255-x, "\"ButtonState\": %d,", buttonstate);
		x += snprintf(&message[x], 255-x, "\"ButtonCount\": %d,", buttoncount);
	}
	if (state == 1) {
		x += snprintf(&message[x], 255-x, "\"state\":\"on\",");
	} else {
		x += snprintf(&message[x], 255-x, "\"state\":\"off\",");
	}
	if (battery > -1) {
		x += snprintf(&message[x], 255-x, "\"battery\": %d,", battery);
	}
	x += snprintf(&message[x-1], 255-x, "}");

	broadcast_message(message, zigbee_button->id);

	if (battery > -1) {
		return;
	}
	if (execute != NULL) {
		pid = fork(); // run script without waiting for result
		if (pid == 0) {
			//child
			if (fork() == 0) {
				// child of child becomes no zombie but is adopted by init
				char script_arg1[7], script_arg2[4], script_arg3[4], script_arg4[4], script_arg5[4];
				snprintf(script_arg1, 7, "0x%04X",shortaddr);
				snprintf(script_arg2, 4, "%d",endpoint);
				snprintf(script_arg3, 4, "%d",buttonid);
				snprintf(script_arg4, 4, "%d",buttonstate);
				snprintf(script_arg5, 4, "%d",buttoncount);
				execlp(execute, "arg0", script_arg1, script_arg2, script_arg3, script_arg4, script_arg5, NULL);
				exit(0);
			}
			exit(0); // end child
		} else {
			waitpid(pid, &status, 0); // avoid zombie defunct process
		}
	}
}

static int createCode(struct JsonNode *code, char **message) {
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
//		if (tmp->execute != NULL) {
//			FREE(tmp->execute);
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
	printf("\t protocol support for Philips Hue Dimmer Switch and Xiaomi Smart Wireless Switch\n");
	printf("\t use rules to process button actions\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void zigbeeButtonInit(void) {
    protocol_register(&zigbee_button);
    protocol_set_id(zigbee_button, "zigbee_button");
    protocol_device_add(zigbee_button, "zigbee_button", "ZigBee Button or Philips Hue Dimmer Switch");
    zigbee_button->devtype = SWITCH;
    zigbee_button->hwtype = ZIGBEE;

	options_add(&zigbee_button->options, 's', "shortaddr", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, NULL);
	options_add(&zigbee_button->options, 'e', "endpoint", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_button->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_button->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_button->options, 'x', "execute", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_button->options, 'b', "ButtonId", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_button->options, 'c', "ButtonState", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_button->options, 'd', "ButtonCount", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_button->options, 'w', "battery", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);

	options_add(&zigbee_button->options, 0, "extaddr", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, "[0][xX][0-9a-fA-F]{16}");
	options_add(&zigbee_button->options, 0, "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]{1,5}"); // allow 0 - 99999 seconds
	options_add(&zigbee_button->options, 0, "ProfileId", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_button->options, 0, "ManufacturerName", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_button->options, 0, "ModelIdentifier", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_button->options, 0, "DateCode", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_button->options, 0, "Version", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);

	zigbee_button->parseCommand=&parseCommand;
	zigbee_button->createCode=&createCode;
	zigbee_button->printHelp=&printHelp;
	zigbee_button->gc=&gc;
	zigbee_button->checkValues=&checkValues;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice);
}
#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
    module->name = "zigbee_button";
    module->version = "0.1";
    module->reqversion = "7.0";
    module->reqcommit = NULL;
}

void init(void) {
	zigbeeButtonInit();
}
#endif
