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
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#define MSG_NOSIGNAL 0
#else
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <sys/un.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <netdb.h>
	#include <arpa/inet.h>
#endif
#include <stdint.h>
#include <math.h>

#include "../../core/threads.h"
#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/socket.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/json.h"
#include "../../core/gc.h"
#include "lirc.h"

#ifndef _WIN32
static char lirc_socket[BUFFER_SIZE];
static int lirc_sockfd = -1;

static unsigned short lirc_loop = 1;
static unsigned short lirc_threads = 0;
static unsigned short lirc_init = 0;


// Windows
// #define UNIX_PATH_MAX 108

// struct sockaddr_un {
	// uint16_t sun_family;
	// char sun_path[UNIX_PATH_MAX];
// };

static void *lircParse(void *param) {
	struct protocol_threads_t *node = (struct protocol_threads_t *)param;
	struct sockaddr_un addr;
	struct timeval timeout;

	int nrloops = 0, n = -1, bytes = 0;
	char recvBuff[BUFFER_SIZE];
	fd_set fdsread;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	/* Clear the server address */
	memset(&addr, '\0', sizeof(addr));
	memset(&recvBuff, '\0', BUFFER_SIZE);

	lirc_threads++;

	while(lirc_loop) {
		if(lirc_sockfd > -1) {
			close(lirc_sockfd);
			lirc_sockfd = -1;
		}

		if(path_exists(lirc_socket) == EXIT_SUCCESS) {
			/* Try to open a new socket */
			if((lirc_sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
				logprintf(LOG_DEBUG, "could not create Lirc socket");
				break;
			}

			addr.sun_family=AF_UNIX;
			strcpy(addr.sun_path, lirc_socket);

			/* Connect to the server */
			switch(socket_timeout_connect(lirc_sockfd, (struct sockaddr *)&addr, 3)) {
				case -1:
					logprintf(LOG_ERR, "could not connect to Lirc socket @%s", lirc_socket);
					protocol_thread_wait(node, 3, &nrloops);
					continue;
				break;
				case -2:
					logprintf(LOG_ERR, "Lirc socket timeout @%s", lirc_socket);
					protocol_thread_wait(node, 3, &nrloops);
					continue;
				break;
				case -3:
					logprintf(LOG_ERR, "Error in Lirc socket connection @%s", lirc_socket);
					protocol_thread_wait(node, 3, &nrloops);
					continue;
				break;
				default:
				break;
			}

			while(lirc_loop) {
				FD_ZERO(&fdsread);
				FD_SET((unsigned long)lirc_sockfd, &fdsread);

				do {
					n = select(lirc_sockfd+1, &fdsread, NULL, NULL, &timeout);
				} while(n == -1 && errno == EINTR && lirc_loop);

				if(lirc_loop == 0) {
					break;
				}

				if(path_exists(lirc_socket) == EXIT_FAILURE) {
					break;
				}

				if(n == -1) {
					break;
				} else if(n == 0) {
					usleep(100000);
				} else if(n > 0) {
					if(FD_ISSET((unsigned long)lirc_sockfd, &fdsread)) {
						bytes = (int)recv(lirc_sockfd, recvBuff, BUFFER_SIZE, 0);
						if(bytes <= 0) {
							break;
						} else {
							char **nlarray = NULL;
							unsigned int e = explode(recvBuff, "\n", &nlarray), t = 0;
							for(t=0;t<e;t++) {
								int x = 0, nrspace = 0;
								for(x=0;x<strlen(nlarray[t]);x++) {
									if(nlarray[t][x] == ' ') {
										nrspace++;
									}
								}
								if(nrspace >= 3) {
									char **array = NULL;
									unsigned int q = explode(nlarray[t], " ", &array), h = 0;
									if(q == 4) {
										char *code1 = array[0];
										char *rep = array[1];
										char *btn = array[2];
										char *remote = array[3];
										char *y = NULL;
										if((y = strstr(remote, "\n")) != NULL) {
											size_t pos = (size_t)(y-remote);
											remote[pos] = '\0';
										}
										int r = strtol(rep, NULL, 16);
										lirc->message = json_mkobject();
										JsonNode *code = json_mkobject();
										json_append_member(code, "code", json_mkstring(code1));
										json_append_member(code, "repeat", json_mknumber(r, 0));
										json_append_member(code, "button", json_mkstring(btn));
										json_append_member(code, "remote", json_mkstring(remote));

										json_append_member(lirc->message, "message", code);
										json_append_member(lirc->message, "origin", json_mkstring("receiver"));
										json_append_member(lirc->message, "protocol", json_mkstring(lirc->id));

										if(pilight.broadcast != NULL) {
											pilight.broadcast(lirc->id, lirc->message, PROTOCOL);
										}
										json_delete(lirc->message);
										lirc->message = NULL;
									}
									if(q > 0) {
										for(h=0;h<q;h++) {
											FREE(array[h]);
										}
										FREE(array);
									}
								}
							}
							if(t > 0) {
								for(t=0;t<e;t++) {
									FREE(nlarray[t]);
								}
								FREE(nlarray);
							}
							memset(recvBuff, '\0', BUFFER_SIZE);
						}
					}
				}
			}
		} else {
			sleep(1);
		}
	}

	if(lirc_sockfd > -1) {
		close(lirc_sockfd);
		lirc_sockfd = -1;
	}

	lirc_threads--;
	return (void *)NULL;
}

struct threadqueue_t *lircInitDev(JsonNode *jdevice) {
	lirc_loop = 1;

	if(lirc_init == 0) {
		lirc_init = 1;
		struct protocol_threads_t *node = protocol_thread_init(lirc, NULL);
		return threads_register("lirc", &lircParse, (void *)node, 0);
	} else {
		return NULL;
	}
}

static void lircThreadGC(void) {
	lirc_loop = 0;
	protocol_thread_stop(lirc);
	while(lirc_threads > 0) {
		usleep(10);
	}
	protocol_thread_free(lirc);
	lirc_init = 0;
}
#endif

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void lircInit(void) {
	protocol_register(&lirc);
	protocol_set_id(lirc, "lirc");
	protocol_device_add(lirc, "lirc", "Lirc API");
	lirc->devtype = LIRC;
	lirc->hwtype = API;

	options_add(&lirc->options, 'c', "code", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&lirc->options, 'a', "repeat", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "[0-9]");
	options_add(&lirc->options, 'b', "button", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&lirc->options, 'r', "remote", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, NULL);

#ifndef _WIN32
	lirc->initDev=&lircInitDev;
	lirc->threadGC=&lircThreadGC;

	memset(lirc_socket, '\0', BUFFER_SIZE);
	strcpy(lirc_socket, "/dev/lircd");
#endif
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "lirc";
	module->version = "1.8";
	module->reqversion = "6.0";
	module->reqcommit = "58";
}

void init(void) {
	lircInit();
}
#endif
