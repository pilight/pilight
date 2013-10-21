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

#ifndef _PILIGHT_H_
#define _PILIGHT_H_

#include "json.h"

/* Internals */

#define WEBSERVER

/* Protocol support */

#define PROTOCOL_ALECTO
#define PROTOCOL_COGEX_SWITCH
#define PROTOCOL_COCO_SWITCH
#define PROTOCOL_DS18B20
#define PROTOCOL_DIO_SWITCH
#define PROTOCOL_GENERIC_DIMMER
#define PROTOCOL_GENERIC_SWITCH
#define PROTOCOL_GENERIC_WEATHER
#define PROTOCOL_HOMEEASY_OLD
#define PROTOCOL_IMPULS
#define PROTOCOL_INTERTECHNO_SWITCH
#define PROTOCOL_INTERTECHNO_OLD
#define PROTOCOL_KAKU_DIMMER
#define PROTOCOL_KAKU_OLD
#define PROTOCOL_KAKU_SWITCH
#define PROTOCOL_NEXA_SWITCH
#define PROTOCOL_RAW
#define PROTOCOL_RELAY
#define PROTOCOL_ELRO
#define PROTOCOL_SELECTREMOTE

/* Hardware support */

#define HARDWARE_433_GPIO
#define HARDWARE_433_MODULE

#define VERSION				1.0

#ifdef HARDWARE_433_MODULE
	#define HW_MODE				"gpio"
#elif defined(HARDWARE_433_MODULE)
	#define HW_MODE				"module"
#else
	#define HW_MODE				"none"
#endif

#ifdef HARDWARE_433_MODULE
	#define GPIO_IN_PIN			1
	#define GPIO_OUT_PIN		0

	#define POLL_TIMEOUT		1000
	#define RDBUF_LEN			5
	#define GPIO_FN_MAXLEN		32
#endif

#ifdef HARDWARE_433_MODULE
	#define DEFAULT_LIRC_SOCKET	"/dev/lirc0"
	#define FREQ433				433920
	#define FREQ38				38000
#endif

#define PULSE_DIV			34

#define PORT 				5000

#ifdef WEBSERVER
	#define WEBSERVER_PORT		5001
	#define WEBSERVER_ROOT		"/usr/local/share/pilight/"
	#define WEBSERVER_ENABLE	1
	#define WEBSERVER_CACHE		1
#endif

#define MAX_CLIENTS			30
#define BUFFER_SIZE			1025
#define BIG_BUFFER_SIZE		1024001

#define SEND_BUFFER			10000

#define PID_FILE			"/var/run/pilight.pid"
#define CONFIG_FILE			"/etc/pilight/config.json"
#define LOG_FILE			"/var/log/pilight.log"
#define SETTINGS_FILE		"/etc/pilight/settings.json"

#define SEND_REPEATS		10
#define RECEIVE_REPEATS		1
#define MULTIPLIER			0.3

typedef struct pilight_t {
    int (*broadcast)(char *name, JsonNode *message);
    void (*send)(JsonNode *json);
    void (*receive)(int *rawcode, int rawlen, int plslen);
} pilight_t;

struct pilight_t pilight;

#endif
