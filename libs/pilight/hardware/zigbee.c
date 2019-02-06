/*
 * zigbee.c
 *
 *  Created on: Sep 21, 2016
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
#include <fcntl.h>
#include <termios.h>
#ifndef _WIN32
	#include <arpa/inet.h>
#endif

#include "../core/common.h"
#include "../core/dso.h"
#include "../core/log.h"
#include "../core/socket.h"
#include "../hardware/hardware.h"
#include "../protocols/protocol.h"
#include "zigbee.h"

#define MAX_STRBUF 1024
#define ZIGBEE_PORT 5008

static struct zigbee_devices_t *zdev = NULL;
static struct zigbee_data_t *zigbee_data = NULL;
uv_tcp_t *socket_zigbee = NULL;
static int zigbee_initialized = 0;
static unsigned int zigbee_port = ZIGBEE_PORT;
static char *location = NULL;
int matchtime = 0;

static void create_zigbee_json_config(int shortaddr, int endpoint, char *protocol);
static void reconnect(char *req);

static int zigbeeSend(struct JsonNode *code) {
	return EXIT_SUCCESS;
}

static void *reason_code_received_free(void *param) {
	struct reason_code_received_t *data = param;
	FREE(data);
	return NULL;
}

/*
 * reverse bytes from 0x001FEE00xxxxxxxx => xxxxxxxx00EE1F00
 * and append '\0'
 */
void reverseStr(char *inputstr, char *out, int bytecount) {
	int i = 0, j = 0;
	int inlen = strlen(inputstr);
	if (inlen < bytecount) {
		return;
	}
	bytecount = bytecount >> 1;  // divide by 2
	inlen--;

	for (i = 0; i < bytecount; i++) {
		out[j++] = inputstr[inlen - 1];
		out[j++] = inputstr[inlen];
		inlen -= 2;
	}
	out[j] = '\0';
}

/*
 * check power configuration on battery device
 */
void sendPowerCmd(int shortaddr) {
	char cmd[200];
	char shortaddrStr[7];
	char shortaddrRev[5];
	memset(cmd, '\0', sizeof(cmd));
	memset(shortaddrStr, '\0', sizeof(shortaddrStr));
	memset(shortaddrRev, '\0', sizeof(shortaddrRev));
	snprintf(shortaddrStr, sizeof(shortaddrStr), "0x%04X", shortaddr);
	reverseStr(shortaddrStr, shortaddrRev, 4);

	snprintf(cmd, sizeof(cmd), "zdpcmd %s 0x0003 %s",
			shortaddrStr, shortaddrRev); // read power configuration
	zigbeeSendCommand(cmd);
}

void sendBindCmd(int shortAddr, int srcEp, int dstEp, int dobind, char *clusterIn, char *srcExtAddrIn, char *dstExtAddrIn, int sec) {
	char cluster[5], srcExtAddr[17], dstExtAddr[17], sendCmd[200];
	reverseStr(clusterIn, cluster, 4);
	reverseStr(srcExtAddrIn, srcExtAddr, 16);
	reverseStr(dstExtAddrIn, dstExtAddr, 16);
	printf("[ZigBee] binding cluster = %s, shortAddr = 0x%04X, srcExtAddrIn = %s, srcEp = %d, dstExtAddrIn = %s, dstEp = %d, srcExtAddr = %s, dstExtAddr = %s\n",
			clusterIn, shortAddr, srcExtAddrIn, srcEp, dstExtAddrIn, dstEp, srcExtAddr, dstExtAddr);

	// zdpcmd <shortAddr> 0x0021 <srcExtAddr><srcEp><cluster><03><dstExtAddr><dstEp>
	// zdpcmd 0x4157 0x0021 <xxxxxxxx00EE1F00><05><0207><03><xxxxxxxxFF2E2100><01>
	if (pilight.process == PROCESS_DAEMON) {
		sprintf(sendCmd, "zdpcmd 0x%04X 0x%04X %s%02X%s03%s%02X", shortAddr, dobind, srcExtAddr, srcEp, cluster, dstExtAddr, dstEp);
		addsinglethread(sendCmd, sec); // zigbeeSendCommand(sendCmd);
		logprintf(LOG_DEBUG, "[ZigBee] zigbee_ctrl finished send bind command %s", sendCmd);
	}
}

void sendRepoCmd(int shortAddr, int dstEp, char *clusterIn, char *attributeid, char *minimum, char *maximum, int attrtype, int reportchange) {
	int datalength = -1;
	char attrid[5], miniRev[5], maxiRev[5], sendCmd[200];
	reverseStr(attributeid, attrid, 4);
	reverseStr(minimum, miniRev, 4);
	reverseStr(maximum, maxiRev, 4);
	// dir attrid attrtype min max reportablechange
	sprintf(sendCmd, "zclattr 0x%04X %d %s 0600%s%02X%s%s", shortAddr, dstEp, clusterIn, attrid, attrtype, miniRev, maxiRev);
	if (reportchange != -1) {
		// attrtype 0x*8 = 8Bit, 0x*9 = 16 Bit, 0x*A = 24 Bit, ..., 0x*F = 64 Bit
		datalength = ((attrtype & 0x07) + 1) * 2; // Bytes 2,4,..16
		char strtmp[datalength + 1];
		char repchangestr[datalength + 1];
		char repchangestrRev[datalength + 1];
		memset(strtmp, '\0', datalength + 1);
		memset(repchangestr, '\0', datalength + 1);
		memset(repchangestr, '0', datalength);  // fill with zero
		snprintf(strtmp, datalength + 1, "%X", reportchange);
		int nrstart = datalength - strlen(strtmp);
		sprintf(&repchangestr[nrstart], "%s", strtmp);
		reverseStr(repchangestr, repchangestrRev, datalength);

		sprintf(sendCmd, "zclattr 0x%04X %d %s 0600%s%02X%s%s%s",
				shortAddr, dstEp, clusterIn, attrid, attrtype, miniRev, maxiRev, repchangestrRev);
		printf("[ZigBee] configure reporting: 0x%04X %d %s %s %02X %s %s %s\n",
				shortAddr, dstEp, clusterIn, attrid, attrtype, minimum, maximum, repchangestrRev);
	} else {
		printf("[ZigBee] configure reporting: 0x%04X %d %s %s %02X %s %s\n",
				shortAddr, dstEp, clusterIn, attrid, attrtype, minimum, maximum);
	}
	if (pilight.process == PROCESS_DAEMON) {
		zigbeeSendCommand(sendCmd);
		logprintf(LOG_DEBUG, "[ZigBee] configure reporting sendCmd = %s", sendCmd);
	}
}

void sendIdentifyCmd(int shortAddr, int dstEp, int duration) {
	char sendCmd[200], sduration[5], sdurationRev[5];
	memset(sendCmd, '\0', sizeof(sendCmd));
	memset(sduration, '\0', sizeof(sduration));
	snprintf(sduration, 5, "%04X", duration);
	reverseStr(sduration, sdurationRev, 4);
	printf("[ZigBee] identify on 0x%04X %d, duration = %d\n", shortAddr, dstEp, duration);
	sprintf(sendCmd, "zclcmd 0x%04X %d 0x0003 00%s", shortAddr, dstEp, sdurationRev);  // identify duration seconds
	if (pilight.process == PROCESS_DAEMON) {
		zigbeeSendCommand(sendCmd);
		logprintf(LOG_DEBUG, "[ZigBee] configure reporting sendCmd = %s", sendCmd);
	}
}

/*
 * get extended address from short address
 */
char *getextaddr(int shortaddr) {
	struct zigbee_devices_t *tmp = getlqi(); //zigbee_data;
	while (tmp) {
		logprintf(LOG_DEBUG, "shortaddr = 0x%04X, extaddr = %s", tmp->shortaddr, tmp->extaddr);
		if (tmp->shortaddr == shortaddr && tmp->extaddr != NULL) {
			return tmp->extaddr;
		}
		tmp = tmp->next;
	}
	return NULL;
}

void broadcast_message(char *message, char *protocol) {
	struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
	if(data == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(data->message, message);
	strcpy(data->origin, "receiver");
	data->protocol = protocol;
	if(strlen(pilight_uuid) > 0) {
		data->uuid = pilight_uuid;
	} else {
		data->uuid = NULL;
	}
	data->repeat = 1;
	eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);
}

static void receive_parse_api(struct JsonNode *code, int hwtype) {
	struct protocol_t *protocol = NULL;
	struct protocols_t *pnode = protocols;
	char message[255]; char *pmessage = message;

	while(pnode != NULL) {
		protocol = pnode->listener;

		if(protocol->hwtype == hwtype && protocol->parseCommand != NULL) {
			memset(message, 0, 255);
			protocol->parseCommand(code, &pmessage /*message*/);
			/*
			 * FIXME
			 */
			// receiver_create_message(protocol, message);
		}
		pnode = pnode->next;
	}
}

static void *threadSingleDeviceDiscover(void *param) {
	uv_timer_t *timer_req = param;
	int *shortaddr = timer_req->data;
	logprintf(LOG_DEBUG, "[ZigBee] threadSingleDeviceDiscover shortaddr = 0x%04X", *shortaddr);
	discoverSingleDevice(*shortaddr, 0);
	return NULL;
}

static int findCluster(uint16_t *clusters, uint8_t clcount, int16_t cl) {
	int i;
	for (i = 0; i < clcount; i++) {
		if (clusters[i] == cl) {
			return 1;  // found true
		}
	}
	return 0; // found false
}

static void checkCluster(struct zigbee_devices_t *tmp, int i, int *sec) {
	uint16_t cluster = 0;
	char command[1024];
	if (findCluster(tmp->clusters[i], tmp->clcount[i], 0x0008)) {			// check for 0x0008 cluster id Level_Control
		cluster = 0x0008;
	} else if (findCluster(tmp->clusters[i], tmp->clcount[i], 0x0006)) {	// check for 0x0006 cluster id OnOff
		cluster = 0x0006;
	} else if (findCluster(tmp->clusters[i], tmp->clcount[i], 0x0500)) {	// check for 0x0500 cluster id IAS_Zone
		cluster = 0x0500;
	} else if (findCluster(tmp->clusters[i], tmp->clcount[i], 0x0502)) {	// check for 0x0502 cluster id IAS_WD
		cluster = 0x0502;
	} else if (findCluster(tmp->clusters[i], tmp->clcount[i], 0x0702)) {	// check for 0x0702 cluster id Simple_Metering
		cluster = 0x0702;
	} else if (findCluster(tmp->clusters[i], tmp->clcount[i], 0x0201)) {	// check for 0x0201 cluster id Thermostat
		cluster = 0x0201;
	} else if (findCluster(tmp->clusters[i], tmp->clcount[i], 0x0406)) {	// check for 0x0406 cluster id Occupancy_Sensing
		cluster = 0x0406;
	} else if (findCluster(tmp->clusters[i], tmp->clcount[i], 0x0400)) {	// check for 0x0400 cluster id Illuminance_Measurement
		cluster = 0x0400;
	} else if (findCluster(tmp->clusters[i], tmp->clcount[i], 0x0402)) {	// check for 0x0402 cluster id Temperature
		cluster = 0x0402;
	} else if (findCluster(tmp->clusters[i], tmp->clcount[i], 0x0102)) {	// check for 0x0102 cluster id Window Covering Cluster
		cluster = 0x0102;
	} else {
		logprintf(LOG_DEBUG, "[ZigBee] checkCluster supported cluster NOT found for shortaddr = 0x%04X, ep = %d",
				tmp->shortaddr, tmp->endpoints[i]);
		int j;
		for (j = 0; j < tmp->clcount[i]; j++) {
			logprintf(LOG_DEBUG, "cluster[%d] = 0x%04X", j, tmp->clusters[i][j]);
		}
		return;
	}
	logprintf(LOG_DEBUG, "[ZigBee] checkCluster supported cluster found for shortaddr = 0x%04X, ep = %d, cluster = 0x%04X",
			tmp->shortaddr, tmp->endpoints[i], cluster);

	*sec += 2;
	//send broadcast "<-ZDP match 0x84182600xxxxxxxx 0xD19C   3 0x0008 0xC05E"
	sprintf(command, "<-ZDP match %s 0x%04X   %d 0x%04X %s",
			tmp->extaddr, tmp->shortaddr, tmp->endpoints[i], cluster, tmp->profile[i]);
	struct JsonNode *json = json_mkobject();
	struct JsonNode *jcode = json_mkobject();
	json_append_member(json, "message", jcode);
	json_append_member(json, "origin", json_mkstring("receiver"));
	json_append_member(jcode, "zigbee_response", json_mkstring(command));

	receive_parse_api(json, ZIGBEE);
	json_delete(json);
}

/*
 * discover a single device with short address
 */
void discoverSingleDevice(int shortaddr, int dstEp) {
	logprintf(LOG_DEBUG, "[ZigBee] discoverSingleDevice shortaddr = %04X, dstEp = %02X", shortaddr, dstEp);
	uint16_t payload = (uint16_t) shortaddr;
	static int runstep = 0, runcount = 0, endpoint = 0;
	static int *saddr = NULL;
	int sec = 1, i = 0;
	char command[1024];

	char *p = (char *) &payload;  // convert shortaddr to little endian in payload
	for (i = 0; i < sizeof(payload)/2; i++) {
		char temp = p[i];
		p[i] = p[sizeof(payload)-1-i];
		p[sizeof(payload)-1-i] = temp;
	}

	if (runstep == 0) {
		if ((saddr = MALLOC(sizeof(int))) == NULL) {
			OUT_OF_MEMORY
		}
		*saddr = shortaddr;
		runstep = 1;
	}
	if (dstEp != 0) {
		endpoint = dstEp;
		runstep = 2;
	}

	struct zigbee_devices_t *tmp = zdev;
	while (tmp) {
		if (tmp->shortaddr == shortaddr) {
			if (tmp->epcount > 0) {  // found endpoint
				if (runstep != 2) {
					runstep = 3;
				}
				for (i = 0; i < tmp->epcount; i++) {
					if (tmp->clcount[i] > 0) { // found cluster
						runstep = 4;
						if (endpoint == 0) {
							endpoint = tmp->endpoints[i];
						}
					}
					if (runstep == 4 && tmp->attr[3] != NULL) {  // found attributes
						runstep = 5;  // stop
						runcount = 5;
					}

					checkCluster(tmp, i, &sec);

					if (runstep > 3) {
						break;
					}
				}
			}
			break;
		}
		tmp = tmp->next;
	}
	logprintf(LOG_DEBUG, "[ZigBee] discoverSingleDevice shortaddr = %04X, endpoint = %02X, runstep = %d", shortaddr, endpoint, runstep);

	switch (runstep) {
		case 1: {
			// get active endpoints request 0x0005 (ZDP Active_EP_req ZigBee specification)
			sprintf(command, "zdpcmd 0x%04X 0x0005 %04X", shortaddr, payload);
			addsinglethread(command, sec);
		}; break;
		case 2: {
			// get simple descriptor request 0x0004 (ZDP Simple_Desc_req ZigBee specification) read clusters
			sprintf(command, "zdpcmd 0x%04X 0x0004 %04X%02X", shortaddr, payload, endpoint);
			addsinglethread(command, sec++);
		}; break;
		case 3: {
			// get simple descriptor request 0x0004 (ZDP Simple_Desc_req ZigBee specification) on all endpoints
			for ( i = 0; i < tmp->epcount; i++) {
				sprintf(command, "zdpcmd 0x%04X 0x0004 %04X%02X", shortaddr, payload, tmp->endpoints[i]);
				addsinglethread(command, sec++);
			}
		}; break;
		default:
			break;
	}

	if (runcount < 2) {
		runcount++;
		sec += 2;
		uv_timer_t *timer_req;
		if ((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
			OUT_OF_MEMORY
		}
		timer_req->data = saddr;
		uv_timer_init(uv_default_loop(), timer_req);
		uv_timer_start(timer_req, (void (*)(uv_timer_t *))threadSingleDeviceDiscover, sec * 1000, -1);
	} else {
		runcount = 0;
		runstep = 0;
		endpoint = 0;
		FREE(saddr);
	}
}

void showDiscoveredDevices() {
	struct zigbee_devices_t *tmp = zdev;
	char strcl[MAX_STRLENGTH];
	int i = 0, j = 0, count = 1, x = 0;
	while (tmp) {
		if (tmp->epcount == 0 && tmp->shortaddr != 0) {  // create unknown json config
			create_zigbee_json_config(tmp->shortaddr, 0, "zigbee_unknown");
		}
		if (tmp->shortaddr == 0) {
			logprintf(LOG_INFO, "%d. shortaddr = 0x%04X, extaddr = %s\n", count++, tmp->shortaddr, tmp->extaddr);
		}
		for (i = 0; i < tmp->epcount; i++) {
			x = snprintf(strcl, MAX_STRLENGTH - x, "%d. shortaddr = 0x%04X, endpoint = %3d (%02X), extaddr = %s, profile = %s, deviceid = %s\n   Cluster:",
					count++, tmp->shortaddr, tmp->endpoints[i], tmp->endpoints[i], tmp->extaddr, tmp->profile[i], tmp->deviceid[i]);
			for (j = 0; j < tmp->clcount[i]; j++) {
				x += snprintf(&strcl[x], MAX_STRLENGTH - x, " 0x%04X", tmp->clusters[i][j]);
			}
			if (tmp->attr[0] != NULL) {
				x += snprintf(&strcl[x], MAX_STRLENGTH - x, "\n   %s, %s", tmp->attr[0], tmp->attr[1]);
			}
			logprintf(LOG_INFO, "%s", strcl);
		}
		tmp = tmp->next;
	}
}

void autoDiscoverDevices() {
	int sec = 5, i = 0;
	char command[MAX_STRLENGTH];
	struct zigbee_devices_t *tmp = zdev;

	// step 1. run active ep request
	while (tmp) {
		if (tmp->shortaddr == 0 /*skip coordinator*/ || tmp->epcount > 0 /*skip if ep already found*/) {
			tmp = tmp->next;
			continue;
		}
		uint16_t shortaddr = (uint16_t) tmp->shortaddr;
		char *p = (char *) &shortaddr;  // convert shortaddr to little endian
		for (i = 0; i < sizeof(shortaddr)/2; i++) {
			char temp = p[i];
			p[i] = p[sizeof(shortaddr)-1-i];
			p[sizeof(shortaddr)-1-i] = temp;
		}
		// get active endpoints request 0x0005 (ZDP Active_EP_req ZigBee specification)
		sprintf(command, "zdpcmd 0x%04X 0x0005 %04X", tmp->shortaddr, shortaddr);
		addsinglethread(command, sec++);
		logprintf(LOG_DEBUG, "[ZigBee] command = %s", command);

		tmp = tmp->next;
	}

	// step 2. run simple descriptor request to get clusters
	tmp = zdev;
	while (tmp) {
		if (tmp->shortaddr == 0 || tmp->epcount == 0 /* need endpoint */) {
			tmp = tmp->next;
			continue;
		}
		uint16_t shortaddr = (uint16_t) tmp->shortaddr;
		char *p = (char *) &shortaddr;  // convert shortaddr to little endian
		for (i = 0; i < sizeof(shortaddr)/2; i++) {  // swap bytes order
			char temp = p[i];
			p[i] = p[sizeof(shortaddr)-1-i];
			p[sizeof(shortaddr)-1-i] = temp;
		}
		for (i = 0; i < tmp->epcount; i++) {
			if (tmp->clcount[i] > 0) {
				continue; // cluster already in list
			}
			// simple descriptor request 0x0004 on every endpoint
			sprintf(command, "zdpcmd 0x%04X 0x0004 %04X%02X", tmp->shortaddr, shortaddr, tmp->endpoints[i]);
			addsinglethread(command, sec++);
			logprintf(LOG_DEBUG, "[ZigBee] command = %s", command);
		}
		tmp = tmp->next;
	}

	// step 3. broadcast result and retrieve attributes from discovered devices
	matchtime = sec;
	tmp = zdev;
	while (tmp) {
		if (tmp->shortaddr == 0 || tmp->epcount == 0 /* need endpoint */) {
			tmp = tmp->next;
			continue;
		}
		for (i = 0; i < tmp->epcount; i++) {
			if (tmp->clcount[i] == 0) {
				continue; // no cluster
			}
			checkCluster(tmp, i, &sec);
		}
		tmp = tmp->next;
	}

	// step 4. repeat discover 3 times
    sec += 2;
	uv_timer_t *timer_req;
	if ((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY
	}
	timer_req->data = NULL;
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))threadDiscover, sec * 1000, -1);

	logprintf(LOG_DEBUG, "[ZigBee] autoDiscoverDevices() - sec = %d - done", sec);
}

void *threadDiscover(void *param) {
    static int runcount = 0;

    logprintf(LOG_DEBUG, "[ZigBee] threadDiscover runcount = %d", runcount);

    if (runcount++ < 2) {
    	autoDiscoverDevices();
    } else {
    	runcount = 0;
    	showDiscoveredDevices();
    }

	return NULL;
}

zigbee_devices_t *getlqi() {
	return zdev;
}

/*
 * Add entry to device list
 */
static void addDevEntry(char *extaddr, int shortaddr, int devicetype) {
	int i = 0;
	struct zigbee_devices_t *tmp = zdev;
	while (tmp) {
		if (tmp->shortaddr == shortaddr && strcmp(tmp->extaddr, extaddr) == 0) {
			return;
		}
		tmp = tmp->next;
	}
	struct zigbee_devices_t *addlqi = NULL;
	if ((addlqi = MALLOC(sizeof(struct zigbee_devices_t))) == NULL) {
		OUT_OF_MEMORY
	}
	if ((addlqi->profile = MALLOC(sizeof(char *))) == NULL) {
		OUT_OF_MEMORY
	}
	if ((addlqi->extaddr = MALLOC(strlen(extaddr) + 1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(addlqi->extaddr, extaddr);
	addlqi->shortaddr = shortaddr;
	addlqi->devicetype = devicetype;
	addlqi->endpoints = NULL;
	addlqi->epcount = 0;
	addlqi->clusters = NULL;
	addlqi->clcount = NULL;
	addlqi->matchcluster = 0;
	addlqi->profile[0] = NULL;
	addlqi->deviceid = NULL;
	for (i = 0; i < BASIC_ATTR; i++) {
		addlqi->attr[i] = NULL;
	}
	addlqi->next = zdev;
	zdev = addlqi;
	logprintf(LOG_DEBUG, "[ZigBee] add device extaddr %s, shortaddr 0x%04X, devicetype = %d",
			extaddr, shortaddr, devicetype);
}

/*
 * Add endpoint entry to device
 */
static void addDevEpEntry(int shortaddr, int endpoint) {
	int i = 0;
	zigbee_devices_t *tmp = zdev;
	while (tmp) {
		if (tmp->shortaddr == shortaddr) {
			for (i = 0; i < tmp->epcount; i++) {
				if (tmp->endpoints[i] == endpoint) {
					return; // endpoint already in list
				}
			}
			uint8_t *newep = REALLOC(tmp->endpoints, (tmp->epcount + 1) * sizeof(uint8_t));
			if (newep == NULL) {
				OUT_OF_MEMORY
			}
			uint16_t **newcl = REALLOC(tmp->clusters, (tmp->epcount + 1) * sizeof(uint16_t *));
			if (newcl == NULL) {
				OUT_OF_MEMORY
			}
			uint8_t *newclcount = REALLOC(tmp->clcount, (tmp->epcount + 1) * sizeof(uint8_t));
			if (newclcount == NULL) {
				OUT_OF_MEMORY
			}
			char **newprofile = REALLOC(tmp->profile, (tmp->epcount + 1) * sizeof(char *));
			if (newprofile == NULL) {
				OUT_OF_MEMORY
			}
			char **newdeviceid = REALLOC(tmp->deviceid, (tmp->epcount + 1) * sizeof(char *));
			if (newdeviceid == NULL) {
				OUT_OF_MEMORY
			}
			tmp->clusters = newcl;
			tmp->clusters[tmp->epcount] = NULL;
			tmp->clcount = newclcount;
			tmp->clcount[tmp->epcount] = 0;
			tmp->endpoints = newep;
			tmp->endpoints[tmp->epcount] = endpoint;
			tmp->profile = newprofile;
			tmp->profile[tmp->epcount] = NULL;
			tmp->deviceid = newdeviceid;
			tmp->deviceid[tmp->epcount] = NULL;
			tmp->epcount++;
			logprintf(LOG_DEBUG, "[ZigBee] add EP shortaddr 0x%04X, endpoint = %d", shortaddr, endpoint);
			break;
		}
		tmp = tmp->next;
	}
}

/*
 * Add profile to device endpoint
 */
static void addDevProfileEntry(int shortaddr, int endpoint, char *profile, char *deviceid) {
	int epcount = -1, i = 0;
	zigbee_devices_t *tmp = zdev;
	while (tmp) {
		if (tmp->shortaddr == shortaddr) {
			for (i = 0; i < tmp->epcount; i++) {
				if (tmp->endpoints[i] == endpoint) {
					epcount = i;
					break;
				}
			}
			if (epcount == -1) {
				return; // endpoint not found
			}
			if (tmp->profile[epcount] != NULL) {
				FREE(tmp->profile[epcount]);
			}
			if ((tmp->profile[epcount]= MALLOC(strlen(profile) + 1)) == NULL) {
				OUT_OF_MEMORY
			}
			strcpy(tmp->profile[epcount], profile);

			if (strlen(deviceid) > 0) {
				if (tmp->deviceid[epcount] != NULL) {
					FREE(tmp->deviceid[epcount]);
				}
				if ((tmp->deviceid[epcount]= MALLOC(strlen(deviceid) + 1)) == NULL) {
					OUT_OF_MEMORY
				}
				strcpy(tmp->deviceid[epcount], deviceid);
			}
		}
		tmp = tmp->next;
	}
}

/*
 * Add cluster to device endpoint
 */
static void addDevClusterEntry(int shortaddr, int endpoint, int cluster) {
	int epcount = -1, i = 0;
	uint8_t clcount = 0;
	zigbee_devices_t *tmp = zdev;
	while (tmp) {
		if (tmp->shortaddr == shortaddr) {
			for (i = 0; i < tmp->epcount; i++) {
				if (tmp->endpoints[i] == endpoint) {
					epcount = i;
					clcount = tmp->clcount[i];
					break;
				}
			}
			if (epcount == -1) {
				return; // endpoint not found
			}
			for (i = 0; i < clcount; i++) {
				if (tmp->clusters[epcount][i] == cluster) {
					return; // cluster already in list
				}
			}
			uint16_t *newcl = REALLOC(tmp->clusters[epcount], (clcount + 1) * sizeof(uint16_t));
			if (newcl == NULL) {
				OUT_OF_MEMORY
			}
			uint8_t *newclcount  = REALLOC(tmp->clcount, (clcount + 1) * sizeof(uint8_t));
			if (newclcount == NULL) {
				OUT_OF_MEMORY
			}
			tmp->clusters[epcount] = newcl;
			tmp->clusters[epcount][clcount++] = cluster;
			tmp->clcount = newclcount;
			tmp->clcount[epcount] = clcount;

			break;
		}
		tmp = tmp->next;
	}
}

/*
 * Add cluster from match request
 * return 1 if cluster successfully added or does match
 * return 0 if cluster already exists and does not match
 */
static int addDevMatchClusterEntry(int shortaddr, int endpoint, int cluster) {
	int epcount = -1, i = 0;
	zigbee_devices_t *tmp = zdev;
	while (tmp) {
		if (tmp->shortaddr == shortaddr) {
			for (i = 0; i < tmp->epcount; i++) {
				if (tmp->endpoints[i] == endpoint) {
					epcount = i;
					break;
				}
			}
			if (epcount == -1) {
				return 0; // endpoint not found
			}
			if (tmp->matchcluster != cluster && tmp->matchcluster != 0) {
				return 0;  // cluster is not match cluster
			}
			tmp->matchcluster = cluster;
			return 1;
		}
		tmp = tmp->next;
	}
	return 0; // not found and cluster not added
}

/*
 * Check if cluster is the cluster from match request
 * return 1 if cluster does match
 * return 0 if cluster does not match
 */
static int checkDevMatchClusterEntry(int shortaddr, int endpoint, int cluster) {
	int epcount = -1, i = 0;
	zigbee_devices_t *tmp = zdev;
	while (tmp) {
		if (tmp->shortaddr == shortaddr) {
			for (i = 0; i < tmp->epcount; i++) {
				if (tmp->endpoints[i] == endpoint) {
					epcount = i;
					break;
				}
			}
			if (epcount == -1) {
				return 0; // endpoint not found
			}
			if (tmp->matchcluster == cluster) {
				return 1;  // cluster does match
			}
			return 0; // cluster does not match
		}
		tmp = tmp->next;
	}
	return 0; // not found
}

/*
 * Add basic cluster attribute entry (attr id 4,5,6 or 0x0400) to device endpoint
 */
static void addDevAttributeEntry(int shortaddr, int endpoint, int attrid, char *attrval) {
	int epcount = -1, i = 0, attrindex = -1;
	char *str = NULL;
	zigbee_devices_t *tmp = zdev;
	while (tmp) {
		if (tmp->shortaddr == shortaddr) {
			for (i = 0; i < tmp->epcount; i++) {
				if (tmp->endpoints[i] == endpoint) {
					epcount = i;
					break;
				}
			}
			if (epcount == -1) {
				return; // endpoint not found
			}
			if (attrid >= BASIC_MIN && attrid <= BASIC_MAX) {
				attrindex = attrid - BASIC_MIN;
			}
			else if (attrid == BASIC_VER) {
				attrindex = BASIC_ATTR - 1;
			} else {
				return; // invalid attribute id, only 4,5,6,0x0400 allowed
			}
			if ((str = MALLOC(strlen(attrval) + 1)) == NULL) {
				OUT_OF_MEMORY
			}
			memset(str, '\0', strlen(attrval) + 1);
			strcpy(str, attrval);
			tmp->attr[attrindex] = str;
		}
		tmp = tmp->next;
	}
}

/*
 * Check if basic attributes have been added to device endpoint
 */
static int checkDevAttributes(int shortaddr, int endpoint) {
	int i = 0, j = 0;
	zigbee_devices_t *tmp = zdev;
	while (tmp) {
		if (tmp->shortaddr == shortaddr) {
			for (i = 0; i < tmp->epcount; i++) {
				if (tmp->endpoints[i] == endpoint) {
					for (j = 0; j < BASIC_ATTR; j++) {
						if (tmp->attr[j] == NULL) {
							return 0; // attribute not found
						}
					}
					return 1; // all attributes found
				}
			}
		}
		tmp = tmp->next;
	}
	return 0; // not found
}

/*
 * Add LQI response (neighbor table entry) to device list
 */
static void addlqi(char *data) {
	char extaddr[19];
	int shortaddr = 0, devicetype = 0;
	memset(extaddr, '\0', sizeof(extaddr));
	//          <-LQI <source addr> <neighborTableEntries> <start> <count> <extaddr> <short>
	//                <device type> <rxOnWhenIdle> <relationship> <permitJoin> <depth> <lqi>
	// Example "<-LQI 0x84182600xxxxxxxx   06 2 1 0x00124B00xxxxxxxx 0x3B58 2 0 4 01 02 64"

	if (sscanf(data, "<-LQI 0x%*s %*x %*d %*d %18s %x %d", extaddr, &shortaddr, &devicetype) == 3) {
		addDevEntry(extaddr, shortaddr, devicetype);
	}
}

/*
 * Add Endpoint response to device entry
 */
static void addep(char *data) {
	char extaddr[19];
	int shortaddr = 0, endpoint = 0;
	// Exapmle "<-EP 0x001FEE00xxxxxxxx 0x4157   1 (01)"
	if (sscanf(data, "<-EP %18s %x %d", extaddr, &shortaddr, &endpoint) == 3) {
		addDevEntry(extaddr, shortaddr, 2 /*devicetype*/);
		addDevEpEntry(shortaddr, endpoint);
	}
}

/*
 * Add Cluster response to device entry
 */
static void addcluster(char *data) {
	char profile[7];  //0x1234 = 6 Bytes + '\0'
	char deviceid[7];
	char extaddr[19];
	int shortaddr = 0, endpoint = 0, cluster = 0;

	if (sscanf(data, "<-CLUSTER %18s %x ep %x profile %s deviceid %s", extaddr, &shortaddr, &endpoint, profile, deviceid) == 5) {
		// Example   "<-CLUSTER 0x001FEE00xxxxxxxx 0x4157 ep 0x01 profile 0x0104 deviceid 0x0002 deviceversion 0x00"
		addDevEntry(extaddr, shortaddr, 2 /*devicetype*/);
		addDevEpEntry(shortaddr, endpoint);
		addDevProfileEntry(shortaddr, endpoint, profile, deviceid);
	}
	if (sscanf(data, "<-CLUSTER 0x%*s %x %x In %x", &shortaddr, &endpoint, &cluster) == 3) {
		// Example   "<-CLUSTER 0x00124B00xxxxxxxx 0xDC6E 0x01 In  0x0000 BASIC_CLUSTER_ID"
		addDevClusterEntry(shortaddr, endpoint, cluster);
	}
}

/*
 * Add basic cluster 0x0000 attribute from ZCL attribute report
 */
static void addattr(char *data) {
	char *p = NULL, *dend = NULL, *pstr = NULL, *ptmp = NULL;
	char stmp[19], slen[100], attrval[100];
	int endpoint = 0, a[2], itmp = 0, attrid = 0, attrtype = 0, attrcount = 0;
	int attrstrlen = 0;
	int len = strlen(data);
	dend = data + len;
	memset(stmp, '\0', sizeof(stmp));
	memset(attrval, '\0', sizeof(attrval));

	// <-ZCL attribute report 0x00158D00xxxxxxxx 0x0000 1 05 00 42 0E 6C 75 6D 69 2E 73 65 6E 73 6F 72 5F 68 74
	if (sscanf(data,"<-ZCL attribute report %18s 0x0000 %d ", stmp, &endpoint) == 2) {
		len = snprintf(slen, sizeof(slen), "<-ZCL attribute report %s 0x0000 %d ", stmp, endpoint);
		p = data + len;
		// p = 05 00 42 0E 6C 75 6D 69 2E 73 65 6E 73 6F 72 5F 68 74
		if (sscanf(p, "%x %x %x ", &a[0], &a[1], &attrtype) == 3) {
			attrid = (a[1] << 8 | a[0]);
			len = strlen("00 00 00 ");
			p += len;
			while (p < dend) {
				if (sscanf(p, "%x", &itmp) == 1) {
					attrval[attrcount++] = (char) itmp;
				}
				p += 3;
			}
			if (attrtype == 0x42 && attrcount > 0) { // string type
				attrstrlen = attrval[0];
				if (attrstrlen != attrcount - 1) {
					logprintf(LOG_DEBUG, "[ZigBee] addattr error mismatch attrcount = %d, string length = %d",
							attrcount, attrstrlen);
				}
				pstr = attrval + 1;  // first byte is string length
				ptmp = pstr;
				while (ptmp < pstr + attrcount - 1) {
					if (*ptmp < 0x20 || *ptmp > 0x7E) { // non printable char
						*ptmp = '.';
					}
					ptmp++;
				}
				logprintf(LOG_DEBUG, "[ZigBee] addattr addr = %s, attrid = 0x%04X, %s", stmp, attrid, pstr);
			}
		}
	}
}


void *zigbeethread(void *param) {
	uv_timer_t *timer_req = param;
	struct zigbee_data_t *settings = timer_req->data;

	int nextthread = 0;
	char command[MAX_STRLENGTH];

	if (settings->shortaddr == 0 || settings->endpoint == 0) {
		logprintf(LOG_ERR, "[ZigBee] thread missing shortaddr or endpoint on device = %s", settings->dev);
		return NULL;
	}
	if (settings->nextcmd == NULL) {
		logprintf(LOG_DEBUG, "[ZigBee] thread missing command on device = %s, shortaddr = 0x%04X, endpoint = %d",
				settings->dev, settings->shortaddr, settings->endpoint);
		return NULL;
	}
	// Read attribute 0x0000 from device shortaddr on endpoint
	// Example: zclattr 0x4157 1 0x0006 000000 // zclattr 0xBACE 11 0x0008 000000
	zigbee_cmd_t *zcmd = settings->nextcmd;
	snprintf(command, MAX_STRLENGTH, zcmd->command, settings->shortaddr, settings->endpoint);
	logprintf(LOG_DEBUG, "[ZigBee] thread interval = %d, command = %s", settings->interval, command);
    zigbeeSendCommand(command);

    if (zcmd->next != NULL) {
    	settings->nextcmd = zcmd->next;
    	nextthread = 2;
    } else {
    	settings->nextcmd = settings->commands; // start commands again on next interval
    	if (settings->interval > 0) {
    		nextthread = settings->interval;
    	}
    }
	if (nextthread > 0) {
		if ((settings->timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
			OUT_OF_MEMORY
		}
		settings->timer_req->data = settings;
		uv_timer_init(uv_default_loop(), settings->timer_req);
		uv_timer_start(settings->timer_req, (void (*)(uv_timer_t *))zigbeethread, nextthread * 1000, -1);
	}
	return NULL;
}

void *singlethread(void *param) {
	uv_timer_t *timer_req = param;
	struct zigbee_cmd_t *zcmd = timer_req->data;

	if (zcmd == NULL || zcmd->command == NULL) {
		logprintf(LOG_DEBUG, "[ZigBee] singlethread command is empty command");
		return NULL;
	}
	logprintf(LOG_DEBUG, "[ZigBee] singlethread command = %s", zcmd->command);
    zigbeeSendCommand(zcmd->command);

    FREE(zcmd->command);
    FREE(zcmd);

	return NULL;
}

void addsinglethread(char *command, int sec) {
	if (command == NULL) {
		logprintf(LOG_DEBUG, "[ZigBee] singlethread command is empty command");
		return;
	}

	struct zigbee_cmd_t *zcmd = NULL;
	if ((zcmd = MALLOC(sizeof(struct zigbee_cmd_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(zcmd, '\0', sizeof(struct zigbee_cmd_t));
	if ((zcmd->command = MALLOC(strlen(command) + 1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(zcmd->command, command);
	zcmd->next = NULL;

	logprintf(LOG_DEBUG, "[ZigBee] addsinglethread command = %s, seconds = %d", command, sec);

	uv_timer_t *timer_req;
	if ((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY
	}
	timer_req->data = zcmd;
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))singlethread /*callback*/, sec * 1000, -1);
}

/*
 *   Create a new json config entry for the new device
 *   must copy and paste the output into json.config file devices
 *   result is created in /tmp directory
 *
 */
static void create_zigbee_json_config(int shortaddr, int endpoint, char *protocol) {
	int i = 0, isfound = 0, epcount = 0;
	struct zigbee_devices_t *tmp = zdev;
	while (tmp) {
		if (shortaddr == tmp->shortaddr) {
			isfound = 1;
			for (i = 0; i < tmp->epcount; i++) {
				if (tmp->endpoints[i] == endpoint) {
					epcount = i;
					break;
				}
			}
		}
		if (isfound == 1) {
			break;
		}
		tmp = tmp->next;
	}
	if (isfound == 0 && tmp->epcount == 0) {
		logprintf(LOG_ERR, "[ZigBee] matchAttributeResponse not found addr = 0x%04X, ep = %d", shortaddr, endpoint);
		return;
	}
	if (isfound == 0) {
		logprintf(LOG_ERR, "[ZigBee] matchAttributeResponse not found addr = 0x%04X", shortaddr);
		return;
	}

	int devnamelen = strlen("zigbee-0x0000-0x00") + strlen(protocol) + 1;
	int filenamelen = strlen("/tmp/pilight_-0x0000-0x00_json.config") + strlen(protocol) + 1;
	char discoverdev[1024], *devname, *configfile;
	memset(discoverdev, '\0', sizeof(discoverdev));

	int x = snprintf(discoverdev,  1023, "{\"protocol\":[\"%s\"],", protocol);
	x += snprintf(&discoverdev[x], 1023-x, "\"id\":[{\"shortaddr\":\"0x%04X\",\"endpoint\":%d}],", shortaddr, endpoint);
	x += snprintf(&discoverdev[x], 1023-x, "\"extaddr\":\"%s\",", tmp->extaddr);
	x += snprintf(&discoverdev[x], 1023-x, "\"ProfileId\":\"%s\",", tmp->profile[epcount]);
	x += snprintf(&discoverdev[x], 1023-x, "\"ManufacturerName\":\"%s\",", tmp->attr[0]);
	x += snprintf(&discoverdev[x], 1023-x, "\"ModelIdentifier\":\"%s\",", tmp->attr[1]);
	x += snprintf(&discoverdev[x], 1023-x, "\"DateCode\":\"%s\",", tmp->attr[2]);
	if ((strcmp(protocol, "zigbee_dimmer") == 0
			|| strcmp(protocol, "zigbee_thermostat") == 0
			|| strcmp(protocol, "zigbee_huemotion") == 0)
			&& (tmp->attr[3] == NULL || strcmp(tmp->attr[3], "unknown") != 0)) {
		x += snprintf(&discoverdev[x], 1023-x, "\"Version\":\"%s\",", tmp->attr[3]);
	}
	if (strcmp(protocol, "zigbee_dimmer") == 0
			|| strcmp(protocol, "zigbee_shutter") == 0) {
		x += snprintf(&discoverdev[x], 1023-x, "\"dimlevel\": 0,");
	}
	if (strcmp(protocol, "zigbee_metering") == 0
			|| strcmp(protocol, "zigbee_thermostat") == 0
			|| strcmp(protocol, "zigbee_temperature") == 0
			|| strcmp(protocol, "zigbee_huemotion") == 0) {
		x += snprintf(&discoverdev[x], 1023-x, "\"logfile\": \"\",");
	}
	if (strcmp(protocol, "zigbee_thermostat") == 0) {
		x += snprintf(&discoverdev[x], 1023-x, "\"localtemperature\": 0,");
		x += snprintf(&discoverdev[x], 1023-x, "\"heatingsetpoint\": 0,");
		x += snprintf(&discoverdev[x], 1023-x, "\"dimlevel\": 0,");
		x += snprintf(&discoverdev[x], 1023-x, "\"scheduler\": \"\",");
	}
	if (strcmp(protocol, "zigbee_temperature") == 0) {
		x += snprintf(&discoverdev[x], 1023-x, "\"temperature\": 0,");
		x += snprintf(&discoverdev[x], 1023-x, "\"humidity\": 0");
	}
	if (strcmp(protocol, "zigbee_huemotion") == 0) {
		x += snprintf(&discoverdev[x], 1023-x, "\"occupancy\": 0,");
		x += snprintf(&discoverdev[x], 1023-x, "\"illuminance\": 0,");
		x += snprintf(&discoverdev[x], 1023-x, "\"temperature\": 0,");
		x += snprintf(&discoverdev[x], 1023-x, "\"OccupiedToUnoccupiedDelay\": 0,");
		x += snprintf(&discoverdev[x], 1023-x, "\"battery\": 100,");
		x += snprintf(&discoverdev[x], 1023-x, "\"testmode\": 0,");
		x += snprintf(&discoverdev[x], 1023-x, "\"sensitivity\": 2,");
	}
	if (strcmp(protocol, "zigbee_shutter") == 0) {
		x += snprintf(&discoverdev[x], 1023-x, "\"WindowCoveringType\": 0,");
		x += snprintf(&discoverdev[x], 1023-x, "\"CurrentPositionLift\": 0,");
		x += snprintf(&discoverdev[x], 1023-x, "\"CurrentPositionTilt\": 0,");
		x += snprintf(&discoverdev[x], 1023-x, "\"CurrentPositionLiftPercentage\": 0,");
		x += snprintf(&discoverdev[x], 1023-x, "\"CurrentPositionTiltPercentage\": 0,");
		x += snprintf(&discoverdev[x], 1023-x, "\"OperationalStatus\": 0,");
		x += snprintf(&discoverdev[x], 1023-x, "\"InstalledClosedLimitLift\": 0,");
		x += snprintf(&discoverdev[x], 1023-x, "\"InstalledClosedLimitTilt\": 0,");
		x += snprintf(&discoverdev[x], 1023-x, "\"Mode\": 0,");
	}
	if (strcmp(protocol, "zigbee_dimmer") == 0 || strcmp(protocol, "zigbee_switch") == 0
			|| strcmp(protocol, "zigbee_motion") == 0
			|| strcmp(protocol, "zigbee_thermostat") == 0
			|| strcmp(protocol, "zigbee_huemotion") == 0
			|| strcmp(protocol, "zigbee_shutter") == 0
			|| strcmp(protocol, "zigbee_unknown") == 0) {
		x += snprintf(&discoverdev[x], 1023-x, "\"state\":\"off\"");
	}
	if (strcmp(protocol, "zigbee_motion") == 0 || strcmp(protocol, "zigbee_unknown") == 0) {
		x += snprintf(&discoverdev[x], 1023-x, ",\"battery\": 100,");
		x += snprintf(&discoverdev[x], 1023-x, "\"ZoneStatus\": 0");
	}
	if (strcmp(protocol, "zigbee_metering") == 0) {
		x += snprintf(&discoverdev[x], 1023-x, "\"CurrentSummationDelivered\": 0,");
		x += snprintf(&discoverdev[x], 1023-x, "\"InstantaneousDemand\": 0");
	}
	x += snprintf(&discoverdev[x], 1023-x, "}");

	if (! json_validate(discoverdev)) {
		logprintf(LOG_ERR, "create_zigbee_json_config discoverdev = %s", discoverdev);
		return;
	}
	if ((devname = MALLOC(devnamelen)) == NULL) {
		OUT_OF_MEMORY
	}
	snprintf(devname, devnamelen, "zigbee-0x%04X-0x%02X", shortaddr, endpoint);
	if ((configfile = MALLOC(filenamelen)) == NULL) {
		OUT_OF_MEMORY
	}
	snprintf(configfile, filenamelen, "/tmp/pilight_%s-0x%04X-0x%02X_json.config", protocol, shortaddr, endpoint);

	struct JsonNode *jdevnew = json_mkobject();
	json_append_member(jdevnew, devname, json_decode(discoverdev));

	// modifying json.config during runtime is not possible
	// writing to file in /tmp
	FILE *fp = NULL;
	if((fp = fopen(configfile, "w+")) == NULL) {
		logprintf(LOG_ERR, "cannot write config file: %s", configfile);
		return;
	}
	fseek(fp, 0L, SEEK_SET);
	char *content = NULL;
	if((content = json_stringify(jdevnew, "\t")) != NULL) {
		content[0] = ' ';                    // remove beginning '{'
		content[strlen(content) - 1] = ' ';  // remove trailing  '}'
		logprintf(LOG_INFO, "[ZigBee] Please add new Device to json.config \n%s", content);
		fwrite(content, sizeof(char), strlen(content), fp);
		json_free(content);
		logprintf(LOG_INFO, "[ZigBee] parseCommand created config file: %s", configfile);
	}
	fclose(fp);

	json_delete(jdevnew);
	FREE(devname);
	FREE(configfile);
}

void matchResponse(char *extaddr, int shortaddr, int endpoint, int cluster, char *profileid, char *protocol) {
	// check if match device already has been discovered
	if (checkDevAttributes(shortaddr, endpoint) == 1) {
		logprintf(LOG_DEBUG, "[ZigBee] matchResponse already found addr = 0x%04X, ep = %d, cluster = 0x%04X",
				shortaddr, endpoint, cluster);
		return;
	} else {
		logprintf(LOG_DEBUG, "[ZigBee] matchResponse add new addr = 0x%04X, ep = %d, cluster = 0x%04X, matchtime = %d",
				shortaddr, endpoint, cluster, matchtime);
	}
	addDevEntry(extaddr, shortaddr, 1 /* device type router*/);
	addDevEpEntry(shortaddr, endpoint);
	addDevProfileEntry(shortaddr, endpoint, profileid, "");
	addDevMatchClusterEntry(shortaddr, endpoint, cluster);

	// read basic cluster attributes from discovered device
	char command[MAX_STRLENGTH];
	snprintf(command, MAX_STRLENGTH, "b 0x%04X %d 0x%04X", shortaddr, endpoint, cluster);
	addsinglethread(command, matchtime);
	matchtime += 2;

	create_zigbee_json_config(shortaddr, endpoint, protocol);
}

void matchAttributeResponse(int shortaddr, int endpoint, int cluster, int attrid, char *attr, char *protocol) {
	if (attrid != BASIC_VER && (attrid < BASIC_MIN || attrid > BASIC_MAX)) {
		return;
	}

	addDevAttributeEntry(shortaddr, endpoint, attrid, attr);

	if (attrid != BASIC_VER) {  // waiting until all attributes are received
		return;
	}

	if (checkDevMatchClusterEntry(shortaddr, endpoint, cluster) == 0) {
		logprintf(LOG_DEBUG, "[ZigBee] matchAttributeResponse cluster does not match, addr = 0x%04X, ep = %d, cluster = 0x%04X",
				shortaddr, endpoint, cluster);
		return;
	}

	create_zigbee_json_config(shortaddr, endpoint, protocol);
}

struct zigbee_data_t *addZigbeeDevice(void *param, zigbee_data_t *zigbee_data_old, zigbee_cmd_t *zigbee_commands, char *protocol) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct JsonNode *jchild1 = NULL;
	struct zigbee_data_t *node = NULL;
	char *stmp = NULL;
	double itmp = 0.0;
	static int interval = 2;
	int match = 0;

	if(param == NULL) {
		return zigbee_data;
	}
	if((jdevice = json_first_child(param)) == NULL) {
		return zigbee_data;
	}
	if((jprotocols = json_find_member(jdevice, "protocol")) != NULL) {
		jchild = json_first_child(jprotocols);
		while(jchild) {
			if(strcmp(protocol, jchild->string_) == 0) {
				match = 1;
				break;
			}
			jchild = jchild->next;
		}
	}
	if(match == 0) {
		return zigbee_data;
	}

	if((node = MALLOC(sizeof(struct zigbee_data_t)))== NULL) {
		OUT_OF_MEMORY
	}
	memset(node, '\0', sizeof(struct zigbee_data_t));
	logprintf(LOG_DEBUG, "addZigbeeDevice %s", json_stringify(jdevice, NULL));

	if((jid = json_find_member(jdevice, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			jchild1 = json_first_child(jchild);
			while(jchild1) {
				if(strcmp(jchild1->key, "shortaddr") == 0 && jchild1->tag == JSON_STRING) {
					sscanf(jchild1->string_, "%x", &node->shortaddr);
				}
				if(strcmp(jchild1->key, "endpoint") == 0 && jchild1->tag == JSON_NUMBER) {
					node->endpoint = (uint8_t) jchild1->number_;
				}
				jchild1 = jchild1->next;
			}
			jchild = jchild->next;
		}
	}
	if(json_find_number(jdevice, "poll-interval", &itmp) == 0) {
		node->interval = (int) itmp;
	}
	if(json_find_number(jdevice, "state", &itmp) == 0) {
		node->currentstate = (int) itmp;
	}
	if(json_find_number(jdevice, "battery", &itmp) == 0) {
		node->battery = (int) itmp;
	}
	if(json_find_number(jdevice, "Mode", &itmp) == 0) {
		node->mode = (int) itmp;
	}
	if(json_find_number(jdevice, "WindowCoveringType", &itmp) == 0) {
		node->covertype = (int) itmp;
	}
	node->extaddr = NULL;
	if(json_find_string(jdevice, "extaddr", &stmp) == 0) {
		if((node->extaddr = MALLOC(strlen(stmp)+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(node->extaddr, stmp);
	} else {
		if((node->extaddr = MALLOC(1)) == NULL) {
			OUT_OF_MEMORY
		}
		memset(node->extaddr, '\0', 1);
	}
	if((node->dev = MALLOC(strlen(jdevice->key)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(node->dev, jdevice->key);
	node->protocol = NULL;
	if (protocol != NULL) {
		if((node->protocol = MALLOC(strlen(protocol)+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(node->protocol, protocol);
	}
	node->logfile = NULL;
	if(json_find_string(jdevice, "logfile", &stmp) == 0) {
		if((node->logfile = MALLOC(strlen(stmp)+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(node->logfile, stmp);
	}
	node->execute = NULL;
	if(json_find_string(jdevice, "execute", &stmp) == 0) {
		if((node->execute = MALLOC(strlen(stmp)+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(node->execute, stmp);
	}
	node->lasttime = 0;
	node->commands = zigbee_commands;
	node->nextcmd = zigbee_commands;

	node->next = zigbee_data;
	zigbee_data = node;

	addDevEntry(node->extaddr, node->shortaddr, 0 /*devicetype*/);
	addDevEpEntry(node->shortaddr, node->endpoint);

	uv_timer_t *timer_req;
	if ((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY
	}
	timer_req->data = node;
	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))zigbeethread, interval * 1000, -1);

	interval += 2; // wait 2 seconds between multiple read threads

	struct zigbee_data_t *tmp = zigbee_data;
	int i = 0;
	while(tmp) {
		logprintf(LOG_DEBUG, "addDevice %d. tmp->extaddr = %s, shortaddr = 0x%04X, protocol = %s, 0x%lX",
				i++, tmp->extaddr, tmp->shortaddr, tmp->protocol, pthread_self());
		tmp = tmp->next;
	}

	return zigbee_data;
}

int newstate(int shortAddr, uint8_t endpointId, int state, zigbee_data_t *zigbee_data) {
	struct zigbee_data_t *tmp = zigbee_data;
	while(tmp) {
		if (shortAddr == tmp->shortaddr && endpointId == tmp->endpoint) {
			if (state > 1) {
				state = 1 - tmp->currentstate; // toggle
			}
			if (state < 0) {
				state = tmp->currentstate;
			}
			tmp->currentstate = state;
			break;
		}
		tmp = tmp->next;
	}
	return state;
}

static void free_socket_reconnect(uv_handle_t *handle) {
	logprintf(LOG_ERR, "[ZigBee] free_socket_reconnect");
	reconnect(handle->data);
}

static void free_write_req(uv_write_t *req, int status) {
	if (req->data != NULL) {
		FREE(req->data);
	}
	FREE(req);
}

void zigbeeSendCommand(char *command) {
	char *sbuf = NULL;
	int cmdlen = 0, r = 0;

	if (zigbee_initialized == 0) {  // if not running in daemon
		printf("\nzigbeeSendCommand = %s\n", command);
		logprintf(LOG_ERR, "[ZigBee] client connection failed");
	} else {
		logprintf(LOG_DEBUG, "[ZigBee] zigbeeSendCommand send command %s", command);
		cmdlen = strlen(command) + 2;
		if ((sbuf = MALLOC(cmdlen)) == NULL) {
			OUT_OF_MEMORY
		}
		snprintf(sbuf, MAX_STRLENGTH, "%s\n", command);
		uv_buf_t buf = uv_buf_init(sbuf, cmdlen);
		uv_write_t *req = (uv_write_t *) MALLOC(sizeof(uv_write_t));
		if (req == NULL) {
			OUT_OF_MEMORY
		}
		r = uv_is_active((uv_handle_t *) socket_zigbee);
		if (r != 0) {
			req->data = sbuf;
			uv_write(req, (uv_stream_t *) socket_zigbee, &buf, 1, free_write_req);
		} else {
			logprintf(LOG_ERR, "[ZigBee] zigbeeSendCommand write error");
			if (socket_zigbee != NULL) {
				if (uv_is_closing((uv_handle_t *) socket_zigbee) != 0) {
					logprintf(LOG_ERR, "[ZigBee] zigbeeSendCommand handle is already closing");
					return;  // already closing, do not close again
				}
				socket_zigbee->data = sbuf;
				uv_close((uv_handle_t *) socket_zigbee, free_socket_reconnect);
			} else {
				reconnect(sbuf);
			}
		}
	}
}

static unsigned short zigbeeSettings(JsonNode *json) {
	if(strcmp(json->key, "location") == 0) {
		if(json->tag == JSON_STRING) {
			if((location = MALLOC(strlen(json->string_)+1)) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
			strcpy(location, json->string_); // for example localhost
		} else {
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

static void read_cb(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
	char stmp[17];
	int nl = 0;
	if(nread > 0) {
			char *p;
			char senddata[MAX_STRBUF];
			p = buf->base;
			while (p < buf->base + nread) {
				nl = strcspn(p, "\n");  // find newline

				memset(senddata, '\0', MAX_STRBUF);
				strncpy(senddata, p, nl);
				p += nl + 1;
				if (strncmp(senddata, "<-LQI", strlen("<-LQI")) == 0) {
					addlqi(senddata); // handle LQI neighbortable
					continue;
				}
				if (strncmp(senddata, "<-EP", strlen("<-EP")) == 0) {
					addep(senddata); // handle active endpoint response
					continue;
				}
				if (strncmp(senddata, "<-CLUSTER", strlen("<-CLUSTER")) == 0) {
					addcluster(senddata);
					continue;
				}
				if (sscanf(senddata,"<-ZCL attribute report 0x%16s 0x0000 ", stmp) == 1) {
					addattr(senddata);
				}

				logprintf(LOG_DEBUG, "[ZigBee] %s", senddata);
				char status[] = "<-"; // only use string if data starts with "<-"
				if (strncmp(senddata, status, strlen(status)) == 0) {
					struct JsonNode *json = json_mkobject();
					struct JsonNode *jcode = json_mkobject();
					json_append_member(json, "message", jcode);
					json_append_member(json, "origin", json_mkstring("receiver"));
					json_append_member(jcode, "zigbee_response", json_mkstring(senddata));

					receive_parse_api(json, ZIGBEE);
					json_delete(json);
				}
			}
		}
	if(nread < 0) {
		if(nread != UV_EOF) {
			logprintf(LOG_ERR, "[ZigBee] read_cb: %s\n", uv_strerror(nread));
		} else {
			uv_read_stop((uv_stream_t *)client);
		}
	}

	free(buf->base);
	return;
}

static void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
	buf->base = malloc(suggested_size);
	buf->len = suggested_size;
	memset(buf->base, '\0', buf->len);
}

static void on_connect(uv_connect_t *req, int status) {
	if (status < 0) {
		logprintf(LOG_ERR, "[ZigBee] New connection error %s\n", uv_strerror(status));
	} else {
//		req->handle->data = req->data;
		uv_read_start((uv_stream_t *)req->handle, alloc_cb, read_cb);
	}

	FREE(req);
}

static unsigned short zigbeeHwInit(void) {
	if(socket_zigbee != NULL) {
		FREE(socket_zigbee);
	}
	socket_zigbee = (uv_tcp_t *) MALLOC(sizeof(uv_tcp_t));
	if (socket_zigbee == NULL) {
		OUT_OF_MEMORY
	}
	socket_zigbee->data = NULL;

	uv_tcp_init(uv_default_loop(), socket_zigbee);

	uv_connect_t *zigbee_connect = (uv_connect_t *) MALLOC(sizeof(uv_connect_t));
	if (zigbee_connect == NULL) {
		OUT_OF_MEMORY
	}

	struct sockaddr_in dest;
	uv_ip4_addr(location, zigbee_port, &dest);

	int r = uv_tcp_connect(zigbee_connect, socket_zigbee, (const struct sockaddr *)&dest, on_connect);
	if (r) {
		logprintf(LOG_ERR, "[ZigBee] Connect error %s\n", uv_strerror(r));
		return EXIT_FAILURE;
	}
	zigbee_initialized = 1;
	logprintf(LOG_INFO, "[ZigBee] connected to location = %s, port = %d", location, zigbee_port);

	return EXIT_SUCCESS;
}

static void reconnect(char *sbuf) {
	logprintf(LOG_ERR, "[ZigBee] Trying to reconnect");
	if (zigbeeHwInit() == EXIT_SUCCESS) {
		if (sbuf == NULL) {
			return;
		}
		logprintf(LOG_DEBUG, "[ZigBee] reconnect trying to send %s", sbuf);
		uv_buf_t buf = uv_buf_init(sbuf, strlen(sbuf) + 1);
		uv_write_t *req = (uv_write_t *) MALLOC(sizeof(uv_write_t));
		if (req == NULL) {
			OUT_OF_MEMORY
		}
		req->data = sbuf;
		uv_write(req, (uv_stream_t *) socket_zigbee, &buf, 1, free_write_req);
		if (socket_zigbee != NULL) {
			socket_zigbee->data = NULL;  // do not try to send again
		}
	} else {
		logprintf(LOG_ERR, "[ZigBee] Failed to reconnect");
	}
}

static unsigned short zigbeeHwDeInit(void) {
	logprintf(LOG_NOTICE, "[ZigBee] Close connection %s", location);
	if (zigbee_initialized == 1) {
		uv_close((uv_handle_t*) socket_zigbee, NULL);
		zigbee_initialized = 0;
	}
	return EXIT_SUCCESS;
}

static int zigbeegc(void) {
	int i = 0;
	if(location != NULL) {
		FREE(location);
	}
	struct zigbee_devices_t *tmp = NULL;
	while (zdev) {
		tmp = zdev;
		if (tmp->extaddr != NULL) {
			FREE(tmp->extaddr);
		}
		if (tmp->endpoints != NULL) {
			FREE(tmp->endpoints);
		}
		if (tmp->clusters != NULL) {
			for (i = 0; i < tmp->epcount; i++) {
				FREE(tmp->clusters[i]);
			}
			FREE(tmp->clusters);
		}
		if (tmp->clcount != NULL) {
			FREE(tmp->clcount);
		}
		if (tmp->profile != NULL) {
			for (i = 0; i < tmp->epcount; i++) {
				FREE(tmp->profile[i]);
			}
			FREE(tmp->profile);
		}
		for (i = 0; i < BASIC_ATTR; i++) {
			if (tmp->attr[i] != NULL) {
				FREE(tmp->attr[i]);
			}
		}
		zdev = zdev->next;
		FREE(tmp);
	}
	if (zdev != NULL) {
		FREE(zdev);
	}
	struct zigbee_data_t *tmp1 = NULL;
	while(zigbee_data) {
		tmp1 = zigbee_data;
		FREE(tmp1->dev);
		if (tmp1->extaddr != NULL) {
			FREE(tmp1->extaddr);
		}
		if (tmp1->protocol != NULL) {
			FREE(tmp1->protocol);
		}
		if (tmp1->logfile != NULL) {
			FREE(tmp1->logfile);
		}
		if (tmp1->execute != NULL) {
			FREE(tmp1->execute);
		}
		zigbee_data = zigbee_data->next;
		FREE(tmp1);
	}
	if (zigbee_data != NULL) {
		FREE(zigbee_data);
	}

	return 1;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void zigbeeInit(void) {
	hardware_register(&zigbee);
	hardware_set_id(zigbee, "zigbee");

	options_add(&zigbee->options, 'l', "location", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);

	zigbee->hwtype = ZIGBEE;
	zigbee->comtype = COMAPI;
	zigbee->init = &zigbeeHwInit;
	zigbee->deinit = &zigbeeHwDeInit;
	zigbee->sendAPI = &zigbeeSend;
	zigbee->settings=&zigbeeSettings;
	zigbee->gc=&zigbeegc;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "zigbee";
	module->version = "1.0";
	module->reqversion = "7.0";
	module->reqcommit = NULL;
}

void init(void) {
	zigbeeInit();
}
#endif
