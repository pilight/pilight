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
#define __USE_GNU
#include <pthread.h>

#include "../../pilight.h"
#include "common.h"
#include "mongoose.h"
#include "config.h"
#include "gc.h"
#include "log.h"
#include "threads.h"
#include "json.h"
#include "socket.h"
#include "webserver.h"
#include "settings.h"
#include "ssdp.h"
#include "fcache.h"

int webserver_port = WEBSERVER_PORT;
int webserver_cache = 1;
int webserver_authentication = 0;
char *webserver_username = NULL;
char *webserver_password = NULL;
unsigned short webserver_loop = 1;
char *webserver_root = NULL;
char *webgui_tpl = NULL;
unsigned char alphabet[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
struct mg_server *mgserver = NULL;

int sockfd = 0;
char *sockReadBuff = NULL;
unsigned char *sockWriteBuff = NULL;

char *server = NULL;

typedef enum {
	WELCOME,
	IDENTIFY,
	REJECT,
	SYNC
} steps_t;

typedef struct webqueue_t {
	char *message;
	struct webqueue_t *next;
} webqueue_t;

struct webqueue_t *webqueue;
struct webqueue_t *webqueue_head;

#ifndef __FreeBSD__
pthread_mutex_t webqueue_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
pthread_cond_t webqueue_signal = PTHREAD_COND_INITIALIZER;
#else
pthread_mutex_t webqueue_lock;
pthread_cond_t webqueue_signal;
#endif

int webqueue_number = 0;

int webserver_gc(void) {
	webserver_loop = 0;
	socket_close(sockfd);
	sfree((void *)&sockWriteBuff);
	sfree((void *)&sockReadBuff);
	if(webserver_root) {
		sfree((void *)&webserver_root);
	}
	if(webgui_tpl) {
		sfree((void *)&webgui_tpl);
	}
	fcache_gc();
	logprintf(LOG_DEBUG, "garbage collected webserver library");
	return 1;
}

int webserver_ishex(int x) {
	return(x >= '0' && x <= '9') || (x >= 'a' && x <= 'f') || (x >= 'A' && x <= 'F');
}

int webserver_urldecode(const char *s, char *dec) {
	char *o;
	const char *end = s + strlen(s);
	int c;

	for(o = dec; s <= end; o++) {
		c = *s++;
		if(c == '+') {
			c = ' ';
		} else if(c == '%' && (!webserver_ishex(*s++) || !webserver_ishex(*s++)	|| !sscanf(s - 2, "%2x", &c))) {
			return -1;
		}
		if(dec) {
			sprintf(o, "%c", c);
		}
	}

	return (int)(o - dec);
}

void webserver_create_header(unsigned char **p, const char *message, char *mimetype, unsigned int len) {
	*p += sprintf((char *)*p,
		"HTTP/1.0 %s\r\n"
		"Server: pilight\r\n"
		"Content-Type: %s\r\n",
		message, mimetype);
	*p += sprintf((char *)*p,
		"Content-Length: %u\r\n\r\n",
		len);
}

void webserver_create_404(const char *in, unsigned char **p) {
	char mimetype[] = "text/html";
	webserver_create_header(p, "404 Not Found", mimetype, (unsigned int)(202+strlen((const char *)in)));
	*p += sprintf((char *)*p, "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\x0d\x0a"
		"<html><head>\x0d\x0a"
		"<title>404 Not Found</title>\x0d\x0a"
		"</head><body>\x0d\x0a"
		"<h1>Not Found</h1>\x0d\x0a"
		"<p>The requested URL %s was not found on this server.</p>\x0d\x0a"
		"</body></html>",
		(const char *)in);
}

char *webserver_mimetype(const char *str) {
	char *mimetype = malloc(strlen(str)+1);
	if(!mimetype) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	memset(mimetype, '\0', strlen(str)+1);
	strcpy(mimetype, str);
	return mimetype;
}
static int webserver_sockets_callback(struct mg_connection *c) {
	if(c->is_websocket) {
		mg_websocket_write(c, 1, (char *)c->callback_param, strlen((char *)c->callback_param));
	}
	return MG_REQUEST_PROCESSED;
}

static int webserver_auth_handler(struct mg_connection *conn) {
	if(webserver_authentication == 1 && webserver_username != NULL && webserver_password != NULL) {
		return mg_authorize_input(conn, webserver_username, webserver_password, mg_get_option(mgserver, "auth_domain"));
	} else {
		return MG_AUTH_OK;
	}
}

static int webserver_request_handler(struct mg_connection *conn) {
	char *request = NULL;
	char *ext = NULL;
	char *mimetype = NULL;
	int size = 0;
	unsigned char *p;
	static unsigned char buffer[4096];

	if(!conn->is_websocket) {
		if(strcmp(conn->uri, "/send") == 0) {
			if(conn->query_string != NULL) {
				char out[strlen(conn->query_string)];
				webserver_urldecode(conn->query_string, out);
				socket_write(sockfd, out);
				mg_printf_data(conn, "{\"message\":\"success\"}");
			}
			return MG_REQUEST_PROCESSED;
		} else if(strcmp(conn->uri, "/config") == 0) {
			JsonNode *jsend = config_broadcast_create();
			char *output = json_stringify(jsend, NULL);
			mg_printf_data(conn, output);
			json_delete(jsend);
			jsend = NULL;
			return MG_REQUEST_PROCESSED;
		} else if(strcmp(conn->uri, "/") == 0) {
			request = malloc(strlen(webserver_root)+strlen(webgui_tpl)+14);
			if(!request) {
				logprintf(LOG_ERR, "out of memory");
				return -1;
			}
			memset(request, '\0', strlen(webserver_root)+strlen(webgui_tpl)+14);
			/* Check if the webserver_root is terminated by a slash. If not, than add it */
			if(webserver_root[strlen(webserver_root)-1] == '/') {
#ifdef __FreeBSD__
				sprintf(request, "%s%s/%s", webserver_root, webgui_tpl, "index.html");
#else
				sprintf(request, "%s%s%s", webserver_root, webgui_tpl, "index.html");
#endif
			} else {
				sprintf(request, "%s/%s/%s", webserver_root, webgui_tpl, "/index.html");
			}
		} else {
			size_t wlen = strlen(webserver_root)+strlen(webgui_tpl)+strlen(conn->uri)+2;
			request = malloc(wlen);
			if(!request) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			memset(request, '\0', wlen);
			/* If a file was requested add it to the webserver path to create the absolute path */
			if(webserver_root[strlen(webserver_root)-1] == '/') {
				if(conn->uri[0] == '/')
					sprintf(request, "%s%s%s", webserver_root, webgui_tpl, conn->uri);
				else
					sprintf(request, "%s%s/%s", webserver_root, webgui_tpl, conn->uri);
			} else {
				if(conn->uri[0] == '/')
					sprintf(request, "%s/%s%s", webserver_root, webgui_tpl, conn->uri);
				else
					sprintf(request, "%s/%s/%s", webserver_root, webgui_tpl, conn->uri);
			}
		}

		char *dot = NULL;
		/* Retrieve the extension of the requested file and create a mimetype accordingly */
		dot = strrchr(request, '.');
		if(!dot || dot == request) {
			mimetype = webserver_mimetype("text/plain");
		} else {
			ext = realloc(ext, strlen(dot)+1);
			if(!ext) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			memset(ext, '\0', strlen(dot)+1);
			strcpy(ext, dot+1);

			if(strcmp(ext, "html") == 0) {
				mimetype = webserver_mimetype("text/html");
			} else if(strcmp(ext, "xml") == 0) {
				mimetype = webserver_mimetype("text/xml");
			} else if(strcmp(ext, "png") == 0) {
				mimetype = webserver_mimetype("image/png");
			} else if(strcmp(ext, "gif") == 0) {
				mimetype = webserver_mimetype("image/gif");
			} else if(strcmp(ext, "ico") == 0) {
				mimetype = webserver_mimetype("image/x-icon");
			} else if(strcmp(ext, "css") == 0) {
				mimetype = webserver_mimetype("text/css");
			} else if(strcmp(ext, "js") == 0) {
				mimetype = webserver_mimetype("text/javascript");
			}
		}

		p = buffer;
		/* If webserver caching is enabled, first load all files in the memory */
		if(fcache_get_size(request, &size) != 0) {
			if((fcache_add(request)) != 0) {
				logprintf(LOG_NOTICE, "(webserver) could not cache %s", request);
				webserver_create_404(conn->uri, &p);
				mg_write(conn, buffer, (int)(p-buffer));
				sfree((void *)&mimetype);
				sfree((void *)&request);
				return MG_REQUEST_PROCESSED;
			}
		}
		/* Check if a file was succesfully stored in memory */
		if(fcache_get_size(request, &size) == 0) {
			webserver_create_header(&p, "200 OK", mimetype, (unsigned int)size);
			mg_write(conn, buffer, (int)(p-buffer));
			mg_write(conn, fcache_get_bytes(request), size);
			if(!webserver_cache) {
				fcache_rm(request);
			}
			sfree((void *)&mimetype);
			sfree((void *)&request);
		}
		sfree((void *)&mimetype);
		sfree((void *)&request);
	} else {
		char input[conn->content_len+1];
		strncpy(input, conn->content, conn->content_len);
		input[conn->content_len] = '\0';

		if(json_validate(input) == true) {
			JsonNode *json = json_decode(input);
			char *message = NULL;
			if(json_find_string(json, "message", &message) != -1) {
				if(strcmp(message, "request config") == 0) {
					JsonNode *jsend = config_broadcast_create();
					char *output = json_stringify(jsend, NULL);
					size_t output_len = strlen(output);
					mg_websocket_write(conn, 1, output, output_len);
					sfree((void *)&output);
					json_delete(jsend);
				} else if(strcmp(message, "send") == 0) {
					/* Write all codes coming from the webserver to the daemon */
					socket_write(sockfd, input);
				}
			}
			json_delete(json);
		}
		return MG_CLIENT_CONTINUE;
	}
	return MG_REQUEST_PROCESSED;
}

void webserver_queue(char *message) {
	pthread_mutex_lock(&webqueue_lock);
	struct webqueue_t *wnode = malloc(sizeof(struct webqueue_t));
	if(!wnode) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	wnode->message = malloc(strlen(message)+1);
	if(!wnode->message) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(wnode->message, message);

	if(webqueue_number == 0) {
		webqueue = wnode;
		webqueue_head = wnode;
	} else {
		webqueue_head->next = wnode;
		webqueue_head = wnode;
	}

	webqueue_number++;
	pthread_mutex_unlock(&webqueue_lock);
	pthread_cond_signal(&webqueue_signal);
}

void *webserver_clientize(void *param) {
	steps_t steps = WELCOME;
	char *message = NULL;
	struct ssdp_list_t *ssdp_list = NULL;
	int standalone = 0;
	
	// settings_find_number("standalone", &standalone);
	if(ssdp_seek(&ssdp_list) == -1 || standalone == 1) {
		logprintf(LOG_DEBUG, "no pilight ssdp connections found");
		server = malloc(10);
		if(!server) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		strcpy(server, "127.0.0.1");
		if((sockfd = socket_connect(server, (unsigned short)socket_get_port())) == -1) {
			logprintf(LOG_DEBUG, "could not connect to pilight-daemon");
			sfree((void *)&server);
			exit(EXIT_FAILURE);
		}
		sfree((void *)&server);
	} else {
		if((sockfd = socket_connect(ssdp_list->ip, ssdp_list->port)) == -1) {
			logprintf(LOG_DEBUG, "could not connect to pilight-daemon");
			exit(EXIT_FAILURE);
		}
		sfree((void *)&ssdp_list);
	}

	sfree((void *)&server);
	while(webserver_loop) {
		if(steps > WELCOME) {
			/* Clear the receive buffer again and read the welcome message */
			/* Used direct read access instead of socket_read */
			if((sockReadBuff = socket_read(sockfd)) == NULL) {
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
				sfree((void *)&sockReadBuff);
				sockReadBuff = NULL;
			}
			break;
			case SYNC:
				if(strlen(sockReadBuff) > 0) {
					webserver_queue(sockReadBuff);
					sfree((void *)&sockReadBuff);
					sockReadBuff = NULL;
				}
			break;
			case REJECT:
			default:
				goto close;
			break;
		}
	}
	if(sockReadBuff) {
		sfree((void *)&sockReadBuff);
		sockReadBuff = NULL;
	}
close:
	webserver_gc();
	return 0;
}

void *webserver_broadcast(void *param) {
	pthread_mutex_lock(&webqueue_lock);	

	while(webserver_loop) {
#ifdef __FreeBSD__
		pthread_mutex_lock(&webqueue_lock);		
#endif
		if(webqueue_number > 0) {
			pthread_mutex_lock(&webqueue_lock);

			mg_iterate_over_connections(mgserver, webserver_sockets_callback, webqueue->message);

			struct webqueue_t *tmp = webqueue;
			sfree((void *)&webqueue->message);
			webqueue = webqueue->next;
			sfree((void *)&tmp);
			webqueue_number--;
			pthread_mutex_unlock(&webqueue_lock);
		} else {
			pthread_cond_wait(&webqueue_signal, &webqueue_lock);
		}
	}
	return (void *)NULL;
}

void *webserver_start(void *param) {

#ifdef __FreeBSD__
	pthread_mutex_t webqueue_lock;
	pthread_cond_t webqueue_signal;
	pthread_mutexattr_t webqueue_attr;

	pthread_mutexattr_init(&webqueue_attr);
	pthread_mutexattr_settype(&webqueue_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_cond_init(&webqueue_signal, NULL);
	pthread_mutex_init(&webqueue_lock, &webqueue_attr);
#endif

	/* Check on what port the webserver needs to run */
	settings_find_number("webserver-port", &webserver_port);
	if(settings_find_string("webserver-root", &webserver_root) != 0) {
		/* If no webserver port was set, use the default webserver port */
		webserver_root = malloc(strlen(WEBSERVER_ROOT)+1);
		if(!webserver_root) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		strcpy(webserver_root, WEBSERVER_ROOT);
	}
	if(settings_find_string("webgui-template", &webgui_tpl) != 0) {
		/* If no webserver port was set, use the default webserver port */
		webgui_tpl = malloc(strlen(WEBGUI_TEMPLATE)+1);
		if(!webgui_tpl) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		strcpy(webgui_tpl, WEBGUI_TEMPLATE);
	}

	/* Do we turn on webserver caching. This means that all requested files are
	   loaded into the memory so they aren't read from the FS anymore */
	settings_find_number("webserver-cache", &webserver_cache);
	settings_find_number("webserver-authentication", &webserver_authentication);
	settings_find_string("webserver-password", &webserver_password);
	settings_find_string("webserver-username", &webserver_username);

	mgserver = mg_create_server(NULL);
	char webport[10] = {'\0'};
	sprintf(webport, "%d", webserver_port);
	mg_set_option(mgserver, "listening_port", webport);
	mg_set_option(mgserver, "auth_domain", "pilight");
	mg_set_request_handler(mgserver, webserver_request_handler);
	mg_set_auth_handler(mgserver, webserver_auth_handler);

	/* Register a seperate thread in which the webserver communicates
	   the main daemon as if it where a gui */
	threads_register("webserver client", &webserver_clientize, (void *)NULL);
	threads_register("webserver broadcast", &webserver_broadcast, (void *)NULL);

	logprintf(LOG_DEBUG, "webserver listening to port %s", mg_get_option(mgserver, "listening_port"));
	/* Main webserver loop */
	while(webserver_loop) {
		mg_poll_server(mgserver, 100);
	}
	mg_destroy_server(&mgserver);

	return 0;
}
