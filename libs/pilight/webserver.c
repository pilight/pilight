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

#include "../websockets/libwebsockets.h"
#include "gc.h"
#include "config.h"
#include "log.h"
#include "json.h"
#include "socket.h"
#include "webserver.h"
#include "fcache.h"

int webserver_port = WEBSERVER_PORT;
int webserver_cache = 1;
unsigned short webserver_loop = 1;
char *webserver_root;

int sockfd = 0;
char *sockReadBuff = NULL;
char *syncBuff = NULL;
unsigned char *sockWriteBuff = NULL;

char *request = NULL;
char *ext = NULL;
char *mimetype = NULL;
char *server;

pthread_t pth1;

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
	webserver_loop = 0;
	socket_close(sockfd);
	if(syncBuff) {
		free(syncBuff);
		syncBuff = NULL;
	}
	if(sockWriteBuff) {
		free(sockWriteBuff);
		sockWriteBuff = NULL;
	}
	if(sockReadBuff) {
		free(sockReadBuff);
		sockReadBuff = NULL;
	}
	if(ext) {
		free(ext);
		ext = NULL;
	}
	if(request) {
		free(request);
		request = NULL;
	}
	if(mimetype) {
		free(mimetype);
		mimetype = NULL;
	}
	pthread_cancel(pth1);
	pthread_join(pth1, NULL);
	fcache_gc();
	logprintf(LOG_DEBUG, "garbage collected webserver library");
	return 1;
}

int webserver_callback_http(struct libwebsocket_context *webcontext, struct libwebsocket *wsi, enum libwebsocket_callback_reasons reason, void *user, void *in, size_t len) {
	int n = 0, m = 0;
	int size;

	switch(reason) {
		case LWS_CALLBACK_HTTP: {
			/* If the webserver didn't request a file, serve the index.html */
			if(strcmp((const char *)in, "/") == 0) {
				request = realloc(request, strlen(webserver_root)+13);
				memset(request, '\0', strlen(webserver_root)+13);
				/* Check if the webserver_root is terminated by a slash. If not, than add it */
				if(webserver_root[strlen(webserver_root)-1] == '/') {
					sprintf(request, "%s%s", webserver_root, "index.html");
				} else {
					sprintf(request, "%s%s", webserver_root, "/index.html");
				}
			} else {
				char *cin = (char *)in;
				/* If a file was requested add it to the webserver path to create the absolute path */
				if(webserver_root[strlen(webserver_root)-1] == '/' && cin[0] == '/') {
					request = realloc(request, strlen(webserver_root)+strlen((const char *)in));
					memset(request, '\0', strlen(webserver_root)+strlen((const char *)in));
					strncpy(&request[0], webserver_root, strlen(webserver_root)-1);
					strncpy(&request[strlen(webserver_root)-1], cin, strlen(cin));
				} else {
					request = realloc(request, strlen(webserver_root)+strlen((const char *)in)+1);
					memset(request, '\0', strlen(webserver_root)+strlen((const char *)in)+1);
					sprintf(request, "%s%s", webserver_root, (const char *)in);
				}
			}

			char *dot = NULL;
			/* Retrieve the extension of the requested file and create a mimetype accordingly */
			dot = strrchr(request, '.');
			if(!dot || dot == request)
				return -1;

			ext = realloc(ext, strlen(dot)+1);
			memset(ext, '\0', strlen(dot)+1);
			strcpy(ext, dot+1);

			if(strcmp(ext, "html") == 0) {
				mimetype = realloc(mimetype, 10);
				memset(mimetype, '\0', 10);
				strcpy(mimetype, "text/html");
			} else if(strcmp(ext, "png") == 0) {
				mimetype = realloc(mimetype, 10);
				memset(mimetype, '\0', 10);
				strcpy(mimetype, "image/png");
			} else if(strcmp(ext, "ico") == 0) {
				mimetype = realloc(mimetype, 13);
				memset(mimetype, '\0', 13);
				strcpy(mimetype, "image/x-icon");
			} else if(strcmp(ext, "css") == 0) {
				mimetype = realloc(mimetype, 10);
				memset(mimetype, '\0', 10);
				strcpy(mimetype, "text/css");
			} else if(strcmp(ext, "js") == 0) {
				mimetype = realloc(mimetype, 16);
				memset(mimetype, '\0', 16);
				strcpy(mimetype, "text/javascript");
			}

			if(webserver_cache) {
				/* If webserver caching is enabled, first load all files in the memory */
				if(fcache_get_size(request, &size) != 0) {
					if((fcache_add(request)) != 0) {
						logprintf(LOG_ERR, "(webserver) could not cache %s", request);
					}
				}

				/* Check if a file was succesfully stored in memory */
				if(fcache_get_size(request, &size) == 0) {
					/* Combine the header and the file content itself and server it */
					unsigned char *header = malloc(74+strlen(mimetype)+sizeof((unsigned int)size)+1);
					sprintf((char *)header,
						"HTTP/1.0 200 OK\x0d\x0a"
						"Server: pilight\x0d\x0a"
						"Content-Type: %s\x0d\x0a"
						"Content-Length: %u\x0d\x0a\x0d\x0a",
						mimetype, (unsigned int)size);
					unsigned char *content = malloc(strlen((char *)header)+(size_t)size+1);
					memset(content, '\0', strlen((char *)header)+(size_t)size+1);
					strcat((char *)content, (char *)header);
					strcat((char *)content, (char *)fcache_get_bytes(request));

					libwebsocket_write(wsi, content, strlen((char *)content), LWS_WRITE_HTTP);
					free(header);
					free(content);
				}
			} else {
				/* Read a file from the FS and server it */
				if(libwebsockets_serve_http_file(webcontext, wsi, request, mimetype)) {
					libwebsocket_callback_on_writable(webcontext, wsi);
					return -1;
				}
			}
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
				if(json_validate((char *)in) == true) {
					JsonNode *json = json_decode((char *)in);
					char *message = NULL;

					if(json_find_string(json, "message", &message) != -1) {
						if(strcmp(message, "request config") == 0) {

							JsonNode *jsend = json_mkobject();
							JsonNode *jconfig = config2json(1);
							json_append_member(jsend, "config", jconfig);

							char *output = json_stringify(jsend, NULL);
							size_t output_len = strlen(output);
							/* This PRE_PADDIGN and POST_PADDING is an requirement for LWS_WRITE_TEXT */
							sockWriteBuff = realloc(sockWriteBuff, LWS_SEND_BUFFER_PRE_PADDING + output_len + LWS_SEND_BUFFER_POST_PADDING);
							memset(sockWriteBuff, '\0', sizeof(sockWriteBuff));
 	  						memcpy(&sockWriteBuff[LWS_SEND_BUFFER_PRE_PADDING], output, output_len);
							libwebsocket_write(wsi, &sockWriteBuff[LWS_SEND_BUFFER_PRE_PADDING], output_len, LWS_WRITE_TEXT);
							free(output);
							json_delete(jsend);
						} else if(strcmp(message, "send") == 0) {
							/* Write all codes coming from the webserver to the daemon */
							socket_write(sockfd, (char *)in);
						}
					}
					json_delete(json);
				}
			}
		break;
		case LWS_CALLBACK_CLIENT_RECEIVE:
		case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
		case LWS_CALLBACK_CLIENT_WRITEABLE:
		case LWS_CALLBACK_SERVER_WRITEABLE: {
			/* Push the incoming message to the webgui */
			size_t syncBuff_len = strlen(syncBuff);

			/* This PRE_PADDIGN and POST_PADDING is an requirement for LWS_WRITE_TEXT */
			sockWriteBuff = realloc(sockWriteBuff, LWS_SEND_BUFFER_PRE_PADDING + syncBuff_len + LWS_SEND_BUFFER_POST_PADDING);
			memset(sockWriteBuff, '\0', sizeof(sockWriteBuff));
			memcpy(&sockWriteBuff[LWS_SEND_BUFFER_PRE_PADDING], syncBuff, syncBuff_len);
			m = libwebsocket_write(wsi, &sockWriteBuff[LWS_SEND_BUFFER_PRE_PADDING], syncBuff_len, LWS_WRITE_TEXT);
			/*
			 * It seems like libwebsocket_write already does memory freeing
			 */
			if (m < n) {
				logprintf(LOG_ERR, "(webserver) %d writing to di socket", n);
				return -1;
			}
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
		case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
		default:
		break;
	}
	return 0;
}

void *webserver_clientize(void *param) {
	steps_t steps = WELCOME;
	char *message = NULL;
	server = malloc(10);
	strcpy(server, "localhost");
	int port = 0;

	settings_find_number("port", &port);

	if((sockfd = socket_connect(server, (short unsigned int)port)) == -1) {
		logprintf(LOG_ERR, "could not connect to pilight-daemon");
		exit(EXIT_FAILURE);
	}
	sockReadBuff = malloc(BUFFER_SIZE);
	free(server);
	while(webserver_loop) {
		if(steps > WELCOME) {
			memset(sockReadBuff, '\0', BUFFER_SIZE);
			/* Clear the receive buffer again and read the welcome message */
			/* Used direct read access instead of socket_read */
			if(read(sockfd, sockReadBuff, BUFFER_SIZE) < 1) {
				goto close;
			}
		}
		switch(steps) {
			case WELCOME:
				socket_write(sockfd, "{\"message\":\"client gui\"}");
				steps=IDENTIFY;
			break;
			case IDENTIFY: {
				JsonNode *json = json_decode(sockReadBuff);
				json_find_string(json, "message", &message);
				if(strcmp(message, "accept client") == 0) {
					steps=SYNC;
				}
				if(strcmp(message, "reject client") == 0) {
					steps=REJECT;
				}
				json_delete(json);
			}
			break;
			case SYNC:
				if(strlen(sockReadBuff) > 0) {
					syncBuff = realloc(syncBuff, strlen(sockReadBuff)+1);
					memset(syncBuff, '\0', sizeof(syncBuff));
					strcpy(syncBuff, sockReadBuff);
					/* Push all incoming sync messages to the web gui */
					libwebsocket_callback_on_writable_all_protocol(&libwebsocket_protocols[0]);
				}
			break;
			case REJECT:
			default:
				goto close;
			break;
		}
	}
	free(sockReadBuff);
	sockReadBuff = NULL;
close:
	webserver_gc();
	return 0;
}

void *webserver_start(void *param) {

	int n = 0;
	struct lws_context_creation_info info;

	/* Check on what port the webserver needs to run */
	settings_find_number("webserver-port", &webserver_port);
	if(settings_find_string("webserver-root", &webserver_root) != 0) {
		/* If no webserver port was set, use the default webserver port */
		webserver_root = malloc(strlen(WEBSERVER_ROOT)+1);
		strcpy(webserver_root, WEBSERVER_ROOT);
	}

	/* Do we turn on webserver caching. This means that all requested files are
	   loaded into the memory so they aren't read from the FS anymore */
	settings_find_number("webserver-cache", &webserver_cache);

	/* Default websockets info */
	memset(&info, 0, sizeof info);
	info.port = webserver_port;
	info.protocols = libwebsocket_protocols;
	info.ssl_cert_filepath = NULL;
	info.ssl_private_key_filepath = NULL;
	info.gid = -1;
	info.uid = -1;

	/* Start the libwebsocket server */
	struct libwebsocket_context *context = libwebsocket_create_context(&info);
	if(context == NULL) {
		lwsl_err("libwebsocket init failed\n");
	} else {
		/* Create a seperate thread in which the webserver communicates
		   the main daemon as if it where a gui */
		pthread_create(&pth1, NULL, &webserver_clientize, (void *)NULL);
		/* Main webserver loop */
		while(n >= 0 && webserver_loop) {
			n = libwebsocket_service(context, 50);
		}
		/* If the main loop stops, stop and destroy the webserver */
		libwebsocket_context_destroy(context);
		if(syncBuff) {
			free(syncBuff);
			syncBuff = NULL;
		}
	}
	return 0;
}