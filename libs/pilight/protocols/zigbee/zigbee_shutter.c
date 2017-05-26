/*
 * zigbee_shutter.c
 *
 *  Created on: Sep 8, 2017
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
#include "zigbee_shutter.h"

/*
 * Ubisys Shutter Control J1
 *
 * ZigBee Home Automation Public Application Profile Document 05-3520-29
 * Chapter 9.3 Window Covering Cluster
 *
 * Cluster Id 0x0102 Window Covering Cluster
 * Attributes
 * 0x0000 enum8,      WindowCoveringType
 * 0x0003 unsinged16, CurrentPositionLift
 * 0x0004 unsinged16, CurrentPositionTilt
 * 0x0008 unsinged8,  CurrentPositionLiftPercentage
 * 0x0009 unsinged8,  CurrentPositionTiltPercentage
 * 0x000A bitmap8,    OperationalStatus (This attribute contains two bits which will be set while the motor is active)
 * 0x0011 unsinged16, InstalledClosedLimitLift (Specifies a bound for the bottom position (lift height), in centimeters)
 * 0x0013 unsinged16, InstalledClosedLimitTilt (Specifies a bound for the closed position (tilt angle), in units of 0.1°)
 * 0x0017 bitmap8,    Mode (bit0=if the motor direction is reversed, bit1=the device is in calibration, bit2=maintenance mode)
 *
 * Commands
 * 0x00 Move up/open, Move upwards, towards the fully open position.
 * 0x01 Move down/close, Move downwards, towards the fully closed position.
 * 0x02 Stop, Stop all motion.
 * 0x04 Go to Lift Value, Moves to the specified lift value. Unsigned 16-bit integer.
 * 0x05 Go to Lift Percentage, Moves to the specified lift percentage. Unsigned 8-bit integer.
 * 0x07 Go to Tilt Value, Move to the specified tilt value. Unsigned 16-bit integer.
 * 0x08 Go to Tilt Percentage, Move to the specified tilt percentage. Unsigned 8-bit integer.
 *
 */

static struct zigbee_data_t *zigbee_data = NULL;
static struct zigbee_cmd_t *zigbee_commands = NULL;

char shutterattr[9][30] = { "WindowCoveringType", "CurrentPositionLift",
		"CurrentPositionTilt", "CurrentPositionLiftPercentage", "CurrentPositionTiltPercentage",
		"OperationalStatus", "InstalledClosedLimitLift", "InstalledClosedLimitTilt", "Mode" };
int shutterattrid[9] = {0x0000, 0x0003, 0x0004, 0x0008, 0x0009, 0x000A, 0x0011, 0x0013, 0x0017};

static void *addDevice(int reason, void *param) {
	zigbee_data = addZigbeeDevice(param, zigbee_data, zigbee_commands, zigbee_shutter->id);

	return NULL;
}

static int createBindingsAndReporting(int shortaddr, int ep, int *sec) {
	char *dstextaddr = NULL;
	char *srcextaddr = NULL;
	char command[MAX_STRLENGTH];
	int dstep = 0x01;  // destination endpoint on coordinator (RaspBee)
	int i = 0;
	int dobind = 0x0021; // bind

	// get coordinator extended address from short address 0x0000
	dstextaddr = getextaddr(0x0000);
	if (dstextaddr == NULL) {
		logprintf(LOG_ERR, "[ZigBee] zigbee_shutter coordinator address not found. Try again later");
		return 1;
	}
	// get extended address from short address
	srcextaddr = getextaddr(shortaddr);
	if (srcextaddr == NULL) {
		logprintf(LOG_ERR, "[ZigBee] zigbee_shutter extended address not found. Try again later");
		return 1;
	}

	// bind cluster 0x0102
	char cluster[7] = "0x0102";
	memset(command, '\0', MAX_STRLENGTH);
	sendBindCmd(shortaddr, ep, dstep, dobind, cluster, srcextaddr, dstextaddr, (*sec)++);
	// configure reporting on attributes 0x0003 0x0004 0x0008 0x0009 0x000A
	// min reporting 60 seconds, max reporting 600 seconds
	char repattr[5][5] = { "0300", "0400", "0800", "0900", "0A00" };
	char repconf[5][15] = { "213C0058020100", "213C0058020100", "203C00580201", "203C00580201", "183C005802" };

	memset(command, '\0', MAX_STRLENGTH);

	// configure reporting command 0x06, Direction 0x00
	int x = snprintf(command, MAX_STRLENGTH, "zclattr 0x%04X %d %s 06",
			shortaddr, ep, cluster);
	for (i = 0; i < 5; i++) {
		x += snprintf(&command[x], MAX_STRLENGTH, "00%s%s", repattr[i], repconf[i]);
	}
	addsinglethread(command, (*sec)++);
	// finally check result and show bindings
	memset(command, '\0', MAX_STRLENGTH);
	snprintf(command, MAX_STRLENGTH, "zdpcmd 0x%04X 0x0033 00", shortaddr);
	addsinglethread(command, (*sec)++);
	// and show reporting configuration
	for (i = 0; i < 5; i++) {
		memset(command, '\0', MAX_STRLENGTH);
		snprintf(command, MAX_STRLENGTH, "zclattr 0x%04X %d %s 0800%s",
				shortaddr, ep, cluster, repattr[i]);
		addsinglethread(command, *sec);
		*sec += 2;
	}
	return 0;
}

static void nextstep(int shortaddr, int endpoint, int newstep) {
	struct zigbee_data_t *tmp = zigbee_data;
	while(tmp) {
		if (shortaddr == tmp->shortaddr && endpoint == tmp->endpoint) {
			tmp->step = newstep;

			break;
		}
		tmp = tmp->next;
	}
}

/*
 * http://www.ubisys.de/downloads/ubisys-j1-technical-reference.pdf
 * page 18, chapter 7.2.5.1. Calibration
 *
 * Step 1
 *    In order to calibrate the device, first choose the appropriate device type.
 *    0 = Roller Shade Lift only, ..., 6 = Shutter Tilt only, ..., 8 Tilt Blind Lift & Tilt
 *    Write attribute 0x10F2:0x0000 (“WindowCoveringType”) accordingly.
 * Step 2
 *    Prepare calibration by setting these values:
 *    Write attribute 0x10F2:0x0010 (“InstalledOpenLimitLift”) as 0x0000 = 0cm.
 *    Write attribute 0x10F2:0x0011 (“InstalledClosedLimitLift”) as 0x00F0 = 240cm.
 *    Write attribute 0x10F2:0x0012 (“InstalledOpenLimitTilt”) as 0x0000 = 0°.
 *    Write attribute 0x10F2:0x0013 (“InstalledClosedLimitTilt”) as 0x0384 = 90.0°.
 *    Write attribute 0x10F2:0x1001 (“LiftToTiltTransitionSteps”) as 0xFFFF = invalid.
 *    Write attribute 0x10F2:0x1002 (“TotalSteps”) as 0xFFFF = invalid.
 *    Write attribute 0x10F2:0x1003 (“LiftToTiltTransitionSteps2”) as 0xFFFF = invalid.
 *    Write attribute 0x10F2:0x1004 (“TotalSteps2”) as 0xFFFF = invalid
 * Step 3
 *    Enter calibration mode:
 *    Write attribute 0x0017 (“Mode”) as 0x02.
 * Step 4
 *    Send the "move down" command and "stop" after a few centimeters.
 * Step 5
 *    Send the “move up” command. When the device reaches its top position,
 *    J1 will recognize the upper bound.
 * Step 6
 *    After J1 has reached the top position and the motor has stopped, send the “move down” command.
 * Step 7
 *    After J1 has reached the lower bound and the motor has stopped, send the “move up” command.
 *    J1 will search for the upper bound. Once the top position is reached,
 *    calibration of the total steps in both directions is complete.
 * Step 8
 *    In case of a tilt blind set attribute 0x10F2:0x1001 and 0x10F2:0x1003 to the time it takes for a lift-to tilt
 *    transition (down) or a tilt-to-lift transition (up), respectively. Otherwise proceed with the next step.
 * Step 9
 *    To leave calibration mode, clear bit #1 in the Mode attribute, e.g. write attribute 0x0017 as 0x00.
 *
 */
static void calibrate(int shortAddr, int endpoint, int step) {
	int sec = 0, newstep = 0;
	char command[MAX_STRLENGTH];
	memset(command, '\0', sizeof(command));
	if (step == 1) {
		// Create binding and configure reporting
		if (createBindingsAndReporting(shortAddr, endpoint, &sec)) {
			logprintf(LOG_ERR, "[ZigBee] createBindingsAndReporting failed and calibration aborted on 0x%04X, %d",
					shortAddr, endpoint);
			return;
		}

		// Step 1,2 send attribute write zclattrmanu <ep> <cluster> <manufacturer id> <command>
		int x = sprintf(command, "zclattrmanu %04X %d 0x0102 0x10F2 02", shortAddr, endpoint);  // write command 02
		x += sprintf(&command[x], "1000210000"); // Write attribute 0x10F2:0x0010 as 0x0000 = 0cm, typeid = 0x21
		x += sprintf(&command[x], "110021F000"); // Write attribute 0x10F2:0x0011 as 0x00F0 = 240cm, typeid = 0x21
		x += sprintf(&command[x], "1200210000"); // Write attribute 0x10F2:0x0012 as 0x0000 = 0°, typeid = 0x21
		x += sprintf(&command[x], "1300218403"); // Write attribute 0x10F2:0x0013 as 0x0384 = 90.0°, typeid = 0x21
		x += sprintf(&command[x], "011021FFFF"); // Write attribute 0x10F2:0x1001 as 0xFFFF = invalid, typeid = 0x21
		x += sprintf(&command[x], "021021FFFF"); // Write attribute 0x10F2:0x1002 as 0xFFFF = invalid, typeid = 0x21
		x += sprintf(&command[x], "031021FFFF"); // Write attribute 0x10F2:0x1003 as 0xFFFF = invalid, typeid = 0x21
		x += sprintf(&command[x], "041021FFFF"); // Write attribute 0x10F2:0x1004 as 0xFFFF = invalid, typeid = 0x21
		addsinglethread(command, sec);
		sec += 2;

		// Step 3 enter calibration mode
		memset(command, '\0', sizeof(command));
		sprintf(command, "zclattr %04X %d 0x0102 0217001802", shortAddr, endpoint); // Write attribute 0x0017 as 0x02, typeid = 0x18
		addsinglethread(command, sec);
		sec += 2;

		// Step 4 send move down command a few centimeters
		memset(command, '\0', sizeof(command));
		sprintf(command, "zclcmd %04X %d 0x0102 01", shortAddr, endpoint); // Move down
		addsinglethread(command, sec);
		sec += 2;

		// Step 4a send stop command
		memset(command, '\0', sizeof(command));
		sprintf(command, "zclcmd %04X %d 0x0102 02", shortAddr, endpoint); // Stop
		addsinglethread(command, sec);
		sec += 2;

		// Step 5 send move up command until fully open
		memset(command, '\0', sizeof(command));
		sprintf(command, "zclcmd %04X %d 0x0102 00", shortAddr, endpoint); // Move up
		addsinglethread(command, sec);
		sec += 2;
		newstep = 6;
	} else if (step == 6) {
		// Step 6 send move down command until fully closed
		sprintf(command, "zclcmd %04X %d 0x0102 01", shortAddr, endpoint); // Move down
		addsinglethread(command, sec);
		newstep = 7;
	} else if (step == 7) {
		// Step 7 send move up command until fully open
		sprintf(command, "zclcmd %04X %d 0x0102 00", shortAddr, endpoint); // Move up
		addsinglethread(command, sec);
		newstep = 9;
	} if (step == 9) {
		// Step 9 write attribute 0x0017 as 0x00 leave calibration mode
		sprintf(command, "zclattr %04X %d 0x0102 0217001800", shortAddr, endpoint); // Write attribute 0x0017 as 0x00, typeid = 0x18
		addsinglethread(command, sec);
	} else {
		logprintf(LOG_ERR, "[ZigBee] invalid calibaration step %d", step);
	}
	nextstep(shortAddr, endpoint, newstep);
}

static void createStateMessage(char *message,
		int shortAddr,
		int endpointId,
		int *state,
		int *dimlevel,
		int shutterattrval[9],
		int i) {
	int /*i,*/ maxlen = 1000;
	int x = snprintf(message, maxlen, "{\"shortaddr\":\"0x%04X\",", shortAddr);
	x += snprintf(&message[x], maxlen - x, "\"endpoint\":%d,", endpointId);

//	for (i = 0; i < 9; i++) {
		if (shutterattrval[i] != 0xFFFFFF) {
			x += snprintf(&message[x], maxlen - x, "\"%s\":%d,", shutterattr[i], shutterattrval[i]);
			if (i == 3) {  // CurrentPositionLiftPercentage translates to dimlevel
				*dimlevel = shutterattrval[i];
				if (*dimlevel > 100) {
					*dimlevel = 100;
				}
				if (*dimlevel == 0) {
					*state = 0;
				} else {
					*state = 1;
				}
			}
		}
//	}

	if (*state == 1) {
		x += snprintf(&message[x], maxlen - x, "\"state\":\"on\",");
	} else if (*state == 0) {
		x += snprintf(&message[x], maxlen - x, "\"state\":\"off\",");
	}
	if (*dimlevel > -1) {
		x += snprintf(&message[x], maxlen - x, "\"dimlevel\": %d,", *dimlevel);
	}
	x += snprintf(&message[x-1], maxlen - x, "}");
}

static void readAttrValues(char *stmp,
		char *p,
		char *extaddr,
		int endpoint,
		int attrid,
		int attrtype,
		int shutterattrid[9],
		int (*shutterattrval)[9],
		int status) {
	int b[2], i;

	for (i = 0; i < 2; i++) {
		b[i] = 0;
	}
	char *pend = stmp + strlen(stmp);

	while (p < pend) {
		int attrindex = -1;
		int datalength = (attrtype & 0x07) + 1; // Bytes 1,2,..8
		for (i = 0; i < 9; i++) {
			if (attrid == shutterattrid[i]) {
				attrindex = i;
				break;
			}
		}
		if (attrindex == -1) {
			p += datalength * 3; // skip unknown attribute
			continue;
		}
		int attrval = 0xFFFFFF;
		switch(datalength) {
			case 1: {
				if (sscanf(p, "%x", &attrval) == 1) {

				}
			}; break;
			case 2: {
				if (sscanf(p, "%x %x", &b[0], &b[1]) == 2) {
					attrval = (b[1] << 8) | b[0];
				}
			}; break;
			default:
				break;
		}
		(*shutterattrval)[attrindex] = attrval;
		p += datalength * 3;
		if (pend - p > 12 && status) {
			// example: 00 04 00 2A = [attributeid] [status] [typeid]
			if (sscanf(p, "%x %x 00 %x", &b[0], &b[1], &attrtype) == 3)  {
				attrid = (b[1] << 8) | b[0];
				p += 12;
			}
		} else if (pend - p > 9) {
			if (sscanf(p, "%x %x %x", &b[0], &b[1], &attrtype) == 3)  {
				attrid = (b[1] << 8) | b[0];
				p += 9;
			}
		} else {
			p = pend;
			break;
		}
	}
}

static void parseCommand(struct JsonNode *code, char **pmessage) {
	char *message = *pmessage;
	struct zigbee_data_t *tmp = NULL;
	struct JsonNode *jmsg = NULL;
	char *stmp = NULL, attrstr[33];
	char extaddr[19];  // "0x" + 64-Bit extended Address (16 Bytes hex String) + '\0'
	char profileid[7]; // ZigBee Home Automation 0x0104 // ZigBee Light Link 0xC05E
	char strbuf[100];
	int shortaddr = -1, endpoint = -1, state = -1, dimlevel = -1;
	int b[2], attrid = -1, attrtype = -1, oldstate = -1;
	int calibrationmode = -1,  calibrationstep = -1, operationalstatus = -1;
	int itmp, found = 0, i;
	int shutterattrval[9];

	for (i = 0; i < 2; i++) {
		b[i] = 0;
	}
	for (i = 0; i < 9; i++) {
		shutterattrval[i] = 0xFFFFFF;
	}
	memset(extaddr, '\0', sizeof(extaddr));
	memset(profileid, '\0', sizeof(profileid));
	memset(strbuf, '\0', sizeof(strbuf));
	memset(attrstr, '\0', sizeof(attrstr));


	if((jmsg = json_find_member(code, "message")) != NULL) {
		if (json_find_string(jmsg, "zigbee_response", &stmp) == 0) { // response from raspbee
			if (// Cluster 0x0102 Window Covering Cluster
					sscanf(stmp,    "<-APS attr %s %d 0x0102 %x", extaddr, &endpoint, &itmp) != 3
					&& sscanf(stmp, "<-ZCL attribute report %s 0x0102 %d", extaddr, &itmp) != 2
					&& sscanf(stmp, "<-ZDP match %s %x %d 0x0102 %s", extaddr, &shortaddr, &endpoint, profileid) != 4
					&& sscanf(stmp, "<-ZCL attr %x %d 0x0102 %x %[^\n]", &shortaddr, &endpoint, &attrid, attrstr) != 4) {
				return;  // only cluster 0x0102
			}
		}
		// APS Attribute
		if (sscanf(stmp, "<-APS attr %s %d 0x0102 %x %x", extaddr, &endpoint, &attrid, &attrtype) == 4) {
			int x = sprintf(strbuf, "<-APS attr %s %d 0x0102 0x%04x 0x%02x", extaddr, endpoint, attrid, attrtype);
			char *p = stmp + x;
			readAttrValues(stmp, p, extaddr, endpoint, attrid, attrtype, shutterattrid, &shutterattrval, 1);
			found = 1;
			logprintf(LOG_DEBUG, "done readAttrValues");
		}
		// ZCL Attribute report
		if (found == 0 && sscanf(stmp, "<-ZCL attribute report %s 0x0102 %d %x %x %x ",
				extaddr, &endpoint, &b[0], &b[1], &attrtype) == 5) {
			int x = sprintf(strbuf, "<-ZCL attribute report %s 0x0102 %d %02x %02x %02x ", extaddr, endpoint, b[0], b[1], attrtype);
			char *p = stmp + x;
			attrid = (b[1] << 8) | b[0];
			readAttrValues(stmp, p, extaddr, endpoint, attrid, attrtype, shutterattrid, &shutterattrval, 0);
			found = 1;
		}

		// discovery response from Cluster 0x0102 - part 1
		if (found == 0) {
			if (sscanf(stmp, "<-ZDP match %s %x %d 0x0102 %s", extaddr, &shortaddr, &endpoint, profileid) == 4) {
				matchResponse(extaddr, shortaddr, endpoint, 0x0102 /* cluster */, profileid, zigbee_shutter->id);
				return;
			}

		// discovery response from Cluster 0x0102 - part 2 --> read ZigBee Basic Cluster Attributes 4,5,6 and 0x4000
			if (sscanf(stmp, "<-ZCL attr %x %d 0x0102 %x %[^\n]", &shortaddr, &endpoint, &attrid, attrstr) == 4) {
				matchAttributeResponse(shortaddr, endpoint, 0x0102 /* cluster */, attrid, attrstr, zigbee_shutter->id);
				return;
			}
		}

		tmp = zigbee_data;
		while(tmp) {
			if (tmp->extaddr != NULL && strcmp(extaddr, tmp->extaddr) == 0) {
				shortaddr = tmp->shortaddr;
				oldstate = tmp->currentstate;
				if (shutterattrval[8] != 0xFFFFFF) {  // Mode attribute 0x0017
					tmp->mode = shutterattrval[8];
				}
				calibrationmode = tmp->mode;
				calibrationstep = tmp->step;

				break;
			}
			tmp = tmp->next;
		}
	} else {   // end message
		return; // no message
	}

	if (shortaddr == -1 || endpoint == -1) {
		return; // device not found
	}
//	logprintf(LOG_DEBUG, "shortaddr = 0x%04X, ep = %d", shortaddr, endpoint);
//	for (i = 0; i < 9; i++) {
//		logprintf(LOG_DEBUG, "shutterattrval[%d] = %d (0x%X), %s",
//				i, shutterattrval[i], shutterattrval[i], shutterattr[i]);
//	}

	operationalstatus = shutterattrval[5];  // two bits which will be set while the motor is active

	if (calibrationmode == 2 && operationalstatus == 0) {
		logprintf(LOG_INFO, "[ZigBee] zigbee_shutter parseCommand continue calibration on shortaddr = 0x%04X, ep = %d",
				shortaddr, endpoint);
		// continue calibration steps when not moving anymore
		calibrate(shortaddr, endpoint, calibrationstep);
	}

//	logprintf(LOG_DEBUG, "1. message = %s", message);
//	createStateMessage(message, shortaddr, endpoint, &state, &dimlevel, shutterattrval);
//	logprintf(LOG_DEBUG, "2. message = %s", message);

	if (dimlevel == -1 && oldstate == newstate(shortaddr, endpoint, state, zigbee_data)) {
//		return; // only broadcast if state has changed or dimlevel has changed
	}
	for (i = 0; i < 9; i++) {
		if (shutterattrval[i] != 0xFFFFFF) {
			createStateMessage(message, shortaddr, endpoint, &state, &dimlevel, shutterattrval, i);
			broadcast_message(message, zigbee_shutter->id);
		}
	}
	createStateMessage(message, shortaddr, endpoint, &state, &dimlevel, shutterattrval, 3); // 3 = dimlevel
}

static int createCode(struct JsonNode *code, char **pmessage) {
	char *message = *pmessage;
	char command[MAX_STRLENGTH];
	char *stmp = NULL;
	double itmp = 0.0;
	int shortAddr = -1;
	int endpoint = -1/*, state = -1*/;
	int /*dimlevel = -1,*/ updownstopCmd = -1, moveValue = -1;
//	int shutterattrval[9], i;
//
//	for (i = 0; i < 9; i++) {
//		shutterattrval[i] = 0xFFFFFF;
//	}

	memset(command, '\0', sizeof(command));
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
	if (json_find_number(code, "on", &itmp) == 0) {
//		state = 1;
//		dimlevel = 100;
		updownstopCmd = 0x01;  // move down
	}
	if (json_find_number(code, "off", &itmp) == 0) {
//		state = 0;
//		dimlevel = 0;
		updownstopCmd = 0x00;  // move up
	}
	if(json_find_number(code, "dimlevel", &itmp) == 0) {
		updownstopCmd = 0x05;
		moveValue = (int) itmp;
	}

	if(json_find_number(code, "moveup", &itmp) == 0) {
		updownstopCmd = 0x00;
	}
	if(json_find_number(code, "movedown", &itmp) == 0) {
		updownstopCmd = 0x01;
	}
	if(json_find_number(code, "stop", &itmp) == 0) {
		updownstopCmd = 0x02;
	}
	if(json_find_number(code, "liftval", &itmp) == 0) {
		updownstopCmd = 0x04;
		moveValue = (int) itmp;
	}
	if(json_find_number(code, "liftpct", &itmp) == 0) {
		updownstopCmd = 0x05;
		moveValue = (int) itmp;
	}
	if(json_find_number(code, "tiltval", &itmp) == 0) {
		updownstopCmd = 0x07;
		moveValue = (int) itmp;
	}
	if(json_find_number(code, "tiltpct", &itmp) == 0) {
		updownstopCmd = 0x08;
		moveValue = (int) itmp;
	}
	if(json_find_number(code, "calibrate", &itmp) == 0) {
		printf("ZigBee starting calibrate\n");
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

    if (pilight.process == PROCESS_DAEMON) {

        if (json_find_number(code, "read", &itmp) == 0) {
        	snprintf(command, MAX_STRLENGTH, "zclattr 0x%04X %d 0x0102 00000003000400080009000A00110013001700", shortAddr, endpoint);
        	addsinglethread(command, 0);  // read attributes
        } else if (json_find_number(code, "identify", &itmp) == 0) {
        	snprintf(command, MAX_STRLENGTH, "zclcmd 0x%04X %d 0x0003 000400", shortAddr, endpoint);
        	zigbeeSendCommand(command);   // identify command 4 seconds
        } else if (json_find_number(code, "autodiscover", &itmp) == 0) {
        	matchtime = 1;
        	snprintf(command, MAX_STRLENGTH, "m 0x0104 0x0102");  // match ZHA LEVEL_CONTROL_CLUSTER
        	addsinglethread(command, 0);  // send match request ZigBee HA
        } else if (json_find_number(code, "calibrate", &itmp) == 0) {
        	logprintf(LOG_DEBUG, "[ZigBee] zigbee_shutter starting calibrate");
        	calibrate(shortAddr, endpoint, /* step 1 */ 1);
        } else if (updownstopCmd == 0x05 || updownstopCmd == 0x08) {  // Moves to the specified lift/tilt percentage
        	snprintf(command, MAX_STRLENGTH, "zclcmd 0x%04X %d 0x0102 %02X%02X",
        			shortAddr, endpoint, updownstopCmd, moveValue);
        	zigbeeSendCommand(command);
//        	dimlevel = moveValue;
        	//createStateMessage(message, shortAddr, endpoint, &state, &dimlevel, shutterattrval, 0);
        } else if (updownstopCmd == 0x00 || updownstopCmd == 0x01 || updownstopCmd == 0x02) {   // Move upwards/downwards or Stop
        	snprintf(command, MAX_STRLENGTH, "zclcmd 0x%04X %d 0x0102 %02X", shortAddr, endpoint, updownstopCmd);
        	zigbeeSendCommand(command);
        	//createStateMessage(message, shortAddr, endpoint, &state, &dimlevel, shutterattrval, 0);
        } else if (updownstopCmd == 0x04 || updownstopCmd == 0x07) {   // Moves to the specified lift/tilt value. Unsigned 16-bit integer.
        	char moveValStr[5], moveValRev[5];
        	memset(moveValStr, '\0', sizeof(moveValStr));
        	memset(moveValRev, '\0', sizeof(moveValRev));
        	snprintf(moveValStr, sizeof(moveValStr), "%04X", moveValue);
        	reverseStr(moveValStr, moveValRev, 4);
        	snprintf(command, MAX_STRLENGTH, "zclcmd 0x%04X %d 0x0102 %02X%s",
        			shortAddr, endpoint, updownstopCmd, moveValRev);
        	zigbeeSendCommand(command);
        	//createStateMessage(message, shortAddr, endpoint, &state, &dimlevel, shutterattrval, 0);
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
	printf("\t -t --on\t\t\tsend an on (open) signal\n");
	printf("\t -f --off\t\t\tsend an off (close) signal\n");
	printf("\t -d --dimlevel=dimlevel\t\tsend command move to (e.g 25%% lift, 50%% tilt)\n");
	printf("\t -s --shortaddr=id\t\tcontrol a device with this short address\n");
	printf("\t -e --endpoint=id\t\tcontrol a device with this endpoint id\n");
	printf("\t -r --read\t\t\tread attribute values from device\n");
	printf("\t -i --identify\t\t\tsend identify request\n");
	printf("\t -a --autodiscover\t\tdiscover ZigBee Window Covering (shutter) devices\n");
	printf("\t -m --moveup\t\t\tmove up/open \n");
	printf("\t -n --movedown\t\t\tmove down/close \n");
	printf("\t -o --stop\t\t\tstop moving\n");
	printf("\t -p --liftval=value\t\tmove to lift value (height e.g 200 cm) \n");
	printf("\t -q --liftpct=pct\t\tmove to lift in percent (0%% = open, 100%% = closed) \n");
	printf("\t -x --tiltval=value\t\tmove to tilt value (tilt angle e.g. 45.0 degrees)\n");
	printf("\t -y --tiltpct=pct\t\tmove to tilt in percent (0%% = open, 100%% = closed) \n");
	printf("\t -c --calibrate\t\t\tcalibrate device\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void zigbeeShutterInit(void) {
    protocol_register(&zigbee_shutter);
    protocol_set_id(zigbee_shutter, "zigbee_shutter");
    protocol_device_add(zigbee_shutter, "zigbee_shutter", "ZigBee Window Covering Cluster");
    zigbee_shutter->devtype = DIMMER;
    zigbee_shutter->hwtype = ZIGBEE;

	options_add(&zigbee_shutter->options, 's', "shortaddr", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, NULL);
	options_add(&zigbee_shutter->options, 'e', "endpoint", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_shutter->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_shutter->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_shutter->options, 'd', "dimlevel", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_shutter->options, 'r', "read", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_shutter->options, 'i', "identify", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_shutter->options, 'a', "autodiscover", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);

	options_add(&zigbee_shutter->options, 'm', "moveup", OPTION_NO_VALUE, 0, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_shutter->options, 'n', "movedown", OPTION_NO_VALUE, 0, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_shutter->options, 'o', "stop", OPTION_NO_VALUE, 0, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_shutter->options, 'p', "liftval", OPTION_HAS_VALUE, 0, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_shutter->options, 'q', "liftpct", OPTION_HAS_VALUE, 0, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_shutter->options, 'x', "tiltval", OPTION_HAS_VALUE, 0, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_shutter->options, 'y', "tiltpct", OPTION_HAS_VALUE, 0, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_shutter->options, 'c', "calibrate", OPTION_NO_VALUE, 0, JSON_NUMBER, NULL, NULL);

	options_add(&zigbee_shutter->options, 0, "WindowCoveringType", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&zigbee_shutter->options, 0, "CurrentPositionLift", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_shutter->options, 0, "CurrentPositionTilt", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_shutter->options, 0, "CurrentPositionLiftPercentage", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_shutter->options, 0, "CurrentPositionTiltPercentage", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_shutter->options, 0, "OperationalStatus", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_shutter->options, 0, "InstalledClosedLimitLift", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_shutter->options, 0, "InstalledClosedLimitTilt", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_shutter->options, 0, "Mode", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);

	options_add(&zigbee_shutter->options, 0, "dimlevel-minimum", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, NULL);
	options_add(&zigbee_shutter->options, 0, "dimlevel-maximum", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)100, NULL);

	options_add(&zigbee_shutter->options, 0, "extaddr", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, "[0][xX][0-9a-fA-F]{16}");
	options_add(&zigbee_shutter->options, 0, "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]{1,5}"); // allow 0 - 99999 seconds
	options_add(&zigbee_shutter->options, 0, "ProfileId", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_shutter->options, 0, "ManufacturerName", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_shutter->options, 0, "ModelIdentifier", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_shutter->options, 0, "DateCode", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);

	zigbee_shutter->parseCommand=&parseCommand;
	zigbee_shutter->createCode=&createCode;
	zigbee_shutter->printHelp=&printHelp;
	zigbee_shutter->gc=&gc;
	zigbee_shutter->checkValues=&checkValues;

	char command[] = "zclattr 0x%04X %d 0x0102 00000003000400080009000A00110013001700";
	// predefined command read attributes in cluster 0x0102
	zigbee_cmd_t *zcmd = NULL;
	if ((zcmd = MALLOC(sizeof(struct zigbee_cmd_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(zcmd, '\0', sizeof(struct zigbee_cmd_t));
	if ((zcmd->command = MALLOC(strlen(command) + 1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(zcmd->command, command);
	zcmd->next = zigbee_commands;
	zigbee_commands = zcmd;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice);
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
    module->name = "zigbee_shutter";
    module->version = "0.1";
    module->reqversion = "7.0";
    module->reqcommit = NULL;
}

void init(void) {
	zigbeeShutterInit();
}
#endif
