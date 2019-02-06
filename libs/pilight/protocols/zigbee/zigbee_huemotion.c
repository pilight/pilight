/*
 * zigbee_huemotion.c
 *
 *  Created on: Jul 31, 2017
 *      Author: ma-ca
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../core/pilight.h"
#include "../../core/dso.h"
#include "../../core/common.h"
#include "../../core/log.h"
#include "../../hardware/zigbee.h"
#include "../protocol.h"
#include "zigbee_huemotion.h"

static struct zigbee_data_t *zigbee_data = NULL;
static struct zigbee_cmd_t *zigbee_commands = NULL;

static void *addDevice(int reason, void *param) {
	zigbee_data = addZigbeeDevice(param, zigbee_data, zigbee_commands, zigbee_huemotion->id);

	return NULL;
}

/* Cluster 0x0406 Occupancy Sensing // ZigBee Cluster Library 4.8
 * Occupancy attribute id: 0x0000, type id: 0x18, 8-bit bitmap, 1 = occupied, 0 = unoccupied
 * 0x0010 PIROccupiedToUnoccupiedDelay, type id: 0x21, Unsigned 16-bit integer
 * 0x0030 sensitivity,     type id: 0x20, Unsigned 8-bit (Manufacturer specific), 0 = low, 1 = medium, 2 = high
 * 0x0031 sensitivity-max, type id: 0x20, Unsigned 8-bit (Manufacturer specific)
 *
 * Cluster 0x0400 Illuminance Measurement
 * MeasuredValue attribute id: 0x0000, type id: 0x21, 16-bit unsigned integer
 * MeasuredValue = 10,000 x log10 Illuminance + 1
 * Illuminance in Lux
 *
 *
 * Cluster 0x0402 Temperature Measurement
 * Attribute id: 0x0000, type id: 0x29, Signed 16-bit integer MeasuredValue
 *
 * Cluster 0x0001 Power Configuration
 * Attribute id: 0x0020, type id: 0x20, Unsigned 8-bit integer BatteryVoltage
 * Attribute id: 0x0021, type id: 0x20, Unsigned 8-bit integer battery percentage
 *
 * Cluster 0x0000 Basic
 * Manufacturer specific (Hue motion only), Manufacturer Code: 0x100B
 * Attribute id: 0x0032, type id: 0x10, Boolean, testmode 1 = on, 0 = off
 * Attribute id: 0x0033, type id: 0x10, Boolean, ledindication
 */
static void parseCommand(struct JsonNode *code, char **pmessage) {
//	logprintf(LOG_DEBUG, "huemotion %s", json_stringify(code, NULL));
	char *message = *pmessage;
	struct zigbee_data_t *tmp = NULL;
	struct JsonNode *jmsg = NULL;
	char *stmp = NULL, *logfile = NULL;
	char extaddr[19];  // "0x" + 64-Bit extended Address (16 Bytes hex String) + '\0'
	char profileid[7]; // ZigBee Home Automation 0x0104 // ZigBee Light Link 0xC05E
	char attr[33];
	char content[200];
	int a[2], shortaddr = -1, endpoint = -1, attrid = -1, i = 0, foundany = 0;
	int foundzone = 0, foundlux = 0, foundtemp = 0, foundbat = 0, foundled = 0;
	int occupancy = 0, state = -1, oldstate = -1, mlux = 0, lux = 0, temperature = 0;
	int occupiedToUnoccupiedDelay = -1, bat = -1, tst = -1;

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

	if (// Cluster 0x0406 Occupancy Sensing
		sscanf(stmp, "<-APS attr %s %d 0x0406", extaddr, &endpoint) != 2
		&& sscanf(stmp, "<-ZCL serverToClient %s for cluster 0x0406", extaddr) != 1
		&& sscanf(stmp, "<-ZCL attribute report %s 0x0406",  extaddr) != 1
		&& sscanf(stmp, "<-ZDP match %s %x %d 0x0406 %s", extaddr, &shortaddr, &endpoint, profileid) != 4
		&& sscanf(stmp, "<-ZCL attr %x %d 0x0406 %x %[^\n]", &shortaddr, &endpoint, &attrid, attr) != 4
		// Cluster 0x0400 Illuminance Measurement
		&& sscanf(stmp, "<-APS attr %s %d 0x0400", extaddr, &endpoint) != 2
		&& sscanf(stmp, "<-ZCL attribute report %s 0x0400",  extaddr) != 1
		// Cluster 0x0402 Temperature Measurement
		&& sscanf(stmp, "<-APS attr %s %d 0x0402", extaddr, &endpoint) != 2
		&& sscanf(stmp, "<-ZCL attribute report %s 0x0402",  extaddr) != 1
		// Cluster 0x0001 Power Configuration
		&& sscanf(stmp, "<-APS attr %s %d 0x0001", extaddr, &endpoint) != 2
		&& sscanf(stmp, "<-ZCL attribute report %s 0x0001",  extaddr) != 1
		// Cluster 0x0000 Basic Cluster Manufacturer Attribute 0x0032 type 0x10
		&& sscanf(stmp, "<-APS attr %s %d 0x0000 0x0032 0x10", extaddr, &endpoint) != 2
		&& sscanf(stmp, "<-ZCL attribute report %s 0x0000 %d 0x0032 00 00 10",  extaddr, &endpoint) != 2) {
		return;  // only cluster 0x0406 or 0x0400 or 0x0402 or 0x0001 or 0x0000
	}
	// Occupancy 8-bit bitmap
	// <-APS attr 0x0017880102035825 2 0x0406 0x0000 0x18 00 10 00 00 21 00 00
	if (foundzone == 0 && sscanf(stmp, "<-APS attr %s %d 0x0406 0x0000 0x18 %x 10 00 00 21 %x %x",
			extaddr, &endpoint, &occupancy, &a[0], &a[1]) == 5) {
		occupiedToUnoccupiedDelay = 0;
		for (i = 0; i < 2; i++) {
			occupiedToUnoccupiedDelay |= ((a[i]) << (i * 8));
		}
		foundzone = 1;
	}
	// <-APS attr 0x00178801xxxxxxxx 2 0x0406 0x0000 0x18 00
	if (foundzone == 0 && sscanf(stmp, "<-APS attr %s %d 0x0406 0x0000 0x18 %x",
			extaddr, &endpoint, &occupancy) == 3) {
		foundzone = 1;
	}
	// <-ZCL attribute report 0x0017880102035825 0x0406 2 00 00 18 01
	if (foundzone == 0 && sscanf(stmp, "<-ZCL attribute report %s 0x0406 %d 00 00 18 %x", extaddr, &endpoint, &occupancy) == 3) {
		foundzone = 1;
	}
	// discovery response from Cluster 0x0406 - part 1
	if (foundzone == 0) {
		if (sscanf(stmp, "<-ZDP match %s %x %d 0x0406 %s", extaddr, &shortaddr, &endpoint, profileid) == 4) {
			matchResponse(extaddr, shortaddr, endpoint, 0x0406 /* cluster */, profileid, zigbee_huemotion->id);
			return;
		}
	// discovery response from Cluster 0x0406  --> read ZigBee Basic Cluster Attributes 4,5,6 and 0x4000
		if (sscanf(stmp, "<-ZCL attr %x %d 0x0406 %x %[^\n]", &shortaddr, &endpoint, &attrid, attr) == 4) {
			matchAttributeResponse(shortaddr, endpoint, 0x0406 /* cluster */, attrid, attr, zigbee_huemotion->id);
			return;
		}
	} else {
		foundany = 1;
	}

	// Illuminance 16-bit unsigned integer
	// <-APS attr 0x00178801xxxxxxxx 2 0x0400 0x0000 0x21 AB 21
	if (foundany == 0
			&& sscanf(stmp, "<-APS attr %s %d 0x0400 0x0000 0x21 %x %x", extaddr, &endpoint, &a[0], &a[1]) == 4) {
		for (i = 0; i < 2; i++) {
			mlux |= ((a[i]) << (i * 8));
		}
		foundlux = 1;
		foundany = 1;
	}
	// <-ZCL attribute report 0x00178801xxxxxxxx 0x0400 2 00 00 21 1B 06
	if (foundany == 0 && sscanf(stmp, "<-ZCL attribute report %s 0x0400 %d 00 00 21 %x %x", extaddr, &endpoint, &a[0], &a[1]) == 4) {
		for (i = 0; i < 2; i++) {
			mlux |= ((a[i]) << (i * 8));
		}
		foundlux = 1;
		foundany = 1;

	}
	if (foundlux == 1) {
		// MeasuredValue represents the Illuminance in Lux (symbol lx) as follows:
		// MeasuredValue = 10,000 x log10 Illuminance + 1
		// => Illuminance = 10 ** ( (MeasuredValue - 1)/10000 )
		lux = pow(10, (mlux - 1.0)/10000.0);
	}

	// Temperature
	// <-ZCL attribute report 0x00158D00xxxxxxxx 0x0402 1 00 00 29 3F 09
	if (foundany == 0 && sscanf(stmp, "<-ZCL attribute report %s 0x0402 %d 00 00 29 %x %x", extaddr, &endpoint, &a[0], &a[1]) == 4) {
		for (i = 0; i < 2; i++) {
			temperature |= ((a[i]) << (i * 8));
		}
		foundtemp = 1;
		foundany = 1;
	}
	// <-APS attr 0x00178801xxxxxxxx 2 0x0402 0x0000 0x29 D3 09
	if (foundany == 0 && sscanf(stmp, "<-APS attr %s %d 0x0402 0x0000 0x29 %x %x",
			extaddr, &endpoint, &a[0], &a[1]) == 4) {
		for (i = 0; i < 2; i++) {
			temperature |= ((a[i]) << (i * 8));
		}
		foundtemp = 1;
		foundany = 1;
	}

	// Power Configuration Battery state
	// <-APS attr 0x00178801xxxxxxxx 2 0x0001 0x0021 0x20 C8
	if (foundbat == 0 && sscanf(stmp, "<-APS attr %s %d 0x0001 0x0021 0x20 %x",
			extaddr, &endpoint, &bat) == 3) {
		foundbat = 1;
		foundany = 1;
	}
	// <-ZCL attribute report 0x0017880102035825 0x0001 2 21 00 20 C8
	if (foundbat == 0 && sscanf(stmp, "<-ZCL attribute report %s 0x0001 %d 21 00 20 %x",
			extaddr, &endpoint, &bat) == 3) {
		foundbat = 1;
		foundany = 1;
	}
	// Basic Cluster Manufacturer Attribute 0x0032 testmode 0 = off (default), 1 = on
	// <-APS attr 0x00178801xxxxxxxx 2 0x0000 0x0032 0x10 01
	// <-APS attr 0x00178801xxxxxxxx 2 0x0000 0x0032 0x10 00
	if (foundled == 0 && sscanf(stmp, "<-APS attr %s %d 0x0000 0x0032 0x10 %x",
			extaddr, &endpoint, &tst) == 3) {
		foundled = 1;
		foundany = 1;
	}
	if (foundled == 0 && sscanf(stmp, "<-ZCL attribute report %s 0x0000 %d 32 00 10 %x",
			extaddr, &endpoint, &tst) == 3) {
		foundled = 1;
		foundany = 1;
	}

	if (foundany == 0) {
		return;
	}
	tmp = zigbee_data;
	while(tmp) {
		if (tmp->extaddr != NULL && strcmp(extaddr, tmp->extaddr) == 0 && tmp->endpoint == endpoint) {
			shortaddr = (int) tmp->shortaddr;
			oldstate = tmp->currentstate;
			logfile = tmp->logfile;
			break;
		}
		tmp = tmp->next;
	}
	if (shortaddr == -1 && strlen(extaddr) > 0 && strlen(extaddr) <= 6) {
		sscanf(extaddr, "%x", &shortaddr);  // sometimes extaddr has the shortaddr
		tmp = zigbee_data;
		while(tmp) {
			if (tmp->shortaddr == shortaddr && tmp->endpoint == endpoint) {
				oldstate = tmp->currentstate;
				logfile = tmp->logfile;
				break;
			}
			tmp = tmp->next;
		}
	}
	logprintf(LOG_DEBUG, "huemotion shortaddr = 0x%04X, logfile = %s", shortaddr, logfile);
	if (shortaddr == -1) { // device not found
		return;
	}
	if (foundzone == 1) {
		state = occupancy;
		logprintf(LOG_DEBUG, "[ZigBee] zigbee_huemotion parseCommand occupancy = 0x%02X, state = %d", occupancy, state);
	}
	if (occupiedToUnoccupiedDelay > -1) {
		logprintf(LOG_DEBUG, "[ZigBee] zigbee_huemotion parseCommand occupiedToUnoccupiedDelay = %d", occupiedToUnoccupiedDelay);
	}
	if (foundlux == 1) {
		logprintf(LOG_DEBUG, "[ZigBee] zigbee_huemotion parseCommand illuminance measured value = %d, lux = %d", mlux, lux);
	}
	if (foundtemp == 1) {
		logprintf(LOG_DEBUG, "[ZigBee] zigbee_huemotion parseCommand temperature = %d", temperature);
	}
	if (foundbat == 1) {
		bat = bat / 2;
		logprintf(LOG_DEBUG, "[ZigBee] zigbee_huemotion parseCommand battery = %d", bat);
	}

	if (state != -1) {
		state = newstate(shortaddr, endpoint, state, zigbee_data);
		if (state == oldstate) { // only broadcast if state has changed
			return;
		}
	}

	int x = snprintf(message, 255, "{\"shortaddr\":\"0x%04X\",", shortaddr);
	x += snprintf(&message[x], 255-x, "\"endpoint\":%d,", endpoint);
	if (foundzone == 1) {
		x += snprintf(&message[x], 255-x, "\"occupancy\": %d,", occupancy);
	}
	if (occupiedToUnoccupiedDelay > -1) {
		x += snprintf(&message[x], 255-x, "\"OccupiedToUnoccupiedDelay\": %d,", occupiedToUnoccupiedDelay);
	}
	if (foundlux == 1) {
		x += snprintf(&message[x], 255-x, "\"illuminance\": %d,", lux);
	}
	if (foundtemp == 1) {
		x += snprintf(&message[x], 255-x, "\"temperature\": %d,", temperature);
	}
	if (foundbat == 1) {
		x += snprintf(&message[x], 255-x, "\"battery\": %d,", bat);
	}
	if (foundled == 1) {
		x += snprintf(&message[x], 255-x, "\"testmode\": %d,", tst);
	}
	if (state == 1) {
		x += snprintf(&message[x], 255-x, "\"state\":\"on\",");
	} else if (state == 0) {
		x += snprintf(&message[x], 255-x, "\"state\":\"off\",");
	}
	x += snprintf(&message[x-1], 255-x, "}");

	broadcast_message(message, zigbee_huemotion->id);

	if (foundtemp == 0 && foundlux == 0) {
		return; // only write logfile for temperature and lux
	}
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
		if (foundlux == 1) {
			x += snprintf(&content[x], 200-x, " %d", lux);
		} else {
			x += snprintf(&content[x], 200-x, " -");
		}
		x += snprintf(&content[x], 200-x, "\n");
		fwrite(content, sizeof(char), strlen(content), fp);
		fclose(fp);
	}
}

/*
 * configure binding and reporting on Hue motion device
 * binding on endpoint 0x02 on cluster to coordinator
 * 		0x0406 cluster occupancy
 * 		0x0400 cluster illumincance
 * 		0x0402 cluster temperature
 * 		0x0001 cluster power configuration
 * reporting on cluster 0x0406, 0x0400, 0x0402, 0x0001
 * 		0x0406 attribute 0x0000 type 0x18 minimum  1 sec maximum  600 sec
 *		0x0400 attribute 0x0000 type 0x21 minimum 60 sec maximum  600 sec onchange 2000
 *		0x0402 attribute 0x0000 type 0x21 minimum 60 sec maximum  600 sec onchange 20
 * 		0x0001 attribute 0x0021 type 0x20 minimum 7200 sec maximum 7200 sec onchange 0xFF
 */
static void autoconfigure(int shortaddr) {
	char *dstextaddr = NULL;
	char *srcextaddr = NULL;
	char command[MAX_STRLENGTH];
	int hueep = 0x02;  // endpoint on Hue motion
	int dstep = 0x01;  // destination endpoint on coordinator (RaspBee)
	int i = 0, sec = 5;
	int dobind = 0x0021; // bind

	// get coordinator extended address from short address 0x0000
	dstextaddr = getextaddr(0x0000);
	if (dstextaddr == NULL) {
		logprintf(LOG_ERR, "[ZigBee] zigbee_huemotion autoconfigure coordinator not found. Try again later");
		return;
	}
	// get hue motion extended address from short address
	srcextaddr = getextaddr(shortaddr);
	if (srcextaddr == NULL) {
		logprintf(LOG_ERR, "[ZigBee] zigbee_huemotion autoconfigure hue motion address not found. Try again later");
		return;
	}

	// identify 15 seconds
	sendIdentifyCmd(shortaddr, dstep, 15);

	// set testmode on
	memset(command, '\0', MAX_STRLENGTH);
	snprintf(command, MAX_STRLENGTH, "zclattrmanu 0x%04X %d 0x0000 0x100B 0232001001", shortaddr, hueep);
	addsinglethread(command, sec++);

	// bind cluster 0x0406, 0x0400, 0x0402, 0x0001
	char cluster[4][7] = { "0x0406", "0x0400", "0x0402", "0x0001" };
	for (i = 0; i < 4; i++) {
		sendBindCmd(shortaddr, hueep, dstep, dobind, cluster[i], srcextaddr, dstextaddr, sec);
		sec += 2;
	}
	// configure reporting on cluster 0x0406, 0x0400, 0x0402, 0x0001
	char repattr[4][5] = { "0000", "0000", "0000", "2100" };
	char repconf[4][15] = { "1801005802", "213C005802D007", "213C0058021400", "20201C201CFF" };

	for (i = 0; i < 4; i++) {
		memset(command, '\0', MAX_STRLENGTH);
		snprintf(command, MAX_STRLENGTH, "zclattr 0x%04X %d %s 0600%s%s",
				shortaddr, hueep, cluster[i], repattr[i], repconf[i]);
		addsinglethread(command, sec);
		sec += 2;
	}
	// finally check result and show bindings
	memset(command, '\0', MAX_STRLENGTH);
	snprintf(command, MAX_STRLENGTH, "zdpcmd 0x%04X 0x0033 00", shortaddr);
	addsinglethread(command, sec);
	sec += 2;
	// show bindings starting from index 3
	snprintf(command, MAX_STRLENGTH, "zdpcmd 0x%04X 0x0033 03", shortaddr);
	addsinglethread(command, sec);
	sec += 2;
	// and show reporting configuration
	for (i = 0; i < 4; i++) {
		memset(command, '\0', MAX_STRLENGTH);
		snprintf(command, MAX_STRLENGTH, "zclattr 0x%04X %d %s 0800%s",
				shortaddr, hueep, cluster[i], repattr[i]);
		addsinglethread(command, sec);
		sec += 2;
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
    	logprintf(LOG_ERR, "zigbee_huemotion: insufficient number of arguments");
    	printf("zigbee_huemotion: insufficient number of arguments\n");
    	printf("code %s stmp = %s, shortAddr = %x, endpoint = %d\n", json_stringify(code, "\t"), stmp, shortAddr, endpoint);
    	return EXIT_FAILURE;
    }
    if (json_find_number(code, "autoconfigure", &itmp) == 0) {
    	printf("Philips Hue Motion sensor -- starting auto configuration\n");
    }

    if (pilight.process == PROCESS_DAEMON) {

    	if (json_find_number(code, "read", &itmp) == 0) {
    		snprintf(command, MAX_STRLENGTH, "zclattr 0x%04X %d 0x0406 0000001000", shortAddr, endpoint);
    		addsinglethread(command, 0);
    		snprintf(command, MAX_STRLENGTH, "zclattr 0x%04X %d 0x0400 000000", shortAddr, endpoint);
    		addsinglethread(command, 5);
    		snprintf(command, MAX_STRLENGTH, "zclattr 0x%04X %d 0x0402 000000", shortAddr, endpoint);
    		addsinglethread(command, 10);
    	} else if (json_find_number(code, "OccupiedToUnoccupiedDelay", &itmp) == 0) {
    		snprintf(command, MAX_STRLENGTH, "zclattr 0x%04X %d 0x0406 02100021%04X", shortAddr, endpoint, (int) itmp);
    		zigbeeSendCommand(command);  // write attribute id 0x0010 OccupiedToUnoccupiedDelay
    	} else if (json_find_number(code, "testmode", &itmp) == 0) {
    		// zclattrmanu 0xE5BB 2 0x0000 0x100B 003200      // read
    		// zclattrmanu 0xE5BB 2 0x0000 0x100B 0232001001  // write
    		snprintf(command, MAX_STRLENGTH, "zclattrmanu 0x%04X %d 0x0000 0x100B 02320010%02X", shortAddr, endpoint, (int) itmp);
    		zigbeeSendCommand(command);  // write manufacturer attribute id 0x0032 testmode
       	} else if (json_find_number(code, "autoconfigure", &itmp) == 0) {
       		logprintf(LOG_INFO, "[ZigBee] Philips Hue Motion sensor -- starting auto configuration\n");
       		autoconfigure(shortAddr);
       	} else {
    		logprintf(LOG_ERR, "[ZigBee] zigbee_huemotion insufficient number of arguments");
    		printf("zigbee_huemotion: insufficient number of arguments\n");
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
	printf("\t -r --read\t\t\tread attribute values from Hue motion sensor\n");
	printf("\t -a --autoconfigure \t\tconfigure bindings and reporting\n");
	printf("\t -s --shortaddr\n");
	printf("\t -e --endpoint\n");
	printf("\t -m --testmode \t\t\t0 = off (default), 1 = on (led is green when motion is detected, testmode is turned off after 2 minutes)\n");
	printf("\t -u --OccupiedToUnoccupiedDelay \n");
	printf("\t -v --sensitivity\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void zigbeeHuemotionInit(void) {
    protocol_register(&zigbee_huemotion);
    protocol_set_id(zigbee_huemotion, "zigbee_huemotion");
    protocol_device_add(zigbee_huemotion, "zigbee_huemotion", "ZigBee Philips Hue motion sensor");
    zigbee_huemotion->devtype = MOTION;
    zigbee_huemotion->hwtype = ZIGBEE;

	options_add(&zigbee_huemotion->options, 's', "shortaddr", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, NULL);
	options_add(&zigbee_huemotion->options, 'e', "endpoint", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_huemotion->options, 'x', "occupancy", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_huemotion->options, 'y', "illuminance", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_huemotion->options, 'z', "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_huemotion->options, 'm', "testmode", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, (void *)0, "[01]"); // allow 0,1
	options_add(&zigbee_huemotion->options, 'b', "battery", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_huemotion->options, 'l', "logfile", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_huemotion->options, 'u', "OccupiedToUnoccupiedDelay", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_huemotion->options, 'v', "sensitivity", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, (void *)2, "[012]"); // allow 0,1,2
	options_add(&zigbee_huemotion->options, 'r', "read", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_huemotion->options, 'a', "autoconfigure", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_huemotion->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_huemotion->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);

	options_add(&zigbee_huemotion->options, 0, "extaddr", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, "[0][xX][0-9a-fA-F]{16}");
	options_add(&zigbee_huemotion->options, 0, "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]{1,5}"); // allow 0 - 99999 seconds
	options_add(&zigbee_huemotion->options, 0, "ProfileId", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_huemotion->options, 0, "ManufacturerName", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_huemotion->options, 0, "ModelIdentifier", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_huemotion->options, 0, "DateCode", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_huemotion->options, 0, "Version", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);

	zigbee_huemotion->parseCommand=&parseCommand;
	zigbee_huemotion->createCode=&createCode;
	zigbee_huemotion->printHelp=&printHelp;
	zigbee_huemotion->gc=&gc;
	zigbee_huemotion->checkValues=&checkValues;

	// predefined commands
	zigbee_cmd_t *zcmd = NULL;
	if ((zcmd = MALLOC(sizeof(struct zigbee_cmd_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(zcmd, '\0', sizeof(struct zigbee_cmd_t));
	if ((zcmd->command = MALLOC(strlen("zclattr 0x%04X %d 0x0406 0000001000") + 1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(zcmd->command, "zclattr 0x%04X %d 0x0406 0000001000");  // read occupancy
	zcmd->next = zigbee_commands;
	zigbee_commands = zcmd;

	if ((zcmd = MALLOC(sizeof(struct zigbee_cmd_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(zcmd, '\0', sizeof(struct zigbee_cmd_t));
	if ((zcmd->command = MALLOC(strlen("zclattr 0x%04X %d 0x0400 000000") + 1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(zcmd->command, "zclattr 0x%04X %d 0x0400 000000");  // read illuminace
	zcmd->next = zigbee_commands;
	zigbee_commands = zcmd;

	if ((zcmd = MALLOC(sizeof(struct zigbee_cmd_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(zcmd, '\0', sizeof(struct zigbee_cmd_t));
	if ((zcmd->command = MALLOC(strlen("zclattr 0x%04X %d 0x0402 000000") + 1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(zcmd->command, "zclattr 0x%04X %d 0x0402 000000");  // read temperature
	zcmd->next = zigbee_commands;
	zigbee_commands = zcmd;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice);
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
    module->name = "zigbee_huemotion";
    module->version = "0.1";
    module->reqversion = "7.0";
    module->reqcommit = NULL;
}

void init(void) {
	zigbeeHuemotionInit();
}
#endif
