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

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <syslog.h>
#include "config.h"
#include "log.h"
#include "options.h"

int main() {
	enable_shell_log();
	disable_shell_log();
	set_loglevel(LOG_INFO);

    int sockfd = 0, n = 0, connected = 0;
    char recvBuff[BUFFER_SIZE];
    char *message;

    struct sockaddr_in serv_addr;

	memset(recvBuff, '\0', sizeof(recvBuff));

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        logprintf(LOG_ERR, "could not create socket");
		return EXIT_FAILURE;
    }

    memset(&serv_addr, '\0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		logprintf(LOG_ERR, "could not connect to 433-daemon");
		return EXIT_FAILURE;
    }

	while(1) {
		bzero(recvBuff,BUFFER_SIZE);
		if((n = read(sockfd, recvBuff, sizeof(recvBuff)-1)) < 1) {
			logprintf(LOG_ERR, "could not read from socket");
			goto close;
		}

		if(n > 0) {
			recvBuff[n]='\0';
			if(connected == 0) {
				if(strcmp(recvBuff,"ACCEPT CONNECTION\n") == 0) {
					message="CLIENT RECEIVER\n";
					if((n = write(sockfd, message, strlen(message))) < 0) {
						logprintf(LOG_ERR, "could not write to socket");
						goto close;
					}
				}
				if(strcmp(recvBuff,"ACCEPT CLIENT\n") == 0) {
					connected=1;
				}
			} else {
				printf("%s\n",recvBuff);
			}
		}
	}
close:
	shutdown(sockfd, SHUT_WR);
	close(sockfd);
return EXIT_SUCCESS;
}
