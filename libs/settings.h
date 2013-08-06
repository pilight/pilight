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

/* Uncomment this line if you want to use the this
 * program with the lirc_rpi module.
 * This is adviced when running the daemon without
 * the low-pass filter.
 *
 * #define USE_LIRC
 *
 */

#ifndef USE_LIRC

	#define GPIO_IN_PIN			1
	#define GPIO_OUT_PIN		0

	#define POLL_TIMEOUT		1000
	#define RDBUF_LEN			5
	#define GPIO_FN_MAXLEN		32

#else

	#define DEFAULT_LIRC_SOCKET	"/dev/lirc0"
	#define FREQ433				433920
	#define FREQ38				38000

#endif

#define PULSE_LENGTH		295

#define PORT 				5000
#define MAX_CLIENTS			30
#define BUFFER_SIZE			1025
#define BIG_BUFFER_SIZE		1025000

#define PID_FILE			"/var/run/433-daemon.pid"
#define CONFIG_FILE			"/etc/433-daemon/config.json"
#define LOG_FILE			"/var/log/433-daemon.log"
#define SETTINGS_FILE		"/etc/433-daemon/settings.json"

#define SEND_REPEATS		10
#define RECEIVE_REPEATS		1
#define MULTIPLIER			0.3


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
