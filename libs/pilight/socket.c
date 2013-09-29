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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "settings.h"
#include "log.h"
#include "gc.h"
#include "socket.h"

char *readBuff = NULL;
char *recvBuff = NULL;
char *sendBuff = NULL;
unsigned short socket_loop = 1;

int socket_gc(void) {
	if(readBuff) {
		free(readBuff);
	}
	if(recvBuff) {
		free(recvBuff);
	}
	if(sendBuff) {
		free(sendBuff);
	}
	socket_loop = 0;
	logprintf(LOG_DEBUG, "garbage collected socket library");
	return EXIT_SUCCESS;
}

int socket_check_whitelist(char *ip) {
	char *whitelist = NULL;
	unsigned int client[4] = {0};
	int x = 0, i = 0, error = 1;
	char *pch = NULL;
	char wip[16] = {'\0'};
	
	pch = strtok(ip, ".");
	x = 0;
	while(pch) {
		client[x] = (unsigned int)atoi(pch);
		x++;
		pch = strtok(NULL, ".");
	}

	if(settings_find_string("whitelist", &whitelist) != 0) {
		return 0;
	}
	if(strlen(whitelist) == 0) {
		return 0;
	}

	char *tmp = whitelist;
	x = 0;
	while(*tmp != '\0') {
		while(*tmp == ',' || *tmp == ' ') {
			tmp++;
		}
		wip[x] = *tmp;
		x++;
		tmp++; 

		if(*tmp == '\0' || *tmp == ',') {
			x = 0;
			unsigned int lower[4] = {0};
			unsigned int upper[4] = {0};
			
			i = 0;
			pch = strtok(wip, ".");
			while(pch) {
				if(strcmp(pch, "*") == 0) {
					lower[i] = 0;
					upper[i] = 255;
				} else {
					lower[i] = (unsigned int)atoi(pch);
					upper[i] = (unsigned int)atoi(pch);
				}
				pch = strtok(NULL, ".");
				i++;
			}

			unsigned int wlower = lower[0] << 24 | lower[1] << 16 | lower[2] << 8 | lower[3];
			unsigned int wupper = upper[0] << 24 | upper[1] << 16 | upper[2] << 8 | upper[3];
			unsigned int nip = client[0] << 24 | client[1] << 16 | client[2] << 8 | client[3];

			if((nip >= wlower && nip <= wupper) || (nip == 2130706433)) {
				error = 0;
			}
			memset(wip, '\0', 16);
		}		
	}

	free(pch);

	return error;
}

/* Start the socket server */
int socket_start(unsigned short port) {
	//gc_attach(socket_gc);

	int opt = 1;
    struct sockaddr_in address;

    //create a master socket
    if((serverSocket = socket(AF_INET , SOCK_STREAM , 0)) == 0)  {
        logprintf(LOG_ERR, "could not create new socket");
        exit(EXIT_FAILURE);
    }

    //set master socket to allow multiple connections , this is just a good habit, it will work without this
    if(setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ) {
        logprintf(LOG_ERR, "could not set proper socket options");
        exit(EXIT_FAILURE);
    }

    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    //bind the socket to localhost
    if (bind(serverSocket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        logprintf(LOG_ERR, "cannot bind to socket port %d, address already in use?", address);
        exit(EXIT_FAILURE);
    }

    //try to specify maximum of 3 pending connections for the master socket
    if(listen(serverSocket, 3) < 0) {
        logprintf(LOG_ERR, "failed to listen to socket");
        exit(EXIT_FAILURE);
    }

	logprintf(LOG_INFO, "daemon listening to port: %d", port);

    return 0;
}

int socket_connect(char *address, unsigned short port) {
	struct sockaddr_in serv_addr;
	int sockfd;

	if(!sendBuff) {
		sendBuff = malloc(BIG_BUFFER_SIZE);
		recvBuff = malloc(BIG_BUFFER_SIZE);
	}
	
	/* Try to open a new socket */
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        logprintf(LOG_ERR, "could not create socket");
		return -1;
    }

	/* Clear the server address */
    memset(&serv_addr, '\0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, address, &serv_addr.sin_addr);

	/* Connect to the server */
    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		return -1;
    } else {
		return sockfd;
	}
}


void socket_close(int sockfd) {
	int i = 0;
	struct sockaddr_in address;
	int addrlen = sizeof(address);

	if(sockfd > 0) {
		getpeername(sockfd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
		logprintf(LOG_INFO, "client disconnected, ip %s, port %d", inet_ntoa(address.sin_addr), ntohs(address.sin_port));				

		for(i=0;i<MAX_CLIENTS;i++) {
			if(socket_clients[i] == sockfd) {
				socket_clients[i] = 0;
				break;
			}
		}
		close(sockfd);
	}
}

void socket_write(int sockfd, const char *msg, ...) {
	va_list ap;

	if(strlen(msg) > 0 && sockfd > 0) {
		memset(sendBuff, '\0', BIG_BUFFER_SIZE);

		va_start(ap, msg);
		vsprintf(sendBuff, msg, ap);
		va_end(ap);
		strcat(sendBuff, "\n");

		if(send(sockfd, sendBuff, strlen(sendBuff), MSG_NOSIGNAL) == -1) {
			logprintf(LOG_DEBUG, "socket write failed: %s", sendBuff);
			socket_close(sockfd);
		} else {
			if(strcmp(sendBuff, "BEAT\n") != 0) {
				logprintf(LOG_DEBUG, "socket write succeeded: %s", sendBuff);
			}
			// Prevent buffering of messages
			usleep(100);
		}
	}
}

void socket_write_big(int sockfd, const char *msg, ...) {
	va_list ap;

	if(strlen(msg) > 0 && sockfd > 0) {
		memset(sendBuff, '\0', BIG_BUFFER_SIZE);

		va_start(ap, msg);
		vsprintf(sendBuff, msg, ap);
		va_end(ap);
		strcat(sendBuff, "\n");

		if(send(sockfd, sendBuff, strlen(sendBuff), MSG_NOSIGNAL) == -1) {
			logprintf(LOG_DEBUG, "socket write failed: %s", sendBuff);
			socket_close(sockfd);
		} else {
			if(strcmp(sendBuff, "BEAT\n") != 0) {
				logprintf(LOG_DEBUG, "socket write succeeded: %s", sendBuff);
			}
			// Prevent buffering of messages
			usleep(100);
		}
	}
}

char *socket_read(int sockfd) {
	memset(recvBuff, '\0', BUFFER_SIZE);

	if(read(sockfd, recvBuff, BUFFER_SIZE) < 1) {
		return NULL;
	} else {
		return recvBuff;
	}
}

char *socket_read_big(int sockfd) {
	memset(recvBuff, '\0', BIG_BUFFER_SIZE);

	if(read(sockfd, recvBuff, BIG_BUFFER_SIZE) < 1) {
		return NULL;
	} else {
		return recvBuff;
	}
}

void *socket_wait(void *param) {
	struct socket_callback_t *socket_callback = (struct socket_callback_t *)param;

	int activity;
	int i, n, sd;
    int max_sd;
    struct sockaddr_in address;
	int addrlen = sizeof(address);
	fd_set readfds;
	char *pch;
	readBuff = malloc(BIG_BUFFER_SIZE);

	while(socket_loop) {
		do {
			//clear the socket set
			FD_ZERO(&readfds);

			//add master socket to set
			FD_SET((long unsigned int)serverSocket, &readfds);
			max_sd = serverSocket;

			//add child sockets to set
			for(i=0;i<MAX_CLIENTS;i++) {
				//socket descriptor
				sd = socket_clients[i];

				//if valid socket descriptor then add to read list
				if(sd > 0)
					FD_SET((long unsigned int)sd, &readfds);

				//highest file descriptor number, need it for the select function
				if(sd > max_sd)
					max_sd = sd;
			}
			//wait for an activity on one of the sockets, timeout is NULL, so wait indefinitely
			activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
		} while(activity == -1 && errno == EINTR && socket_loop);

        //If something happened on the master socket, then its an incoming connection
        if(FD_ISSET((long unsigned int)serverSocket, &readfds)) {
            if((clientSocket = accept(serverSocket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
                logprintf(LOG_ERR, "failed to accept client");
                exit(EXIT_FAILURE);
            }
			if(socket_check_whitelist(inet_ntoa(address.sin_addr)) != 0) {
				logprintf(LOG_INFO, "rejected client, ip: %s, port: %d", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
				close(clientSocket);
			} else {
				//inform user of socket number - used in send and receive commands
				logprintf(LOG_INFO, "new client, ip: %s, port: %d", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
				logprintf(LOG_DEBUG, "client fd: %d", clientSocket);
				//send new connection accept message
				//socket_write(clientSocket, "{\"message\":\"accept connection\"}");

				//add new socket to array of sockets
				for(i=0;i<MAX_CLIENTS;i++) {
					//if position is empty
					if(socket_clients[i] == 0) {
						socket_clients[i] = clientSocket;
						if(socket_callback->client_connected_callback)
							socket_callback->client_connected_callback(i);
						logprintf(LOG_DEBUG, "client id: %d", i);
						break;
					}
				}
			}
        }

        //else its some IO operation on some other socket :)
        for(i=0;i<MAX_CLIENTS;i++) {
			sd = socket_clients[i];

            if(FD_ISSET((long unsigned int)sd , &readfds)) {
                //Check if it was for closing, and also read the incoming message

				memset(readBuff, '\0', BUFFER_SIZE);				
                if((n = (int)read(sd, readBuff, BUFFER_SIZE-1)) == 0) {
                    //Somebody disconnected, get his details and print
                    getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
					logprintf(LOG_INFO, "client disconnected, ip %s, port %d", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
					if(socket_callback->client_disconnected_callback)
						socket_callback->client_disconnected_callback(i);
					//Close the socket and mark as 0 in list for reuse
					close(sd);
					socket_clients[i] = 0;
                } else {
                    //set the string terminating NULL byte on the end of the data read
                    // readBuff[n] = '\0';

					if(n > -1 && socket_callback->client_data_callback) {
						pch = strtok(readBuff, "\n");
						while(pch) {
							strcat(pch, "\n");
							socket_callback->client_data_callback(i, pch);
							pch = strtok(NULL, "\n");
						}
					}
                }
            }
        }
    }
	return NULL;
}
