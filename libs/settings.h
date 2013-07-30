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

#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include "protocol.h"

//#define PULSE_LENGTH 	165 	// wiringPi pulse length
#define PULSE_LENGTH		295

#define PORT 				5000
#define MAX_CLIENTS			30
#define BUFFER_SIZE			1025
#define BIG_BUFFER_SIZE		1025000

#define PID_FILE			"/var/run/433-daemon.pid"
#define CONFIG_FILE			"/etc/433-daemon/config.json"
#define LOG_FILE			"/var/log/433-daemon.log"
#define SETTINGS_FILE		"/etc/433-daemon/settings.json"
#define DEFAULT_LIRC_SOCKET "/dev/lirc0"

#define SEND_REPEATS	10
#define RECEIVE_REPEATS	1
#define FREQ433			433920
#define FREQ38			38000

typedef union value_t {
	int ivalue;
	char *cvalue;
} value_t;

typedef struct settings_t {
	char name[15];
	int type;
	union value_t *value;
	struct settings_t *next;
} settings_t;

struct settings_t *settings;

char *progname;
/* The location of the settings file */
char *settingsfile;

void settings_add_string_node(const char *name, char *value);
void settings_add_int_node(const char *name, int value);
int settings_find_number(const char *name, int *out);
int settings_find_string(const char *name, char **out);
int settings_path_exists(char *fil);
int settings_parse(JsonNode *root);
int settings_write(char *content);
int settings_read(void);
int settings_set_file(char *settfile);

#endif
