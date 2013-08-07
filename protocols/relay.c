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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "settings.h"
#include "log.h"
#include "protocol.h"
#include "gpio.h"
#include "relay.h"

void relayCreateMessage(int gpio, int state) {
	relay.message = json_mkobject();
	json_append_member(relay.message, "gpio", json_mknumber(gpio));
	if(state == 1)
		json_append_member(relay.message, "state", json_mkstring("on"));
	else
		json_append_member(relay.message, "state", json_mkstring("off"));
}

int relayCreateCode(JsonNode *code) {
	int gpio = -1;
	int state = -1;
	char *tmp;
	char command[50];
	char folder[32];
	struct stat s;

	relay.length = 0;

	if(json_find_string(code, "gpio", &tmp) == 0)
		gpio=atoi(tmp);
	if(json_find_string(code, "off", &tmp) == 0)
		state=0;
	else if(json_find_string(code, "on", &tmp) == 0)
		state=1;
	
	if(gpio == -1 || state == -1) {
		logprintf(LOG_ERR, "relay: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(gpio > 7 || gpio < 0) {
		logprintf(LOG_ERR, "relay: invalid gpio range");
		return EXIT_FAILURE;
#ifndef USE_LIRC
	} else if(gpio == GPIO_IN_PIN || gpio == GPIO_OUT_PIN) {
		logprintf(LOG_ERR, "relay: gpio's already in use");
		return EXIT_FAILURE;
#endif
	} else {
		if(strstr(progname, "daemon") != 0) {
			sprintf(folder, "/sys/class/gpio/gpio%d 2>/dev/null", gpio_wiringPi2BCM(gpio));
			if(stat(folder, &s) != -1 && S_ISDIR(s.st_mode) == 0) {
				gpio_request(gpio);
			}
			gpio_pinmode(gpio, OUTPUT);
			if(state == 1) {
				sprintf(command, "echo 0 > /sys/class/gpio/gpio%d/value 2>/dev/null", gpio_wiringPi2BCM(gpio));
			} else if(state == 0) {
				sprintf(command, "echo 1 > /sys/class/gpio/gpio%d/value 2>/dev/null", gpio_wiringPi2BCM(gpio));
			}
			system(command);
			relayCreateMessage(gpio, state);
		}
	}
	return EXIT_SUCCESS;
}

void relayPrintHelp(void) {
	printf("\t -t --on\t\t\tturn the relay on\n");
	printf("\t -f --off\t\t\tturn the relay off\n");
	printf("\t -g --gpio=gpio\t\t\tthe gpio the relay is connected to\n");
}

void relayInit(void) {

	strcpy(relay.id, "relay");
	protocol_add_device(&relay, "relay", "Control connected relay's");
	relay.type = SWITCH;
	relay.message = malloc(sizeof(JsonNode));

	options_add(&relay.options, 't', "on", no_value, config_state, NULL);
	options_add(&relay.options, 'f', "off", no_value, config_state, NULL);
	options_add(&relay.options, 'g', "gpio", has_value, config_id, "^[0-7]{1}$");

	relay.createCode=&relayCreateCode;
	relay.printHelp=&relayPrintHelp;

	protocol_register(&relay);
}
