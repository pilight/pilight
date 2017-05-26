/*
 * zigbee_temperature.c
 *
 *  Created on: May 11, 2017
 *      Author: ma-ca
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../../core/pilight.h"
#include "../../core/dso.h"
#include "../../core/common.h"
#include "../../core/log.h"
#include "../../hardware/zigbee.h"
#include "../protocol.h"
#include "zigbee_temperature.h"

static struct zigbee_data_t *zigbee_data = NULL;
static struct zigbee_cmd_t *zigbee_commands = NULL;

static void *addDevice(int reason, void *param) {
	zigbee_data = addZigbeeDevice(param, zigbee_data, zigbee_commands, zigbee_temperature->id);

	return NULL;
}

/*
 * 0x0402 Temperature Measurement Cluster
 *
 * Attribute id: 0x0000, type id: 0x29: Signed 16-bit integer MeasuredValue
 *
 * 0x0405 Relative Humidity Measurement Cluster
 *
 * Attribute id: 0x0000, type id: 0x21: Unsigned 16-bit Integer MeasuredValue
 *
 * <-ZCL attribute report 0x00158D00xxxxxxxx 0x0402 1 00 00 29 3F 09
 * <-ZCL attribute report 0x00158D00xxxxxxxx 0x0405 1 00 00 21 DE 1A
 * <-ZCL attribute report 0xA31F 0x0402 1 00 00 29 48 08
 * <-ZCL attribute report 0xA31F 0x0405 1 00 00 21 09 19
 *
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
	int a[2], shortaddr = -1, endpoint = -1, attrid = -1, i;
	int foundvalue = 0, foundtemp = 0, foundhumidity = 0;
	int temperature = 0, humidity = 0;

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

	if (sscanf(stmp, "<-APS attr %s %d 0x0402", extaddr, &endpoint) != 2
			&& sscanf(stmp, "<-APS attr %s %d 0x0405", extaddr, &endpoint) != 2
			&& sscanf(stmp, "<-ZCL serverToClient %s for cluster 0x0402", extaddr) != 1
			&& sscanf(stmp, "<-ZCL serverToClient %s for cluster 0x0405", extaddr) != 1
			&& sscanf(stmp, "<-ZCL attribute report %s 0x0402",  extaddr) != 1
			&& sscanf(stmp, "<-ZCL attribute report %s 0x0405",  extaddr) != 1
			&& sscanf(stmp, "<-ZDP match %s %x %d 0x0402 %s", extaddr, &shortaddr, &endpoint, profileid) != 4
			&& sscanf(stmp, "<-ZCL attr %x %d 0x0402 %x %[^\n]", &shortaddr, &endpoint, &attrid, attr) != 4) {
		return;  // only cluster 0x0402 temperature and 0x0405 humidity
	}
	// <-ZCL attribute report 0x00158D00xxxxxxxx 0x0402 1 00 00 29 3F 09
	if (sscanf(stmp, "<-ZCL attribute report %s 0x0402 %d 00 00 29 %x %x", extaddr, &endpoint, &a[0], &a[1]) == 4) {
		foundtemp = 1;
	}
	// <-ZCL attribute report 0x00158D00xxxxxxxx 0x0405 1 00 00 21 DE 1A
	if (foundtemp == 0 && sscanf(stmp, "<-ZCL attribute report %s 0x0405 %d 00 00 21 %x %x", extaddr, &endpoint, &a[0], &a[1]) == 4) {
		foundhumidity = 1;
	}
	// discovery response from Cluster 0x0402 - part 1
	if (foundtemp == 0) {
		if (sscanf(stmp, "<-ZDP match %s %x %d 0x0402 %s", extaddr, &shortaddr, &endpoint, profileid) == 4) {
			matchResponse(extaddr, shortaddr, endpoint, 0x0402 /* cluster */, profileid, zigbee_temperature->id);
			return;
		}
	// discovery response from Cluster 0x0402 - part 2 --> read ZigBee Basic Cluster Attributes 4,5,6 and 0x4000
		if (sscanf(stmp, "<-ZCL attr %x %d 0x0402 %x %[^\n]", &shortaddr, &endpoint, &attrid, attr) == 4) {
			matchAttributeResponse(shortaddr, endpoint, 0x0402 /* cluster */, attrid, attr, zigbee_temperature->id);
			return;
		}
	}
	if (foundtemp == 0 && foundhumidity == 0) {
		return;
	}
	for (i = 0; i < 2; i++) {
		foundvalue |= ((a[i]) << (i * 8));
	}
	tmp = zigbee_data;
	while(tmp) {
		if (tmp->extaddr != NULL
				&& strcmp(extaddr, tmp->extaddr) == 0
				&& tmp->endpoint == endpoint
				&& strcmp(tmp->protocol, zigbee_temperature->id) == 0) {
			shortaddr = (int) tmp->shortaddr;
			logfile = tmp->logfile;
			break;
		}
		tmp = tmp->next;
	}
	if (shortaddr == -1
			&& strlen(extaddr) > 0
			&& strlen(extaddr) <= 6) {
		sscanf(extaddr, "%x", &shortaddr);  // sometimes extaddr has the shortaddr
		tmp = zigbee_data;
		while (tmp) {
			if (tmp->shortaddr == shortaddr
					&& tmp->endpoint == endpoint
					&& strcmp(tmp->protocol, zigbee_temperature->id) == 0) {
				shortaddr = (int) tmp->shortaddr;
				logfile = tmp->logfile;
				break;
			}
			tmp = tmp->next;
		}
	}
	logprintf(LOG_DEBUG, "temperature shortaddr = 0x%04X, logfile = %s", shortaddr, logfile);
	if (shortaddr == -1) { // device not found
		return;
	}

	if (foundtemp == 1) {
		temperature = (int16_t) foundvalue;
		logprintf(LOG_DEBUG, "[ZigBee] zigbee_temperature parseCommand temperature = %d", temperature);
	}
	if (foundhumidity == 1) {
		humidity = (uint16_t) foundvalue;
		logprintf(LOG_DEBUG, "[ZigBee] zigbee_temperature parseCommand humidity = %d", humidity);
	}

	int x = snprintf(message, 255, "{\"shortaddr\":\"0x%04X\",", shortaddr);
	x += snprintf(&message[x], 255-x, "\"endpoint\":%d,", endpoint);
	if (foundtemp == 1) {
		x += snprintf(&message[x], 255-x, "\"temperature\": %.1f,", temperature / 100.0);
	}
	if (foundhumidity == 1) {
		x += snprintf(&message[x], 255-x, "\"humidity\": %.0f,", humidity / 100.0);
	}
	x += snprintf(&message[x-1], 255-x, "}");

	broadcast_message(message, zigbee_temperature->id);

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
		if (foundtemp == 1) {
			x += snprintf(&content[x], 200-x, " %.1f", temperature / 100.0);
		} else {
			x += snprintf(&content[x], 200-x, " -");
		}
		if (foundhumidity == 1) {
			x += snprintf(&content[x], 200-x, " %.0f", humidity / 100.0);
		} else {
			x += snprintf(&content[x], 200-x, " -");
		}
		x += snprintf(&content[x], 200-x, "\n");
		fwrite(content, sizeof(char), strlen(content), fp);
		fclose(fp);
	}
}

static int createCode(struct JsonNode *code, char **message) {
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
	printf("\t Temperature and Humidity Sensor\n");
}
#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void zigbeeTemperatureInit(void) {
    protocol_register(&zigbee_temperature);
    protocol_set_id(zigbee_temperature, "zigbee_temperature");
    protocol_device_add(zigbee_temperature, "zigbee_temperature", "ZigBee Temperature and Humidity");
    zigbee_temperature->devtype = WEATHER;
    zigbee_temperature->hwtype = ZIGBEE;

	options_add(&zigbee_temperature->options, 's', "shortaddr", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, NULL);
	options_add(&zigbee_temperature->options, 'e', "endpoint", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_temperature->options, 'r', "read", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_temperature->options, 't', "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_temperature->options, 'h', "humidity", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_temperature->options, 'l', "logfile", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);

	options_add(&zigbee_temperature->options, 0, "extaddr", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, "[0][xX][0-9a-fA-F]{16}");
	options_add(&zigbee_temperature->options, 0, "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]{1,5}"); // allow 0 - 99999 seconds
	options_add(&zigbee_temperature->options, 0, "ProfileId", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_temperature->options, 0, "ManufacturerName", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_temperature->options, 0, "ModelIdentifier", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_temperature->options, 0, "DateCode", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);

	options_add(&zigbee_temperature->options, 0, "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&zigbee_temperature->options, 0, "humidity-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&zigbee_temperature->options, 0, "show-humidity", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&zigbee_temperature->options, 0, "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	zigbee_temperature->parseCommand=&parseCommand;
	zigbee_temperature->createCode=&createCode;
	zigbee_temperature->printHelp=&printHelp;
	zigbee_temperature->gc=&gc;
	zigbee_temperature->checkValues=&checkValues;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice);
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
    module->name = "zigbee_temperature";
    module->version = "0.1";
    module->reqversion = "7.0";
    module->reqcommit = NULL;
}

void init(void) {
	zigbeeTemperatureInit();
}
#endif
