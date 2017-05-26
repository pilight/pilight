/*
 * zigbee_thermostat.c
 *
 *  Created on: Mar 26, 2017
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
#include "zigbee_thermostat.h"

static struct zigbee_data_t *zigbee_data = NULL;
static struct zigbee_cmd_t *zigbee_commands = NULL;

static void *addDevice(int reason, void *param) {
	zigbee_data = addZigbeeDevice(param, zigbee_data, zigbee_commands, zigbee_thermostat->id);

	return NULL;
}

/* Cluster 0x0201 Thermostat Cluster // ZigBee Cluster Library 6.3
 *
 * Attribute id: 0x0000, type id 0x29: 16-bit signed int Local Temperature
 * Attribute id: 0x0012, type id 0x29: 16-bit signed int Occupied Heating Setpoint
 * Attribute id: 0x0025, type id 0x18: 8-bit bitmap
 *   Thermostat Programming Operation Mode, bit 0
 *   - enabling 1 or disabling 0 of the Thermostat Scheduler
 *
 * Heating Off: Temperature >= (Heating SetPoint + High_Hysteresis)
 * Heating On:  Temperature <= (Heating SetPoint - Low_Hysteresis)
 *
 * Commands:
 * Command id: 0x00 Setpoint Raise/Lower
 * Command id: 0x01 Set Weekly Schedule
 * Command id: 0x02 Get Weekly Schedule
 * Command id: 0x03 Clear Weekly Schedule
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
	char buf[80];
	int a[2], b[2], c = 0, d = 0, shortaddr = -1, endpoint = -1, attrid = -1, i, j;
	int t[4], timeattr = 0, attrtype = 0;
	int foundthermo = 0, foundsetpoint = 0, foundstate = 0;
	int localtemperature = 0, heatingsetpoint = 0, state = -1;
	int numbertransitions = 0, dayofweek = 0, transitiontime = 0;
	time_t ltime;
	struct tm ts;

	for (i = 0; i < 2; i++) {
		a[i] = b[i] = 0;
	}
	memset(extaddr, '\0', sizeof(extaddr));
	memset(profileid, '\0', sizeof(profileid));

	if((jmsg = json_find_member(code, "message")) == NULL) {
		return;
	}
	if (json_find_string(jmsg, "zigbee_response", &stmp) != 0) {
		return;
	}

	if (sscanf(stmp, "<-APS attr %s %d 0x0201", extaddr, &endpoint) != 2
			&& sscanf(stmp, "<-APS attr %s %d 0x000A", extaddr, &endpoint) != 2
			&& sscanf(stmp, "<-ZCL serverToClient %s for cluster 0x0201", extaddr) != 1
			&& sscanf(stmp, "<-ZCL attribute report %s 0x0201",  extaddr) != 1
			&& sscanf(stmp, "<-ZDP match %s %x %d 0x0201 %s", extaddr, &shortaddr, &endpoint, profileid) != 4
			&& sscanf(stmp, "<-ZCL attr %x %d 0x0201 %x %[^\n]", &shortaddr, &endpoint, &attrid, attr) != 4) {
		return;  // only cluster 0x0201 thermostat
	}
	// <-APS attr 0x000D6F00xxxxxxxx 1 0x0201 0x0000 0x29 66 08 12 00 00 29 A4 06 25 00 00 18 01 29 00 00 19 00 00
	// <-APS attr 0x000D6F00xxxxxxxx 1 0x0201 0x0000 0x29 9E 07 12 00 00 29 D0 07 25 00 00 18 00
	if (sscanf(stmp, "<-APS attr %s %d 0x0201 0x0000 0x29 %x %x 12 00 00 29 %x %x 25 00 00 18 %x 29 00 00 19 %x 00",
			extaddr, &endpoint, &a[0], &a[1], &b[0], &b[1], &c, &d) == 8) {
		foundthermo = 1;
		foundsetpoint = 1;
		foundstate = 1;
	}
	// Time attributes 0,1,2,3,4,5,6,7
	// <-APS attr 0x000D6F00xxxxxxxx 1 0x000A 0x0000 0xE2 00 00 00 00
	// ->01 00 00 18 00<- ->02 00 00 2B 00 00 00 00<- ->03 00 00 23 00 00 00 00<-
	// ->04 00 00 23 00 00 00 00<- ->05 00 00 2B 00 00 00 00<- ->06 00 00 23 00 00 00 00<-
	// ->07 00 00 23 00 00 00 00<-
	if (sscanf(stmp, "<-APS attr %s %d 0x000A 0x0000 0xE2 %x %x %x %x ", extaddr, &endpoint, &t[0], &t[1], &t[2], &t[3]) == 6) {
		char *p = NULL;
		p = strstr(stmp, "0x0000 0xE2");
		if (p == NULL) {
			logprintf(LOG_ERR, "[ZigBee] invalid time: %s", stmp);
			return;
		}
		for (i = 0; i < 4; i++) {
			timeattr |= (t[i] << (i *8 ));
		}
		ltime = (time_t) (timeattr + 946684800);  // 946684800 = epoch on 1. Jan 2000
		ts = *gmtime(&ltime);
		strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y (%Z)", &ts);
		logprintf(LOG_INFO, "time = %d, %s, time difference = %d", timeattr, buf, (int) difftime(time(NULL), ltime));
		p += strlen("0x0000 0xE2 00 00 00 00 ");
		if (sscanf(p, "01 00 00 18 %x", &timeattr) != 1) {
			logprintf(LOG_ERR, "[ZigBee] invalid time: %s", p);
			return;
		}
		logprintf(LOG_INFO, "time status = 0x%02X", timeattr);
		p += strlen("01 00 00 18 00 ");
		for (j = 2; j < 8; j++) {
			if (sscanf(p, "0%*[2-7] 00 00 %x %x %x %x %x", &attrtype, &t[0], &t[1], &t[2], &t[3]) != 5) {
				logprintf(LOG_ERR, "[ZigBee] invalid time: %s", p);
				return;
			}
			timeattr = 0;
			for (i = 0; i < 4; i++) {
				timeattr |= (t[i] << (i *8 ));
			}
			if (attrtype == 0x23) {
				ltime = (time_t) (timeattr + 946684800);  // 946684800 = epoch on 1. Jan 2000
				ts = *gmtime(&ltime);
				strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y", &ts);
				if (j < 5) {
					snprintf(buf, sizeof(buf), "%s", ctime(&ltime));
				}
				logprintf(LOG_INFO, "%d. time = %u, %s", j, timeattr, buf);  // unsigned int
			} else {
				logprintf(LOG_INFO, "%d. time = %d", j, timeattr);  // signed int
			}
			if (j < 7) {
				p += strlen("02 00 00 2B 00 00 00 00 ");
			}
		}
	}
	// <-ZCL serverToClient 0x000D6F00xxxxxxxx 1 for cluster 0x0201 01 01 01 B4 02 DC 05
	// <-ZCL serverToClient 0x000D6F00xxxxxxxx 1 for cluster 0x0201 02 01 01 BC 02 02 08 BD 02 08 07
	if (sscanf(stmp, "<-ZCL serverToClient %s %d for cluster 0x0201 %x %x 01 ", extaddr, &endpoint, &numbertransitions, &c) == 4) {
		for (i = 0; i < 7; i++) {
			if (c & (1 << i)) {  // bit 0 = Sunday, 1 = Monday, ... 6 = Saturday
				dayofweek = '0' + i;
			}
		}
		logprintf(LOG_INFO, "[ZigBee] thermostat read scheduler: number transitions = %d, day of week = %c (%02x)",
				numbertransitions, dayofweek, c );

		char *p = NULL;
		if ((p = strstr(stmp,"cluster 0x0201")) == NULL) {
			return;
		}
		p += strlen("cluster 0x0201 00 00 00 ");

		for (i = 0; i < numbertransitions; i++) {
			if (sscanf(p, "%x %x %x %x", &a[0], &a[1], &b[0], &b[1]) != 4) {
				logprintf(LOG_ERR, "[ZigBee] invalid stmp = %s, p = %s", stmp, p);
				return;
			}
			transitiontime = heatingsetpoint = 0;
			for (j = 0; j < 2; j++) {
				transitiontime |= ((a[j]) << (j * 8));
				heatingsetpoint |= ((b[j]) << (j * 8));
			}
			int hours = transitiontime / 60;
			int minutes = transitiontime % 60;
			logprintf(LOG_INFO, "%d. transition time = %02d:%02d (%d, 0x%04X), heat setpoint = %.1f (%d, 0x%04X)",
					i, hours, minutes, transitiontime, transitiontime,
					heatingsetpoint / 100.0, heatingsetpoint, heatingsetpoint);
			p += strlen("00 00 00 00 ");
		}

	}
	// <-ZCL attribute report 0x00124B00xxxxxxxx 0x0201 1 00 00 29 10 00
	if (foundthermo == 0 && sscanf(stmp, "<-ZCL attribute report %s 0x0201 %d 00 00 29 %x %x", extaddr, &endpoint, &a[0], &a[1]) == 4) {
		foundthermo = 1;
	}
	// discovery response from Cluster 0x0201 - part 1
	if (foundthermo == 0) {
		if (sscanf(stmp, "<-ZDP match %s %x %d 0x0201 %s", extaddr, &shortaddr, &endpoint, profileid) == 4) {
			matchResponse(extaddr, shortaddr, endpoint, 0x0201 /* cluster */, profileid, zigbee_thermostat->id);
			return;
		}
	// discovery response from Cluster 0x0201 - part 2 --> read ZigBee Basic Cluster Attributes 4,5,6 and 0x4000
		if (sscanf(stmp, "<-ZCL attr %x %d 0x0201 %x %[^\n]", &shortaddr, &endpoint, &attrid, attr) == 4) {
			matchAttributeResponse(shortaddr, endpoint, 0x0201 /* cluster */, attrid, attr, zigbee_thermostat->id);
			return;
		}
	}

	if (foundthermo == 0) {
		return;
	}
	if (foundthermo == 1) {
		for (i = 0; i < 2; i++) {
			localtemperature |= ((a[i]) << (i * 8));
			heatingsetpoint |= ((b[i]) << (i * 8));
		}
		state = c & 0x01;
		logprintf(LOG_DEBUG, "[ZigBee] zigbee_thermostat parseCommand localtemperature = %d, heatingsetpoint = %d, state = %d, heat = %d",
				localtemperature, heatingsetpoint, state, d);
	}

	tmp = zigbee_data;
	while(tmp) {
		if (tmp->extaddr != NULL && strcmp(extaddr, tmp->extaddr) == 0 && tmp->endpoint == endpoint) {
			shortaddr = (int) tmp->shortaddr;
			logfile = tmp->logfile;
			break;
		}
		tmp = tmp->next;
	}
	if (shortaddr == -1 && strlen(extaddr) > 0) {
		sscanf(extaddr, "%x", &shortaddr);  // sometimes extaddr has the shortaddr
		tmp = zigbee_data;
		while(tmp) {
			if (tmp->shortaddr == shortaddr && tmp->endpoint == endpoint) {
				logfile = tmp->logfile;
				break;
			}
			tmp = tmp->next;
		}
	}

	if (shortaddr == -1) { // device not found
		return;
	}

	int x = snprintf(message, 255, "{\"shortaddr\":\"0x%04X\",", shortaddr);
	x += snprintf(&message[x], 255-x, "\"endpoint\":%d,", endpoint);
	if (foundthermo == 1) {
		x += snprintf(&message[x], 255-x, "\"localtemperature\": %.1f,", localtemperature / 100.0);
	}
	if (foundsetpoint == 1) {
		x += snprintf(&message[x], 255-x, "\"heatingsetpoint\": %.1f,", heatingsetpoint / 100.0);
		x += snprintf(&message[x], 255-x, "\"dimlevel\": %d,", heatingsetpoint / 100);
	}
	if (foundstate == 1) {
		if (state == 1) {
			x += snprintf(&message[x], 255-x, "\"state\":\"on\",");
		} else {
			x += snprintf(&message[x], 255-x, "\"state\":\"off\",");
		}
	}
	x += snprintf(&message[x-1], 255-x, "}");

	broadcast_message(message, zigbee_thermostat->id);

	FILE *fp = NULL;
	if (foundthermo == 1 && logfile != NULL && strlen(logfile) > 0) {
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
		x += snprintf(&content[x], 200-x, " %.1f", localtemperature / 100.0);
		if (foundsetpoint == 1) {
			x += snprintf(&content[x], 200-x, " %.1f", heatingsetpoint / 100.0);
			x += snprintf(&content[x], 200-x, " %d", d);
		} else {
			x += snprintf(&content[x], 200-x, " - -");
		}
		x += snprintf(&content[x], 200-x, " \n");
		fwrite(content, sizeof(char), strlen(content), fp);
		fclose(fp);
	}
}

// convert input to 32-bit little endian
static uint32_t tolittleendian32bit(uint32_t input) {
	int i;
	uint32_t toIntLE = input;
	char *p = (char *) &toIntLE;
	for (i = 0; i < sizeof(toIntLE)/2; i++) {
		char temp = p[i];
		p[i] = p[sizeof(toIntLE)-1-i];
		p[sizeof(toIntLE)-1-i] = temp;
	}
	return toIntLE;
}

// convert input to little endian
static int tolittleendian(int input) {
	int i;
	uint16_t toIntLE = (uint16_t) input;
	char *p = (char *) &toIntLE;
	for (i = 0; i < sizeof(toIntLE)/2; i++) {
		char temp = p[i];
		p[i] = p[sizeof(toIntLE)-1-i];
		p[sizeof(toIntLE)-1-i] = temp;
	}
	return (int) toIntLE;
}

// read schedule
//    in format "1,2,3,5 6:30 21.2; 23:30 18.1"
// or in format "1,2,3,5 6:30 21.2; 23:30 18.1; 0,6 8:00 23.2; 22:00 19.3"
static int readschedule(char *stmp, char *a, int *hour, int *minute, float *temperature) {
	while (isspace(*stmp)) stmp++; // strip leading white space
	if (sscanf(stmp, "%[0-7],%[0-7],%[0-7],%[0-7],%[0-7],%[0-7],%[0-7] %d:%d %f",
			&a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], hour, minute, temperature) == 10) {
		return 7;
	}
	if (sscanf(stmp, "%[0-7],%[0-7],%[0-7],%[0-7],%[0-7],%[0-7] %d:%d %f",
			&a[0], &a[1], &a[2], &a[3], &a[4], &a[5], hour, minute, temperature) == 9) {
		return 6;
	}
	if (sscanf(stmp, "%[0-7],%[0-7],%[0-7],%[0-7],%[0-7] %d:%d %f",
			&a[0], &a[1], &a[2], &a[3], &a[4], hour, minute, temperature) == 8) {
		return 5;
	}
	if (sscanf(stmp, "%[0-7],%[0-7],%[0-7],%[0-7] %d:%d %f",
			&a[0], &a[1], &a[2], &a[3], hour, minute, temperature) == 7) {
		return 4;
	}
	if (sscanf(stmp, "%[0-7],%[0-7],%[0-7] %d:%d %f",
			&a[0], &a[1], &a[2], hour, minute, temperature) == 6) {
		return 3;
	}
	if (sscanf(stmp, "%[0-7],%[0-7] %d:%d %f",
			&a[0], &a[1], hour, minute, temperature) == 5) {
		return 2;
	}
	if (sscanf(stmp, "%[0-7] %*[ ] %d:%d %f",
			&a[0], hour, minute, temperature) == 4) {
		return 1;
	}
	if (sscanf(stmp, "%d:%d %f",
			hour, minute, temperature) == 3) {
		return 0;
	}

	return -1;
}

static int createCode(struct JsonNode *code, char **pmessage) {
	char *message = *pmessage;
	char command[MAX_STRLENGTH], a[7];
	char payload[10][80];  // (3 + 2 + 2 + '\0')*10 = 80
	char *stmp = NULL, *p = NULL;
	char *reada = a;
	double itmp = 0.0;
	float temperature = 0.0;
	int shortAddr = -1, endpoint = -1;
	int count = 1, found = 0, hour = -1, minute = -1, days = 0;
	int transtime = 0, setpoint = 0, schedsetpoint, i, j, x, pindex = -1;
    snprintf(message, 255, "{}");

    memset(payload, '\0', sizeof(payload));
	for (i = 0; i < sizeof(a); i++) {
		a[i] = '\0';
	}

	if(json_find_number(code, "dimlevel", &itmp) == 0) {
		setpoint = itmp * 10 * 10;
	}
    if (json_find_number(code, "heatingsetpoint", &itmp) == 0) {
    	setpoint = itmp * 10 * 10;
    }
    if (json_find_string(code, "shortaddr", &stmp) == 0) {
    	sscanf(stmp, "%x", &shortAddr);
    }
    if (json_find_number(code, "shortaddr", &itmp) == 0) {
    	shortAddr = (int) itmp;
    }
    if (json_find_number(code, "endpoint", &itmp) == 0) {
    	endpoint = (int) itmp;
    }
    if (json_find_string(code, "scheduler", &stmp) == 0) {
    	p = stmp;
    	// payload header 3 bytes + transition time +  heat setpoint
    	// header = number of transitions (8-bit), day of week (8-bit), heat (8-bit)
    	// transition time (16-bit), heat set point (16-bit) (maximum 10 setpoints)
    	for (i = 0; i < 10; i++) {   // allow maximum 10 setpoints
    		found = readschedule(p, reada, &hour, &minute, &temperature);
    		if (found == -1) {
    			printf("invalid schedule format: %s\n", p);
    			return EXIT_FAILURE;
    		}
    		transtime = hour * 60 + minute;
    		schedsetpoint = temperature * 10 *  10;
    		if (found > 0) {
    			days = 0;
    			for (j = 0; j < found; j++) {
    				days |= 1 << (a[j] - '0'); // bit 0 = Sunday, 1 = Monday, ... 6 = Saturday
    			}
    			count = 1; // reset count
    			x = 0;     // reset x
    			x += snprintf(&payload[++pindex][x], 80 - x, "%02X%02X01%04X%04X", count, days, tolittleendian(transtime), tolittleendian(schedsetpoint));
    			printf("days of week = %s\n", a);
    		} else {
    			if (pindex < 0) {
    				printf("invalid schedule format, missing date: %s\n", p);
    				return EXIT_FAILURE;
    			}
    			x += snprintf(&payload[pindex][x], 80 - x, "%04X%04X", tolittleendian(transtime), tolittleendian(schedsetpoint));
    			count++;
    			payload[pindex][1] = count + '0';  // number of transitions
    		}
    		printf("%d. %02d:%02d transitiontime = 0x%04X, temperature = %.1f, heat setpoint = 0x%04X\n",
    				count, hour, minute, transtime, temperature, schedsetpoint);

    		p = strstr(p, ";");
    		if (p == NULL) {
    			break;
    		}
    		p++;
    	}
    	for (i = 0; i < 10; i++) {
    		if (strlen(payload[i]) < 8) {
    			break;
    		}
    		printf("%d. payload = %s\n", i, payload[i]);
    	}
    	int x = snprintf(message, 255, "{\"shortaddr\":\"0x%04X\",", shortAddr);
    	x += snprintf(&message[x], 255-x, "\"endpoint\":%d,", endpoint);
    	x += snprintf(&message[x], 255-x, "\"scheduler\": \"%s\"}", stmp);
    }
    if (shortAddr == -1 || endpoint == -1) {
    	logprintf(LOG_ERR, "zigbee_thermostat: insufficient number of arguments");
    	printf("zigbee_thermostat: insufficient number of arguments\n");
    	printf("code %s stmp = %s, shortAddr = %x, endpoint = %d\n", json_stringify(code, "\t"), stmp, shortAddr, endpoint);
    	return EXIT_FAILURE;
    }

    if (pilight.process == PROCESS_DAEMON) {

    	if (json_find_number(code, "read", &itmp) == 0) {
    		// read attributes 0x0000 0x0012 0x0025
    		snprintf(command, MAX_STRLENGTH, "zclattr 0x%04X %d 0x0201 00000012002500", shortAddr, endpoint);
    		addsinglethread(command, 0);
       		snprintf(command, MAX_STRLENGTH, "zclattr 0x%04X %d 0x000A 0000000100020003000400050006000700", shortAddr, endpoint);
       		addsinglethread(command, 5); // read time, time status/zone, dst start/end/shift, standard/local time on cluster 0x000A
       		for (i = 0; i < 7; i++) {
       			int dayofweek = 1 << i;
       			snprintf(command, MAX_STRLENGTH, "zclcmd 0x%04X %d 0x0201 02%02X01", shortAddr, endpoint, dayofweek);
       			addsinglethread(command, 8 + (i * 2)); // send command id 0x02 get weekly schedule
       		}
    	} else if (setpoint > 0) {
    		// set attribute id 0x0012, type id 0x29 (signed 16-bit)
    		// set attribute id 0x001C, type id 0x30 (enum-8) system mode 0x04 = heat
    		snprintf(command, MAX_STRLENGTH, "zclattr 0x%04X %d 0x0201 02120029%04X1C003004", shortAddr, endpoint, tolittleendian(setpoint));
    		zigbeeSendCommand(command);
    		logprintf(LOG_DEBUG, "[ZigBee] zigbee_thermostat command = %s", command);
    	} else if (json_find_number(code, "on", &itmp) == 0) {
    		// set attribute id 0x0025, type id 0x18 (bit-8), bit0 = 1 enable schedule
    		// set attribute id 0x0012, type id 0x29 (signed 16-bit) heat setpoint = 2000 (0x07D0) (20 C)
    		// set attribute id 0x001C, type id 0x30 (enum-8) system mode 0x04 = heat
    		snprintf(command, MAX_STRLENGTH, "zclattr 0x%04X %d 0x0201 0225001801120029D0071C003004", shortAddr, endpoint);
    		zigbeeSendCommand(command);
    		logprintf(LOG_DEBUG, "[ZigBee] zigbee_thermostat command = %s", command);
    	} else if (json_find_number(code, "off", &itmp) == 0) {
    		// set attribute id 0x0025, type id 0x18 (bit-8), bit0 = 0 disable schedule
    		// set attribute id 0x0012, type id 0x29 (signed 16-bit) heat setpoint = 0700 (0x02BC) (7 C)
    		// set attribute id 0x001C, type id 0x30 (enum-8) system mode 0x00 = off
    		snprintf(command, MAX_STRLENGTH, "zclattr 0x%04X %d 0x0201 0225001800120029BC021C003000", shortAddr, endpoint);
    		zigbeeSendCommand(command);
    		logprintf(LOG_DEBUG, "[ZigBee] zigbee_thermostat command = %s", command);
    	} else if (json_find_number(code, "clear", &itmp) == 0) {
    		// send command id 0x03 clear weekly schedule
    		snprintf(command, MAX_STRLENGTH, "zclcmd 0x%04X %d 0x0201 03", shortAddr, endpoint);
    		zigbeeSendCommand(command);
    		logprintf(LOG_DEBUG, "[ZigBee] zigbee_thermostat command = %s", command);
       	} else if (strlen(payload[0]) > 0) {
    		// send command id 0x01 set weekly schedule
       		for (i = 0; i < 10; i++) {
        		if (strlen(payload[i]) < 8) {
        			break;
        		}
        		snprintf(command, MAX_STRLENGTH, "zclcmd 0x%04X %d 0x0201 01%s", shortAddr, endpoint, payload[i]);
       			addsinglethread(command, i * 5);
        		logprintf(LOG_DEBUG, "[ZigBee] zigbee_thermostat command = %s", command);
       		}
    	} else if (json_find_number(code, "settime", &itmp) == 0) {
    		// write attribute Time, Time Status on Time_Cluster 0x000A
    		uint32_t timenow = (uint32_t) difftime(time(NULL), 946684800); // 946684800 = seconds on 1. Jan 2000 UTC since epoch
    		snprintf(command, MAX_STRLENGTH, "zclattr 0x%04X %d 0x000A 020000E2%04X01001802", shortAddr, endpoint, tolittleendian32bit(timenow));
    		zigbeeSendCommand(command);
    		logprintf(LOG_DEBUG, "[ZigBee] zigbee_thermostat command = %s", command);
    	} else if (json_find_number(code, "sync", &itmp) == 0) {
    		// write attributes Time, Time Status, Time Zone, Daylight Saving Start/End/Shift on Time_Cluster 0x000A
    		snprintf(command, MAX_STRLENGTH, "sendtime 0x%04X %d", shortAddr, endpoint);
    		zigbeeSendCommand(command);
    		logprintf(LOG_DEBUG, "[ZigBee] zigbee_thermostat command = %s", command);
    	} else {
    		logprintf(LOG_ERR, "[ZigBee] zigbee_thermostat insufficient number of arguments");
    		printf("zigbee_thermostat: insufficient number of arguments\n");
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
	printf("\t ZigBee thermostat\n");
	printf("\t -s --shortaddr\n");
	printf("\t -e --endpoint\n");
	printf("\t -r --read\t\t\tread thermostat settings\n");
	printf("\t -m --settime\t\tset time setting (without daylight saving time)\n");
	printf("\t -y --sync\t\t\tsync time setting on thermostat\n");
	printf("\t -l --logfile\t\t\tset logfile\n");
	printf("\t -h --heatingsetpoint\t\tset thermostat heating setpoint\n");
	printf("\t -n --on\t\t\tset thermostat scheduler on (enable) and turn on thermostat\n");
	printf("\t -f --off\t\t\tset thermostat scheduler off (disable) and turn off thermostat\n");
	printf("\t -c --clear\t\t\tclear thermostat wwekly scheduler \n");
	printf("\t -w --scheduler\t\t\tset thermostat scheduler (format: 0,1,..,6 HH:MM TT.T )\n");
	printf("\t\t     scheduler format: day_of_week[0-6](comma , separated)  \n");
	printf("\t\t     day of week: sunday = 0, monday = 1, ... saturday = 6\n");
	printf("\t\t     setpoint_time_temparature: HH:MM TT.T (semicolon ; separated) \n");
	printf("\t\t     example scheduler format: 0,1,2,3,4 15:00 21.5; 23:00 17.2 \n");
	printf("\t\t             set monday to friday on 15:00 21.5 C and on 23:00 17.2 C\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void zigbeeThermostatInit(void) {
    protocol_register(&zigbee_thermostat);
    protocol_set_id(zigbee_thermostat, "zigbee_thermostat");
    protocol_device_add(zigbee_thermostat, "zigbee_thermostat", "ZigBee thermostat");
    zigbee_thermostat->devtype = DIMMER;
    zigbee_thermostat->hwtype = ZIGBEE;

	options_add(&zigbee_thermostat->options, 's', "shortaddr", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, NULL);
	options_add(&zigbee_thermostat->options, 'e', "endpoint", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_thermostat->options, 'r', "read", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_thermostat->options, 'c', "clear", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_thermostat->options, 'm', "settime", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_thermostat->options, 'y', "sync", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_thermostat->options, 'n', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_thermostat->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_thermostat->options, 't', "localtemperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_thermostat->options, 'h', "heatingsetpoint", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_thermostat->options, 'd', "dimlevel", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_thermostat->options, 'w', "scheduler", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_thermostat->options, 'l', "logfile", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);

	options_add(&zigbee_thermostat->options, 0, "dimlevel-minimum", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)7, NULL);
	options_add(&zigbee_thermostat->options, 0, "dimlevel-maximum", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)25, NULL);

	options_add(&zigbee_thermostat->options, 0, "extaddr", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, "[0][xX][0-9a-fA-F]{16}");
	options_add(&zigbee_thermostat->options, 0, "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]{1,5}"); // allow 0 - 99999 seconds
	options_add(&zigbee_thermostat->options, 0, "ProfileId", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_thermostat->options, 0, "ManufacturerName", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_thermostat->options, 0, "ModelIdentifier", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_thermostat->options, 0, "DateCode", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&zigbee_thermostat->options, 0, "Version", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);

	zigbee_thermostat->parseCommand=&parseCommand;
	zigbee_thermostat->createCode=&createCode;
	zigbee_thermostat->printHelp=&printHelp;
	zigbee_thermostat->gc=&gc;
	zigbee_thermostat->checkValues=&checkValues;

	// predefined commands to get zone values from target ZigBee device
	zigbee_cmd_t *zcmd = NULL;
	if ((zcmd = MALLOC(sizeof(struct zigbee_cmd_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(zcmd, '\0', sizeof(struct zigbee_cmd_t));
	if ((zcmd->command = MALLOC(strlen("zclattr 0x%04X %d 0x0201 000000120025002900") + 1)) == NULL) {
		OUT_OF_MEMORY
	}
	// read Cluster 0x0201 attributes 0x0000 0x0012 0x0025 0x0029 // zclattr 0x3B58 1 0x0201 00000012002500
	// Local Temperature, Heating Setpoint, Operation Mode, Running State
	// always broadcast to 0xFFFF on endpoint 1 // 0x%04X %d
	strcpy(zcmd->command, "zclattr 0x%04X %d 0x0201 000000120025002900");
	zcmd->next = zigbee_commands;
	zigbee_commands = zcmd;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice);
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
    module->name = "zigbee_thermostat";
    module->version = "0.1";
    module->reqversion = "7.0";
    module->reqcommit = NULL;
}

void init(void) {
	zigbeeThermostatInit();
}
#endif
