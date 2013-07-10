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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>

#include "config.h"
#include "log.h"
#include "options.h"
#include "socket.h"

typedef enum {
	WELCOME,
	IDENTIFY,
	REJECT,
	RECEIVE
} steps_t;

int main(void) {
	enable_shell_log();
	disable_file_log();

	set_loglevel(LOG_NOTICE);

	progname = malloc((10*sizeof(char))+1);
	strcpy(progname, "433-receive");

	JsonNode *json = json_mkobject();

    int sockfd = 0;
    char *recvBuff;
	char *message = NULL;
	steps_t steps = WELCOME;

    if((sockfd = connect_to_server(strdup("127.0.0.1"), 5000)) == -1) {
		logprintf(LOG_ERR, "could not connect to 433-daemon");
		return EXIT_FAILURE;
	}

	while(1) {
		/* Clear the receive buffer again and read the welcome message */
		if((recvBuff = socket_read(sockfd)) != NULL) {

			json = json_decode(recvBuff);
			json_find_string(json, "message", &message);
		} else {
			goto close;
		}

		switch(steps) {
			case WELCOME:
				if(strcmp(message, "accept connection") == 0) {
					socket_write(sockfd, "{\"message\":\"client receiver\"}");
					steps=IDENTIFY;
				}
			break;
			case IDENTIFY:
				if(strcmp(message, "accept client") == 0) {
					steps=RECEIVE;
				}
				if(strcmp(message, "reject client") == 0) {
					steps=REJECT;
				}
			break;
			case RECEIVE:
				printf("%s\n", json_stringify(json, "\t"));
			break;
			case REJECT:
			default:
				goto close;
			break;
		}
	}
close:
	socket_close(sockfd);
return EXIT_SUCCESS;
}
