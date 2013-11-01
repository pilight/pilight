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

#include "../../pilight.h"
#include "common.h"
#include "../websockets/libwebsockets.h"
#include "../websockets/private-libwebsockets.h" 
#include "config.h"
#include "gc.h"
#include "log.h"
#include "threads.h"
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

// char *request = NULL;
char *ext = NULL;
// char *mimetype = NULL;
char *server;

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

struct per_session_data__http {
    int fd;
	unsigned char *stream;
	char *mimetype;
	char *request;
};

int webserver_gc(void) {
	webserver_loop = 0;
	socket_close(sockfd);
	sfree((void *)&syncBuff);
	sfree((void *)&sockWriteBuff);
	sfree((void *)&sockReadBuff);
	sfree((void *)&ext);
	// sfree((void *)&request);
	// sfree((void *)&mimetype);
	fcache_gc();
	logprintf(LOG_DEBUG, "garbage collected webserver library");
	return 1;
}

int webserver_callback_http(struct libwebsocket_context *webcontext, struct libwebsocket *wsi, enum libwebsocket_callback_reasons reason, void *user, void *in, size_t len) {
	int n = 0, m = 0;
	int size;
	struct stat stat_buf;
	struct per_session_data__http *pss = (struct per_session_data__http *)user;

	switch(reason) {
		case LWS_CALLBACK_HTTP: {
			/* If the webserver didn't request a file, serve the index.html */
			if(strcmp((const char *)in, "/") == 0) {
				pss->request = realloc(pss->request, strlen(webserver_root)+13);
				memset(pss->request, '\0', strlen(webserver_root)+13);
				/* Check if the webserver_root is terminated by a slash. If not, than add it */
				if(webserver_root[strlen(webserver_root)-1] == '/') {
					sprintf(pss->request, "%s%s", webserver_root, "index.html");
				} else {
					sprintf(pss->request, "%s%s", webserver_root, "/index.html");
				}
			} else {
				char *cin = (char *)in;
				/* If a file was requested add it to the webserver path to create the absolute path */
				if(webserver_root[strlen(webserver_root)-1] == '/' && cin[0] == '/') {
					pss->request = realloc(pss->request, strlen(webserver_root)+strlen((const char *)in));
					memset(pss->request, '\0', strlen(webserver_root)+strlen((const char *)in));
					strncpy(&pss->request[0], webserver_root, strlen(webserver_root)-1);
					strncpy(&pss->request[strlen(webserver_root)-1], cin, strlen(cin));
				} else {
					pss->request = realloc(pss->request, strlen(webserver_root)+strlen((const char *)in)+1);
					memset(pss->request, '\0', strlen(webserver_root)+strlen((const char *)in)+1);
					sprintf(pss->request, "%s%s", webserver_root, (const char *)in);
				}
			}

			char *dot = NULL;
			/* Retrieve the extension of the requested file and create a mimetype accordingly */
			dot = strrchr(pss->request, '.');
			if(!dot || dot == pss->request)
				return -1;

			ext = realloc(ext, strlen(dot)+1);
			memset(ext, '\0', strlen(dot)+1);
			strcpy(ext, dot+1);

			if(strcmp(ext, "html") == 0) {
				pss->mimetype = realloc(pss->mimetype, 10);
				memset(pss->mimetype, '\0', 10);
				strcpy(pss->mimetype, "text/html");
			} else if(strcmp(ext, "png") == 0) {
				pss->mimetype = realloc(pss->mimetype, 10);
				memset(pss->mimetype, '\0', 10);
				strcpy(pss->mimetype, "image/png");
			} else if(strcmp(ext, "ico") == 0) {
				pss->mimetype = realloc(pss->mimetype, 13);
				memset(pss->mimetype, '\0', 13);
				strcpy(pss->mimetype, "image/x-icon");
			} else if(strcmp(ext, "css") == 0) {
				pss->mimetype = realloc(pss->mimetype, 10);
				memset(pss->mimetype, '\0', 10);
				strcpy(pss->mimetype, "text/css");
			} else if(strcmp(ext, "js") == 0) {
				pss->mimetype = realloc(pss->mimetype, 16);
				memset(pss->mimetype, '\0', 16);
				strcpy(pss->mimetype, "text/javascript");
			}
			
			unsigned char *p = webcontext->service_buffer;

			p += sprintf((char *)p,
				"HTTP/1.0 200 OK\x0d\x0a"
				"Server: pilight\x0d\x0a"
				"Content-Type: %s\x0d\x0a",
				pss->mimetype);

			if(webserver_cache) {
				/* If webserver caching is enabled, first load all files in the memory */
				if(fcache_get_size(pss->request, &size) != 0) {
					if((fcache_add(pss->request)) != 0) {
						logprintf(LOG_NOTICE, "(webserver) could not cache %s", pss->request);
					}
				}

				/* Check if a file was succesfully stored in memory */
				if(fcache_get_size(pss->request, &size) == 0) {
					p += sprintf((char *)p,
						"Content-Length: %u\x0d\x0a\x0d\x0a",
						(unsigned int)size);				
				
					libwebsocket_write(wsi, webcontext->service_buffer, (size_t)(p-webcontext->service_buffer), LWS_WRITE_HTTP);
					pss->fd = -1;
					if(size <= sizeof(webcontext->service_buffer)) {
						wsi->u.http.fd = -1;
						wsi->u.http.stream = fcache_get_bytes(pss->request);
						wsi->u.http.filelen = (size_t)size;	
						wsi->u.http.filepos = 0;
						wsi->u.http.choke = 1;
						wsi->state = WSI_STATE_HTTP_ISSUING_FILE;

						libwebsockets_serve_http_file_fragment(webcontext, wsi);
						libwebsocket_callback_on_writable(webcontext, wsi);
						return 0;
					} else {
						libwebsocket_callback_on_writable(webcontext, wsi);
						break;
					}
				}
			} else {
				pss->fd = open(pss->request, O_RDONLY);
				if(pss->fd < 0)
                   return -1;
				fstat(pss->fd, &stat_buf);

				p += sprintf((char *)p,
					"Content-Length: %u\x0d\x0a\x0d\x0a",
					(unsigned int)stat_buf.st_size);

				libwebsocket_write(wsi, webcontext->service_buffer, (size_t)(p-webcontext->service_buffer), LWS_WRITE_HTTP);
				if(stat_buf.st_size <= sizeof(webcontext->service_buffer)) {
					wsi->u.http.fd = pss->fd;
					wsi->u.http.stream = NULL;
					wsi->u.http.filelen = (size_t)stat_buf.st_size;	
					wsi->u.http.filepos = 0;
					wsi->u.http.choke = 1;
					wsi->state = WSI_STATE_HTTP_ISSUING_FILE;
				
					libwebsockets_serve_http_file_fragment(webcontext, wsi);
					libwebsocket_callback_on_writable(webcontext, wsi);
					return 0;
				} else {
					libwebsocket_callback_on_writable(webcontext, wsi);
					break;
				}
			}
		}
		break;
		case LWS_CALLBACK_HTTP_FILE_COMPLETION:
			return -1;
		case LWS_CALLBACK_HTTP_WRITEABLE:
			if(pss->fd > -1) {
				do {
					n = read(pss->fd, webcontext->service_buffer, sizeof(webcontext->service_buffer));
					if(n <= 0) {
						close(pss->fd);
						return -1;
					}
					m = libwebsocket_write(wsi, webcontext->service_buffer, (size_t)n, LWS_WRITE_HTTP);
					if(m < 0) {
						close(pss->fd);
						return -1;
					}
					if(m != n) {
						lseek(pss->fd, m - n, SEEK_CUR);
					}
				} while (!lws_send_pipe_choked(wsi));
				libwebsocket_callback_on_writable(webcontext, wsi);
			} else {
				if(fcache_get_size(pss->request, &size) == 0) {
					wsi->u.http.filepos = 0;
					wsi->u.http.choke = 0;
					wsi->state = WSI_STATE_HTTP_ISSUING_FILE;				
					wsi->u.http.fd = -1;
					wsi->u.http.stream = fcache_get_bytes(pss->request);
					wsi->u.http.filelen = (size_t)size;	
				
					libwebsockets_serve_http_file_fragment(webcontext, wsi);
				}
			}
		break;			
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
							sfree((void *)&output);
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
			size_t n = strlen(syncBuff);

			/* This PRE_PADDIGN and POST_PADDING is an requirement for LWS_WRITE_TEXT */
			sockWriteBuff = realloc(sockWriteBuff, LWS_SEND_BUFFER_PRE_PADDING + n + LWS_SEND_BUFFER_POST_PADDING);
			memset(sockWriteBuff, '\0', sizeof(sockWriteBuff));
			memcpy(&sockWriteBuff[LWS_SEND_BUFFER_PRE_PADDING], syncBuff, n);
			m = libwebsocket_write(wsi, &sockWriteBuff[LWS_SEND_BUFFER_PRE_PADDING], n, LWS_WRITE_TEXT);
			/*
			 * It seems like libwebsocket_write already does memory freeing
			 */
			if (m != n+4) {
				logprintf(LOG_ERR, "(webserver) %d writing to socket", n);
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
	sfree((void *)&server);
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
	sfree((void *)&sockReadBuff);
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
		/* Register a seperate thread in which the webserver communicates
		   the main daemon as if it where a gui */
		threads_register(&webserver_clientize, (void *)NULL);
		/* Main webserver loop */
		while(n >= 0 && webserver_loop) {
			n = libwebsocket_service(context, 50);
		}
		/* If the main loop stops, stop and destroy the webserver */
		libwebsocket_context_destroy(context);
		sfree((void *)&syncBuff);
	}
	return 0;
}