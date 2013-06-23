/*
	Copyright (C) 2013 CurlyMo

	This file is part of the Raspberry Pi 433.92Mhz transceiver.

    Raspberry Pi 433.92Mhz transceiver is free software: you can redistribute
	it and/or modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation, either version 3 of the License,
	or (at your option) any later version.

    Raspberry Pi 433.92Mhz transceiver is distributed in the hope that it will
	be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Raspberry Pi 433.92Mhz transceiver. If not, see
	<http://www.gnu.org/licenses/>
*/

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "protocol.h"

//#define PULSE_LENGTH 	165 	// wiringPi pulse length
#define PULSE_LENGTH	295

#define PORT 			5000
#define MAX_CLIENTS		30
#define BUFFER_SIZE		1025

#define PID_FILE		"/var/run/433-deamon.pid"
#define CONFIG_FILE		"./433-daemon.conf"
#define LOG_FILE		"/var/log/433-daemon.log"

#define SEND_REPEATS	10
#define FREQ433			433920
#define FREQ38			38000

#define MAX_DEVICES		255 	// Per location
#define MAX_SETTINGS	10  	// Per protocol
#define MAX_VALUES		25  	// Per values list

typedef struct conf_locations_t conf_locations_t;
typedef struct conf_devices_t conf_devices_t;
typedef struct conf_settings_t conf_settings_t;
typedef struct conf_values_t conf_values_t;

/*
|------------------|
| conf_locations_t |
|------------------|
| id               |
| name		       |
| devices	       | ---
|------------------|   |
				       |
|------------------|   |
|  conf_devices_t  | <--
|------------------|
| id               |
| name		       |
| protocol	       | --> protocol_t <protocol.h>
| settings	       | ---
|------------------|   |
				       |
|------------------|   |
| conf_settings_t  | <--
|------------------|
| name             |
| values	       | ---
|------------------|   |
				       |
|------------------|   |
|  conf_values_t   | <--
|------------------|
| value            |
| type		       |
|------------------|
*/

struct conf_values_t {
	char *value;
	int type;
	struct conf_values_t *next;
};

struct conf_settings_t {
	char *name;
	struct conf_values_t *values;
	struct conf_settings_t *next;
};

struct conf_devices_t {
	char *id;
	char *name;
	char *protocol;
	struct conf_settings_t *settings;
	struct conf_devices_t *next;
};

struct conf_locations_t {
	char *id;
	char *name;
	struct conf_devices_t *devices;
	struct conf_locations_t *next;
};

int read_config(char *configfile);

char *progname;

/* Struct to store the locations */
struct conf_locations_t *locations;
/* Struct to store the devices per location */
struct conf_devices_t *devices;
/* Struct to store the device specific settings */
struct conf_settings_t *settings;
/* Struct to store the device values list settings */
struct conf_values_t *values;

#endif