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
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <syslog.h>
#include <signal.h>
#include <pthread.h>

#include "libwebsockets.h"
#include "gc.h"
#include "config.h"
#include "json.h"
#include "socket.h"
#include "webserver.h"

struct libwebsocket_context *context;

struct per_session_data__http {
	int fd;
};

int webserver_port = WEBSERVER_PORT;
char *webserver_root;
int sockfd = 0;
char *recvBuff = NULL;

typedef enum {
	WELCOME,
	IDENTIFY,
	REJECT,
	SYNC
} steps_t; 

struct libwebsocket_protocols libwebsocket_protocols[] = {
	{ "http-only", webserver_callback_http, 102400, 102400 },
	{ NULL, NULL, 0, 0 }
};

int webserver_gc(void) {
	libwebsocket_context_destroy(context);
	socket_close(sockfd);
	return 1;
}

int webserver_callback_http(struct libwebsocket_context *webcontext, struct libwebsocket *wsi, enum libwebsocket_callback_reasons reason, void *user, void *in, size_t len) {
	char request[255];
	char *dot = NULL;
	char *ext = NULL;
	char mimetype[255];
	char *message = NULL;
	int /*f = 0,*/ n = 0, m = 0;
	struct stat sb; 
	/* static unsigned char buffer[4096];
	unsigned char *p; */
	struct per_session_data__http *pss = (struct per_session_data__http *)user;
	
	switch(reason) {
		case LWS_CALLBACK_HTTP:
			if(strcmp((const char *)in, "/") == 0) {
				sprintf(request, "%s%s", webserver_root, "/index.html");
			} else {
				sprintf(request, "%s%s", webserver_root, (const char *)in);
			}

			//p = buffer;
			if((pss->fd = open(request, O_RDONLY)) == -1) {
				logprintf(LOG_ERR, "(webserver) file not found %s", request);
				return -1;
			}
			
			dot = strrchr(request, '.');
			if(!dot || dot == request) 
				return -1;
			ext = strdup(dot+1);

			if(strcmp(ext, "html") == 0) {
				strcpy(mimetype, "text/html");
			} else if(strcmp(ext, "png") == 0) {
				strcpy(mimetype, "image/png");
			} else if(strcmp(ext, "ico") == 0) {
				strcpy(mimetype, "image/x-icon");
			} else if(strcmp(ext, "css") == 0) {
				strcpy(mimetype, "text/css");
			} else if(strcmp(ext, "js") == 0) {
				strcpy(mimetype, "text/javascript");
			}			
			
			fstat(pss->fd, &sb);
			
			// if((unsigned int)sb.st_size > 16720) {

				// p += sprintf((char *)p,
					// "HTTP/1.0 200 OK\x0d\x0a"
					// "Server: libwebsockets\x0d\x0a"
					// "Content-Type: %s\x0d\x0a"
					// "Content-Length: %u\x0d\x0a\x0d\x0a",
					// mimetype, (unsigned int)sb.st_size);

				// n = libwebsocket_write(wsi, buffer, p - buffer, LWS_WRITE_HTTP);

				// if (n < 0) {
					// close(pss->fd);
					// return -1;
				// }

				// libwebsocket_callback_on_writable(webcontext, wsi);
				// break;
			// } else {
				if(libwebsockets_serve_http_file(webcontext, wsi, request, mimetype)) {
					return -1;
				libwebsocket_callback_on_writable(webcontext, wsi);
				// }
			}
			break;
		case LWS_CALLBACK_HTTP_FILE_COMPLETION:
			return -1;
		case LWS_CALLBACK_ESTABLISHED:
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		case LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH:
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
		case LWS_CALLBACK_CLOSED:
		case LWS_CALLBACK_CLOSED_HTTP:
		case LWS_CALLBACK_RECEIVE:
			if((int)len < 4) {	
				return -1;
			} else {
				if(json_validate((const char *)in) == true) {
					JsonNode *json = json_decode((const char *)in);
					if(json_find_string(json, "message", &message) != -1) {
						if(strcmp(message, "request config") == 0) {
							json = json_mkobject();
							json_append_member(json, "config", config2json());
							message = json_stringify(json, NULL);
							libwebsocket_write(wsi, (unsigned char *)message, strlen(message), LWS_WRITE_TEXT);
						} else if(strcmp(message, "send") == 0) {
							/* Write all codes coming from the webserver to the daemon */
							socket_write(sockfd, (const char *)in);
						}
					}
				}
			}
		break;
		case LWS_CALLBACK_CLIENT_RECEIVE:
		case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
		case LWS_CALLBACK_CLIENT_WRITEABLE:
		case LWS_CALLBACK_SERVER_WRITEABLE:
			/* Push the incoming message to the webgui */
			m = libwebsocket_write(wsi, (unsigned char *)recvBuff, strlen(recvBuff), LWS_WRITE_TEXT);
			if (m < n) {
				logprintf(LOG_ERR, "(webserver) %d writing to di socket", n);
				return -1;
			}
		break;
		case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
		case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS:
		case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS:
		case LWS_CALLBACK_OPENSSL_PERFORM_CLIENT_CERT_VERIFICATION:
		case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
		case LWS_CALLBACK_CONFIRM_EXTENSION_OKAY:
		case LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED:
		case LWS_CALLBACK_PROTOCOL_INIT:
		case LWS_CALLBACK_PROTOCOL_DESTROY:
		case LWS_CALLBACK_ADD_POLL_FD:
		case LWS_CALLBACK_DEL_POLL_FD:
		case LWS_CALLBACK_SET_MODE_POLL_FD:
		case LWS_CALLBACK_CLEAR_MODE_POLL_FD:
		case LWS_CALLBACK_HTTP_WRITEABLE:
			// if((int)user > 0 && pss->fd > 0) {
				// do {
					// printf("%d\n", n);
					// n = read(pss->fd, buffer, sizeof buffer);
					// /* problem reading, close conn */
					// if (n <= 0) {
						// close(pss->fd);
						// return -1;
					// }

					// m = libwebsocket_write(wsi, buffer, n, LWS_WRITE_HTTP);
					// if(m < 0) {
						// close(pss->fd);
						// return -1;
					// }
					// if(m != n) {
						// lseek(pss->fd, m - n, SEEK_CUR);
					// }

				// } while(!lws_send_pipe_choked(wsi));
				// libwebsocket_callback_on_writable(webcontext, wsi);
			// }
			break;	
		case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
		default:
		break;
	}
	return 0;
}

void *webserver_clientize(void *param) {
	steps_t steps = WELCOME;
	char *message = NULL;
	JsonNode *json = json_mkobject();
	int port = 0;

	settings_find_number("port", &port);

	if((sockfd = socket_connect(strdup("localhost"), (short unsigned int)port)) == -1) {
		logprintf(LOG_ERR, "could not connect to pilight-daemon");
		exit(EXIT_FAILURE);
	}	

	while(1) {
		if(steps > WELCOME) {
			/* Clear the receive buffer again and read the welcome message */
			if((recvBuff = socket_read(sockfd)) != NULL) {
				json = json_decode(recvBuff);
				json_find_string(json, "message", &message);
			} else {
				goto close;
			}
		} 	
		switch(steps) {
			case WELCOME:
				socket_write(sockfd, "{\"message\":\"client gui\"}");
				steps=IDENTIFY;
			break;
			case IDENTIFY:
				if(strcmp(message, "accept client") == 0) {
					steps=SYNC;
				}
				if(strcmp(message, "reject client") == 0) {
					steps=REJECT;
				}
			break;
			case SYNC:
				/* Push all incoming sync messages to the web gui */
				libwebsocket_callback_on_writable_all_protocol(&libwebsocket_protocols[0]);
			break;				
			case REJECT:
			default:
				goto close;
			break;
		}
	}
close:
	json_delete(json);
	socket_close(sockfd);	
	return 0;
}

void *webserver_start(void *param) {

	gc_attach(webserver_gc);

	struct lws_context_creation_info info;
	pthread_t pth1;
	
	settings_find_number("webserver-port", &webserver_port);
	if(settings_find_string("webserver-root", &webserver_root) != 0) {
		webserver_root = strdup(WEBSERVER_ROOT);
	}
	
	memset(&info, 0, sizeof info);
	info.port = webserver_port;

	info.protocols = libwebsocket_protocols;
	info.ssl_cert_filepath = NULL;
	info.ssl_private_key_filepath = NULL;
	info.gid = -1;
	info.uid = -1;
	
	context = libwebsocket_create_context(&info);
	if(context == NULL) {
		lwsl_err("libwebsocket init failed\n");
	} else {
		/* Create a seperate thread in which the webserver communicates
		   the main daemon as if it where a gui */
		pthread_create(&pth1, NULL, &webserver_clientize, (void *)NULL);
		/* Main webserver loop */
		while(1) {
			libwebsocket_service(context, 50);
		}
		pthread_join(pth1, NULL);
	}
	return 0;
}