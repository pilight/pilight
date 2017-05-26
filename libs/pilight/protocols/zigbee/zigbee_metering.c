/*
 * zigbee_metering.c
 *
 *  Created on: Feb 27, 2017
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
#include "zigbee_metering.h"

static struct zigbee_data_t *zigbee_data = NULL;
static struct zigbee_cmd_t *zigbee_commands = NULL;

static void *addDevice(int reason, void *param) {
	zigbee_data = addZigbeeDevice(param, zigbee_data, zigbee_commands, zigbee_metering->id);

	return NULL;
}

/* Cluster 0x0702 Metering
 * CurrentSummationDelivered attribute id; 0x0000, type id: 0x25 unsigned 48-Bit
 * InstantaneousDemand       attribute id: 0x0400, type id: 0x2A signed 24-Bit
 */
static void parseCommand(struct JsonNode *code, char **pmessage) {
	char *message = *pmessage;
	struct zigbee_data_t *tmp = NULL;
	struct JsonNode *jmsg = NULL;
	char *stmp = NULL, *logfile = NULL;
	char extaddr[19];  // "0x" + 64-Bit extended Address (16 Bytes hex String) + '\0'
	char profileid[7]; // ZigBee Home Automation 0x0104 // ZigBee Light Link 0xC05E
	char attr[33];
	char content[200];
	uint64_t powtotal = 0;  // we need unsigned 48-Bit
	int32_t powcurrent = 0; // we need signed 24-Bit
	int a[6], b[3], shortaddr = -1, endpoint = -1, attrid = -1, i = 0, foundtot = 0, foundcur = 0;

	for (i = 0; i < 6; i++) {
		a[i] = 0;
	}
	for (i = 0; i < 3; i++) {
		b[i] = 0;
	}
	memset(extaddr, '\0', sizeof(extaddr));
	memset(profileid, '\0', sizeof(profileid));

	if((jmsg = json_find_member(code, "message")) == NULL) {
		return;
	}
	if (json_find_string(jmsg, "zigbee_response", &stmp) != 0) {
		return;
	}

	// discovery response from Cluster 0x0702 - part 1
	// <-ZDP match 0x001FEE00xxxxxxxx 0x4157   5 0x0702 0x0104
	if (sscanf(stmp, "<-ZDP match %s %x %d 0x0702 %s", extaddr, &shortaddr, &endpoint, profileid) == 4) {
		matchResponse(extaddr, shortaddr, endpoint, 0x0702 /* cluster */, profileid, zigbee_metering->id);
		return;
	}
	// discovery response from Cluster 0x0702 - part 2 --> read ZigBee Basic Cluster Attributes 4,5,6 and 0x4000
	if (sscanf(stmp, "<-ZCL attr %x %d 0x0702 %x %[^\n]", &shortaddr, &endpoint, &attrid, attr) == 4) {
		matchAttributeResponse(shortaddr, endpoint, 0x0702 /* cluster */, attrid, attr, zigbee_metering->id);
		return;
	}

	if (sscanf(stmp, "<-APS attr %s %d 0x0702", extaddr, &endpoint) != 2 && sscanf(stmp, "<-ZCL attribute report %s 0x0702", extaddr) != 1) {
		return;  // only cluster 0x0702 metering
	}

	// CurrentSummationDelivered unsigned 48-Bit and InstantaneousDemand signed 24-Bit
	// <-APS attr 0x001FEE00xxxxxxxx 5 0x0702 0x0000 0x25 C0 39 02 00 00 00 00 04 00 2A 00 00 00
	if (sscanf(stmp, "<-APS attr %s %d 0x0702 0x0000 0x25 %x %x %x %x %x %x 00 04 00 2A %x %x %x",
			extaddr, &endpoint, &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &b[0], &b[1], &b[2]) == 11) {
		foundtot = 1;
		foundcur = 1;
	}
	// CurrentSummationDelivered unsigned 48-Bit
	// <-APS attr 0x001FEE00xxxxxxxx 5 0x0702 0x0000 0x25 6E 71 04 00 00 00
	if (foundtot == 0 &&
			sscanf(stmp, "<-APS attr %s %d 0x0702 0x0000 0x25 %x %x %x %x %x %x", extaddr, &endpoint, &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]) == 8) {
		foundtot = 1;
	}
	// InstantaneousDemand signed 24-Bit
	// <-APS attr 0x001FEE00xxxxxxxx 5 0x0702 0x0400 0x2A 00 00 00
	if (foundcur == 0 && sscanf(stmp, "<-APS attr %s %d 0x0702 0x0400 0x2A %x %x %x", extaddr, &endpoint, &b[0], &b[1], &b[2]) == 5) {
		foundcur = 1;
	}
	// <-ZCL attribute report 0x00124B00xxxxxxxx 0x0702 1 00 04 2A 03 15 00 00 00 25 40 1F 00 00 00 00
	if (foundcur == 0
			&& sscanf(stmp, "<-ZCL attribute report %s 0x0702 %d 00 04 2A %x %x %x 00 00 25 %x %x %x %x %x %x",
					extaddr, &endpoint, &b[0], &b[1], &b[2], &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]) == 11) {
		foundtot = 1;
		foundcur = 1;
	}

	// <-ZCL attribute report 0x001FEE00xxxxxxxx 0x0702 5 00 04 2A 0B 00 00
	if (foundcur == 0 && sscanf(stmp, "<-ZCL attribute report %s 0x0702 %d 00 04 2A %x %x %x", extaddr, &endpoint, &b[0], &b[1], &b[2]) == 5) {
		foundcur = 1;
	}
	if (foundtot == 0 &&
			sscanf(stmp, "<-ZCL attribute report %s 0x0702 %d 00 00 25 %x %x %x %x %x %x", extaddr, &endpoint, &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]) == 8) {
		foundtot = 1;
	}

	if (foundtot == 0 && foundcur == 0) {
		return;
	}
	if (foundtot == 1) {
		for (i = 0; i < 6; i++) {
			powtotal |= (((uint64_t) a[i]) << (i * 8));
		}
		logprintf(LOG_DEBUG, "[ZigBee] zigbee_metering parseCommand powtotal = %llu, 0x%012llX", powtotal, powtotal);
	}
	if (foundcur == 1) {
		for (i = 0; i < 3; i++) {
			powcurrent |= (((int32_t) b[i]) << (i * 8));
		}
		logprintf(LOG_DEBUG, "[ZigBee] zigbee_metering parseCommand powcurrent = %d, 0x%06X", powcurrent, powcurrent);
	}

	tmp = zigbee_data;
	while(tmp) {
//		logprintf(LOG_DEBUG, "metering tmp->extaddr = %s, shortaddr = 0x%04X, protocol = %s", tmp->extaddr, tmp->shortaddr, tmp->protocol);
		if (tmp->extaddr != NULL
				&& strcmp(extaddr, tmp->extaddr) == 0
				&& tmp->endpoint == endpoint
				&& strcmp(tmp->protocol, zigbee_metering->id) == 0) {
			shortaddr = (int) tmp->shortaddr;
			logfile = tmp->logfile;
			break;
		}
		tmp = tmp->next;
	}
	logprintf(LOG_DEBUG, "metering shortaddr = 0x%04X, logfile = %s", shortaddr, logfile);
	if (shortaddr == -1) { // device not found
		return;
	}

	int x = snprintf(message, 255, "{\"shortaddr\":\"0x%04X\",", shortaddr);
	x += snprintf(&message[x], 255-x, "\"endpoint\":%d,", endpoint);
	if (foundtot == 1) {
		x += snprintf(&message[x], 255-x, "\"CurrentSummationDelivered\": %llu,", (unsigned long long int) powtotal);
	}
	if (foundcur == 1) {
		x += snprintf(&message[x], 255-x, "\"InstantaneousDemand\": %d,", powcurrent);
	}
	x += snprintf(&message[x-1], 255-x, "}");

	broadcast_message(message, zigbee_metering->id);

	FILE *fp = NULL;
	if (logfile != NULL && strlen(logfile) > 0) {
		if((fp = fopen(logfile, "a")) == NULL) {
			logprintf(LOG_ERR, "cannot write log file: %s", logfile);
			return;
		}
		time_t timenow = time(NULL);
		struct tm current;
		memset(&current, '\0', sizeof(struct tm));
		localtime_r(&timenow, &current);
		x = strftime(content, sizeof content, "%Y-%m-%d_%H:%M:%S", &current);
		x += snprintf(&content[x], 200-x, " %ld", timenow);
		if (foundtot == 1) {
			x += snprintf(&content[x], 200-x, " %llu", (unsigned long long int) powtotal);
		} else {
			x += snprintf(&content[x], 200-x, " -");
		}
		if (foundcur == 1) {
			x += snprintf(&content[x], 200-x, " %d \n", powcurrent);
		} else {
			x += snprintf(&content[x], 200-x, " - \n");
		}
		fwrite(content, sizeof(char), strlen(content), fp);
		fclose(fp);
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
	if (json_find_number(code, "autodiscover", &itmp) == 0) {
		if (pilight.process != PROCESS_DAEMON) {
			printf("ZigBee starting discovery\n");
			return EXIT_SUCCESS;
		}
	} else if (shortAddr == 0xFFFF || endpoint == 0xFF) {
    	logprintf(LOG_ERR, "zigbee_metering: insufficient number of arguments");
    	printf("zigbee_metering: insufficient number of arguments\n");
    	printf("code %s stmp = %s, shortAddr = %x, endpoint = %d\n", json_stringify(code, "\t"), stmp, shortAddr, endpoint);
    	return EXIT_FAILURE;
    }

    if (pilight.process == PROCESS_DAEMON) {

        if (json_find_number(code, "read", &itmp) == 0) {
        	snprintf(command, MAX_STRLENGTH, "zclattr 0x%04X %d 0x0702 0000000004", shortAddr, endpoint);
        	addsinglethread(command, 0);
        } else if (json_find_number(code, "autodiscover", &itmp) == 0) {
        	matchtime = 1;
        	snprintf(command, MAX_STRLENGTH, "m 0x0104 0x0702");  // match ZHA
        	addsinglethread(command, 0);  // send match request ZigBee ZLL
        } else {
        	logprintf(LOG_ERR, "[ZigBee] zigbee_metering insufficient number of arguments");
        	printf("zigbee_metering: insufficient number of arguments\n");
        	return EXIT_FAILURE;
        }

    }
	return EXIT_SUCCESS;
}

static int checkValues(JsonNode *code) {
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
//		if (tmp->logfile != NULL) {
//			FREE(tmp->logfile);
//		}
//		if (tmp->protocol != NULL) {
//			FREE(tmp->protocol);
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

static void printHelp(void) {
	printf("\t metering\n");
	printf("\t -r --read\t\t\tread summation (total) amount of electrical energy delivered\n");
	printf("\t\t\t\tand power currently delivered\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void zigbeeMeteringInit(void) {
    protocol_register(&zigbee_metering);
    protocol_set_id(zigbee_metering, "zigbee_metering");
    protocol_device_add(zigbee_metering, "zigbee_metering", "ZigBee metering");
    zigbee_metering->devtype = LABEL;
    zigbee_metering->hwtype = ZIGBEE;

	options_add(&zigbee_metering->options, 's', "shortaddr", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, NULL);
	options_add(&zigbee_metering->options, 'e', "endpoint", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_metering->options, 'd', "CurrentSummationDelivered", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_metering->options, 'i', "InstantaneousDemand", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_metering->options, 'r', "read", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_metering->options, 'a', "autodiscover", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_metering->options, 'l', "logfile", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);

	options_add(&zigbee_metering->options, 0, "extaddr", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, "[0][xX][0-9a-fA-F]{16}");
	options_add(&zigbee_metering->options, 0, "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]{1,5}"); // allow 0 - 99999 seconds
	options_add(&zigbee_metering->options, 0, "ProfileId", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_metering->options, 0, "ManufacturerName", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_metering->options, 0, "ModelIdentifier", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_metering->options, 0, "DateCode", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);

	zigbee_metering->parseCommand=&parseCommand;
	zigbee_metering->createCode=&createCode;
	zigbee_metering->printHelp=&printHelp;
	zigbee_metering->gc=&gc;
	zigbee_metering->checkValues=&checkValues;

	// predefined commands to get two metering values from target ZigBee device
	zigbee_cmd_t *zcmd = NULL;
	if ((zcmd = MALLOC(sizeof(struct zigbee_cmd_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(zcmd, '\0', sizeof(struct zigbee_cmd_t));
	if ((zcmd->command = MALLOC(strlen("zclattr 0x%04X %d 0x0702 0000000004") + 1)) == NULL) {
		OUT_OF_MEMORY
	}
	// read Cluster 0x0702 attributes 0x0000 CurrentSummationDelivered and 0x0400 InstantaneousDemand
	strcpy(zcmd->command, "zclattr 0x%04X %d 0x0702 0000000004");
	zcmd->next = zigbee_commands;
	zigbee_commands = zcmd;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice);
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
    module->name = "zigbee_metering";
    module->version = "0.1";
    module->reqversion = "7.0";
    module->reqcommit = NULL;
}

void init(void) {
	zigbeeMeteringInit();
}
#endif
