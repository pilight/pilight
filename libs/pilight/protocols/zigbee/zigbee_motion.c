/*
 * zigbee_motion.c
 *
 *  Created on: Mar 9, 2017
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
#include "zigbee_motion.h"

static struct zigbee_data_t *zigbee_data = NULL;
static struct zigbee_cmd_t *zigbee_commands = NULL;

static void *addDevice(int reason, void *param) {
	zigbee_data = addZigbeeDevice(param, zigbee_data, zigbee_commands, zigbee_motion->id);

	return NULL;
}

/* Cluster 0x0500 IAS Zone // Intruder Alarm Systems (IAS) // ZigBee Cluster Library 8.2.2.2.1.3
 * ZoneStatus attribute id; 0x0002, type id 0x19: 16-bit bitmap
 * Attribute Bit Number 0: Alarm1 = 1 – opened or alarmed, 0 – closed or not alarmed
 * Attribute Bit Number 4: Supervision reports = 1 – Reports, 0 – Does not report
 */
static void parseCommand(struct JsonNode *code, char **pmessage) {
	char *message = *pmessage;
	struct zigbee_data_t *tmp = NULL;
	struct JsonNode *jmsg = NULL;
	char *stmp = NULL, *execute = NULL;
	char extaddr[19];  // "0x" + 64-Bit extended Address (16 Bytes hex String) + '\0'
	char profileid[7]; // ZigBee Home Automation 0x0104 // ZigBee Light Link 0xC05E
	char attr[33];
	int a[2], shortaddr = -1, endpoint = -1, attrid = -1, i = 0, foundzone = 0;
	int zonestatus = 0, state = -1, oldstate = -1, battery = -1, foundpow = 0;
	int itmp, pid, status;
	long lasttime = 0;

	for (i = 0; i < 2; i++) {
		a[i] = 0;
	}
	memset(extaddr, '\0', sizeof(extaddr));
	memset(profileid, '\0', sizeof(profileid));

	if((jmsg = json_find_member(code, "message")) == NULL) {
		return;
	}
	if (json_find_string(jmsg, "zigbee_response", &stmp) != 0) {
		return;
	}

	if (sscanf(stmp, "<-APS attr %s %d 0x0500 %x", extaddr, &endpoint, &itmp) != 3
			&& sscanf(stmp, "<-ZCL serverToClient %s %d for cluster 0x0500 %x", extaddr, &endpoint, &itmp) != 3
			&& sscanf(stmp, "<-ZCL attribute report %s 0x0500 %d", extaddr, &itmp) != 2
			&& sscanf(stmp, "<-ZDP match %s %x %d 0x0500 %s", extaddr, &shortaddr, &endpoint, profileid) != 4
			&& sscanf(stmp, "<-ZCL attr %x %d 0x0500 %x %[^\n]", &shortaddr, &endpoint, &attrid, attr) != 4
			&& sscanf(stmp, "<-POWER %s", extaddr) != 1) {
		return;  // only cluster 0x0500 metering or 0x0001 power
	}

	// <-POWER 0x00178801xxxxxxxx 0x8701 1 7 4 100
	if (sscanf(stmp, "<-POWER %s %x %*d %*d %*d %d", extaddr, &shortaddr, &battery) == 3) {
		foundpow = 1;
	}
	// ZoneStatus 16-bit bitmap
	// <-APS attr 0x00124B00xxxxxxxx 1 0x0500 0x0002 0x19 10 00
	if (foundpow == 0
			&& sscanf(stmp, "<-APS attr %s %d 0x0500 0x0002 0x19 %x %x",
			extaddr, &endpoint, &a[0], &a[1]) == 4) {
		foundzone = 1;
	}
	// <-ZCL serverToClient 0x00124B00xxxxxxxx 1 for cluster 0x0500 10 00 00 00 00 00
	if (foundzone == 0 && foundpow == 0
			&& sscanf(stmp, "<-ZCL serverToClient %s %d for cluster 0x0500 %x %x",
					extaddr, &endpoint, &a[0], &a[1]) == 4) {
		foundzone = 1;
	}
	// <-ZCL attribute report 0x00124B00xxxxxxxx 0x0500 1 02 00 19 10 00
	if (foundzone == 0 && foundpow == 0
			&& sscanf(stmp, "<-ZCL attribute report %s 0x0500 %d 02 00 19 %x %x",
					extaddr, &endpoint, &a[0], &a[1]) == 4) {
		foundzone = 1;
	}
	// discovery response from Cluster 0x0500 - part 1
	if (foundzone == 0 && foundpow == 0) {
		if (sscanf(stmp, "<-ZDP match %s %x %d 0x0500 %s", extaddr, &shortaddr, &endpoint, profileid) == 4) {
			matchResponse(extaddr, shortaddr, endpoint, 0x0500 /* cluster */, profileid, zigbee_motion->id);
			return;
		}

	// discovery response from Cluster 0x0500 - part 2 --> read ZigBee Basic Cluster Attributes 4,5,6 and 0x4000
		if (sscanf(stmp, "<-ZCL attr %x %d 0x0500 %x %[^\n]", &shortaddr, &endpoint, &attrid, attr) == 4) {
			matchAttributeResponse(shortaddr, endpoint, 0x0500 /* cluster */, attrid, attr, zigbee_motion->id);
			return;
		}
	}
	if (foundpow == 1) {
		tmp = zigbee_data;
		while (tmp) {
			if (tmp->shortaddr == shortaddr) {
				tmp->battery = battery;
				tmp->lasttime = (long) time(NULL);
				break;
			}
			tmp = tmp->next;
		}
		return;
	}

	if (foundzone == 0) {
		return;
	}
	if (foundzone == 1) {
		for (i = 0; i < 2; i++) {
			zonestatus |= ((a[i]) << (i * 8));
		}
		state = zonestatus & 0x01;
		logprintf(LOG_DEBUG, "[ZigBee] zigbee_motion parseCommand zonestatus = 0x%02X, state = %d", zonestatus, state);
	}

//	if (shortaddr > -1) {
		tmp = zigbee_data;
		while (tmp) {
			if (tmp->extaddr != NULL
					&& strcmp(extaddr, tmp->extaddr) == 0
					&& strcmp(tmp->protocol, zigbee_motion->id) == 0) {
				shortaddr = (int) tmp->shortaddr;
				oldstate = tmp->currentstate;
				endpoint = tmp->endpoint;
				battery = tmp->battery;
				lasttime = tmp->lasttime;
				execute = tmp->execute;
				break;
			}
			tmp = tmp->next;
		}
//	}
	if (shortaddr == -1 && strlen(extaddr) > 0 && strlen(extaddr) <= 6) {
		sscanf(extaddr, "%x", &shortaddr);  // sometimes extaddr has the shortaddr
		tmp = zigbee_data;
		while (tmp) {
			if (tmp->shortaddr == shortaddr
					&& tmp->endpoint == endpoint
					&& strcmp(tmp->protocol, zigbee_motion->id) == 0) {
				oldstate = tmp->currentstate;
				battery = tmp->battery;
				lasttime = tmp->lasttime;
				execute = tmp->execute;
				break;
			}
			tmp = tmp->next;
		}
	}
	if (shortaddr == -1) { // device not found
		logprintf(LOG_DEBUG, "[ZigBee] zigbee_motion parseCommand device not found shortaddr = %d, extaddr = %s", shortaddr, extaddr);
		return;
	}

	if (lasttime + 7200 < (long) time(NULL)) {  // every 2 hours
		// check power configuration on battery device
		sendPowerCmd(shortaddr);
	}

	state = newstate(shortaddr, endpoint, state, zigbee_data);
	if (state == oldstate) { // only broadcast if state has changed
		return;
	}

	int x = snprintf(message, 255, "{\"shortaddr\":\"0x%04X\",", shortaddr);
	x += snprintf(&message[x], 255-x, "\"endpoint\":%d,", endpoint);
	if (foundzone == 1) {
		x += snprintf(&message[x], 255-x, "\"ZoneStatus\": %d,", zonestatus);
	}
	x += snprintf(&message[x], 255-x, "\"battery\": %d,", battery);
	if (state == 1) {
		x += snprintf(&message[x], 255-x, "\"state\":\"on\",");
	} else {
		x += snprintf(&message[x], 255-x, "\"state\":\"off\",");
	}
	x += snprintf(&message[x-1], 255-x, "}");

	broadcast_message(message, zigbee_motion->id);

	if (execute != NULL) {
		pid = fork(); // run script without waiting for result
		if (pid == 0) {
			//child
			if (fork() == 0) {
				// child of child becomes no zombie but is adopted by init
				char script_arg1[7], script_arg2[4], script_arg3[4]/*, script_arg4[4], script_arg5[4]*/;
				snprintf(script_arg1, 7, "0x%04X",shortaddr);
				snprintf(script_arg2, 4, "%d",endpoint);
				snprintf(script_arg3, 4, "%d",state);
//				snprintf(script_arg4, 4, "%d",buttonstate);
//				snprintf(script_arg5, 4, "%d",buttoncount);
				execlp(execute, "arg0", script_arg1, script_arg2, script_arg3, /*script_arg4, script_arg5,*/ NULL);
				exit(0);
			}
			exit(0); // end child
		} else {
			waitpid(pid, &status, 0); // avoid zombie defunct process
		}
	}
}

static int createCode(struct JsonNode *code, char **pmessage) {
	char *message = *pmessage;
	char command[MAX_STRLENGTH];
	char *stmp = NULL;
	double itmp = 0.0;
	int shortAddr = 0xFFFF;
	uint8_t endpoint = 0xFF;
    snprintf(message, 255, "{}");

    if (json_find_string(code, "shortaddr", &stmp) == 0) {
    	sscanf(stmp, "%x", &shortAddr);
    }
    if (json_find_number(code, "shortaddr", &itmp) == 0) {
    	shortAddr = (int) itmp;
    }
    if (json_find_number(code, "endpoint", &itmp) == 0) {
    	endpoint = (uint8_t) itmp;
    }
    if (shortAddr == 0xFFFF || endpoint == 0xFF) {
    	logprintf(LOG_ERR, "zigbee_motion: insufficient number of arguments");
    	printf("zigbee_motion: insufficient number of arguments\n");
    	printf("code %s stmp = %s, shortAddr = %x, endpoint = %d\n", json_stringify(code, "\t"), stmp, shortAddr, endpoint);
    	return EXIT_FAILURE;
    }

    if (pilight.process == PROCESS_DAEMON) {

    	if (json_find_number(code, "read", &itmp) == 0) {
    		snprintf(command, MAX_STRLENGTH, "zclattr 0x%04X %d 0x0500 000200", shortAddr, endpoint);
    		zigbeeSendCommand(command);
    	} else if (json_find_number(code, "alarm", &itmp) == 0) {
    		// send alarm command 00020100 => 00=alarm, 02=fire, 0100=1 second
    		snprintf(command, MAX_STRLENGTH, "zclcmd 0x%04X %d 0x0502 00020100", shortAddr, endpoint);
    		zigbeeSendCommand(command);
    	} else {
    		logprintf(LOG_ERR, "[ZigBee] zigbee_motion insufficient number of arguments");
    		printf("zigbee_motion: insufficient number of arguments\n");
    		return EXIT_FAILURE;
    	}

    }
	return EXIT_SUCCESS;
}

static int checkValues(JsonNode *code) {
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

static void printHelp(void) {
	printf("\t motion\n");
	printf("\t -r --read\t\t\tread if motion detected\n");
	printf("\t -a --alarm\t\t\tstart alarm 1 second\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void zigbeeMotionInit(void) {
    protocol_register(&zigbee_motion);
    protocol_set_id(zigbee_motion, "zigbee_motion");
    protocol_device_add(zigbee_motion, "zigbee_motion", "ZigBee motion");
    zigbee_motion->devtype = MOTION;
    zigbee_motion->hwtype = ZIGBEE;

	options_add(&zigbee_motion->options, 's', "shortaddr", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, NULL);
	options_add(&zigbee_motion->options, 'e', "endpoint", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_motion->options, 'z', "ZoneStatus", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_motion->options, 'w', "battery", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_motion->options, 'r', "read", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_motion->options, 'a', "alarm", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_motion->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_motion->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

	options_add(&zigbee_motion->options, 0, "extaddr", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, "[0][xX][0-9a-fA-F]{16}");
	options_add(&zigbee_motion->options, 0, "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]{1,5}"); // allow 0 - 99999 seconds
	options_add(&zigbee_motion->options, 0, "ProfileId", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_motion->options, 0, "ManufacturerName", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_motion->options, 0, "ModelIdentifier", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_motion->options, 0, "DateCode", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_motion->options, 0, "execute", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, "", NULL);

	zigbee_motion->parseCommand=&parseCommand;
	zigbee_motion->createCode=&createCode;
	zigbee_motion->printHelp=&printHelp;
	zigbee_motion->gc=&gc;
	zigbee_motion->checkValues=&checkValues;

	// predefined commands to get zone values from target ZigBee device
	zigbee_cmd_t *zcmd = NULL;
	if ((zcmd = MALLOC(sizeof(struct zigbee_cmd_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(zcmd, '\0', sizeof(struct zigbee_cmd_t));
	if ((zcmd->command = MALLOC(strlen("zclattr 0x%04X %d 0x0500 000200") + 1)) == NULL) {
		OUT_OF_MEMORY
	}
	// read Cluster 0x0500 attributes 0x0002 ZoneStatus // zclattr 0x3B58 1 0x0500 000200
	strcpy(zcmd->command, "zclattr 0x%04X %d 0x0500 000200");
	zcmd->next = zigbee_commands;
	zigbee_commands = zcmd;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice);
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
    module->name = "zigbee_motion";
    module->version = "0.1";
    module->reqversion = "7.0";
    module->reqcommit = NULL;
}

void init(void) {
	zigbeeMotionInit();
}
#endif
