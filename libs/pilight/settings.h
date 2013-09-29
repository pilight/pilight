/*
	Copyright (C) 2013 CurlyMo

	This file is part of pilight.

    pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

    pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include "protocol.h"

#define VERSION				1.0

#define HW_MODE				"gpio"

#define GPIO_IN_PIN			1
#define GPIO_OUT_PIN		0

#define POLL_TIMEOUT		1000
#define RDBUF_LEN			5
#define GPIO_FN_MAXLEN		32

#define DEFAULT_LIRC_SOCKET	"/dev/lirc0"
#define FREQ433				433920
#define FREQ38				38000

#define PULSE_LENGTH		295

#define PORT 				5000
#define WEBSERVER_PORT		5001
#define WEBSERVER_ROOT		"/usr/local/share/pilight/"
#define WEBSERVER_ENABLE	1

#define MAX_CLIENTS			30
#define BUFFER_SIZE			1025
#define BIG_BUFFER_SIZE		1024001

#define PID_FILE			"/var/run/pilight.pid"
#define CONFIG_FILE			"/etc/pilight/config.json"
#define LOG_FILE			"/var/log/pilight.log"
#define SETTINGS_FILE		"/etc/pilight/settings.json"

#define SEND_REPEATS		10
#define RECEIVE_REPEATS		1
#define MULTIPLIER			0.3

typedef struct settings_t {
	char *name;
	int type;
	char *value;
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
int settings_gc(void);

#endif

#if defined(__linux__) && defined(__x86_64__)
#define _GNU_SOURCE
#endif
