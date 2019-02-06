/*
 * raspbee.h
 *
 *  Created on: Sep 21, 2016
 *      Author: ma-ca
 */

#ifndef _HARDWARE_ZIGBEE_H_
#define _HARDWARE_ZIGBEE_H_

#include "../hardware/hardware.h"

#define MAX_STRLENGTH 500 // Max string length for sending commands to RaspBee

#define BASIC_ATTR 4  // ZigBee Basic Cluster Attributes 0x0004,0x0005,0x0006 and 0x4000
#define BASIC_MIN 4
#define BASIC_MAX 6
#define BASIC_VER 0x4000  // Basic Attribute 0x4000 has Software Version on Zigbee Light Link

extern int matchtime;

typedef struct zigbee_cmd_t {
	char *command;
	struct zigbee_cmd_t *next;
} zigbee_cmd_t;

typedef struct zigbee_data_t {
	char *dev;
	char *protocol;
	char *extaddr;          // extended Address 64-Bit
	int shortaddr;			// short Address 16-Bit
	uint8_t endpoint;       // endpoint Id 8-Bit
	int currentstate;       // 0 = off, 1 = on (ONOFF_CLUSTER attribute value)
	int interval;           // seconds to poll device state, 0 = do not poll
	long lasttime;			// timestamp last time a button was pressed (Philips Hue Dimmer Switch)
	int buttoncount;		// number of times a button was pressed repeatedly
	int battery;			// battery percentage 0, 33, 66, 100
	int covertype;			// WindowCoveringType on shutter J1
	int step;				// calibration step on shutter J1
	int mode;				// calibration mode on shutter J1
	char *logfile;			// logfile if needed to store metering values
	char *execute;			// script to execute when button is pressed on Hue Dimmer Switch
	zigbee_cmd_t *commands; // zigbee commands used to read attribute values
	zigbee_cmd_t *nextcmd;  // command to execute on next thread
	uv_timer_t *timer_req;

	struct zigbee_data_t *next;
} zigbee_data_t;

typedef struct zigbee_devices_t {
	char *extaddr;          // extended Address 64-Bit
	int shortaddr;			// short Address 16-Bit
	int devicetype;			// device type 0=coordinator, 1=router, 2=end device
	uint8_t *endpoints;		// endpoints
	uint8_t epcount;		// endpoints count
	uint16_t **clusters;	// clusters for each endpoint
	uint8_t *clcount;		// clusters count for each endpoint
	int matchcluster;		// cluster response from match request
	char **profile;			// profile for each endpoint ZHA "0x0104" ZLL "0xC05E"
	char **deviceid;		// deviceid for each endpoint
	char *attr[BASIC_ATTR];	// basic cluster attributes 4,5,6,0x0400
	struct zigbee_devices_t *next;
} zigbee_devices_t;

struct hardware_t *zigbee;

void zigbeeInit(void);
void reverseStr(char *inputstr, char *out, int bytecount);
void sendPowerCmd(int shortaddr);
void sendBindCmd(int shortAddr, int srcEp, int dstEp, int dobind, char *clusterIn, char *srcExtAddrIn, char *dstExtAddrIn, int sec);
void sendRepoCmd(int shortAddr, int dstEp, char *clusterIn, char *attributeid, char *minimum, char *maximum, int attrtype, int reportchange);
void sendIdentifyCmd(int shortAddr, int dstEp, int duration);
char *getextaddr(int shortaddr);
void broadcast_message(char *message, char *protocol);
void discoverSingleDevice(int shortaddr, int endpoint);
void showDiscoveredDevices();
void autoDiscoverDevices();
void *threadDiscover(void *param);
zigbee_devices_t *getlqi();
void *zigbeethread(void *param);
void *singlethread(void *param);
void addsinglethread(char *command, int sec);
struct zigbee_data_t *addZigbeeDevice(void *param, zigbee_data_t *zigbee_data, zigbee_cmd_t *zigbee_commands, char *protocol);
int newstate(int shortAddr, uint8_t endpointId, int state, zigbee_data_t *zigbee_data);
void matchResponse(char *extaddr, int shortaddr, int endpoint, int cluster, char *profileid, char *protocol);
void matchAttributeResponse(int shortaddr, int endpoint, int cluster, int attrid, char *attr, char *protocol);
void zigbeeSendCommand(char *command);

#endif /* _HARDWARE_ZIGBEE_H_ */
