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
#include "../websockets/libwebsockets.h"
#include "../websockets/private-libwebsockets.h"
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

int sockfd = 0;
char *sockReadBuff = NULL;
char *syncBuff = NULL;
unsigned char *sockWriteBuff = NULL;

char *ext = NULL;
char *server;

typedef enum {
	WELCOME,
	IDENTIFY,
	REJECT,
	SYNC
} steps_t;

struct per_session_data__http {
	int fd;
	int free;
	unsigned char *bytes;
	unsigned short loggedin;
};

struct libwebsocket_protocols libwebsocket_protocols[] = {
	{ "http-only", webserver_callback_http, sizeof(struct per_session_data__http), 0 },
	{ "data", webserver_callback_data, 0, 0 },
	{ NULL, NULL, 0, 0 }
};

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
	sfree((void *)&syncBuff);
	sfree((void *)&sockWriteBuff);
	sfree((void *)&sockReadBuff);
	sfree((void *)&ext);
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

void webserver_create_wsi(struct libwebsocket **wsi, int fd, unsigned char *stream, size_t size) {
	(*wsi)->u.http.fd = fd;
	// (*wsi)->u.http.stream = stream;
	(*wsi)->u.http.filelen = size;
	(*wsi)->u.http.filepos = 0;
	// (*wsi)->u.http.choke = 1;
	(*wsi)->state = WSI_STATE_HTTP_ISSUING_FILE;
}

void webserver_create_401(unsigned char **p) {
	*p += sprintf((char *)*p,
		"HTTP/1.1 401 Authorization Required\r\n"
		"WWW-Authenticate: Basic realm=\"pilight\"\r\n"
		"Server: pilight\r\n"
		"Content-Length: 40\r\n"
		"Content-Type: text/html\r\n\r\n");
	*p += sprintf((char *)*p, "401 Authorization Required");
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

int base64decode(unsigned char *dest, unsigned char *src, int l) {
	static char inalphabet[256], decoder[256];
	int i, bits, c, char_count;
	int rpos;
	int wpos = 0;

	for(i=(sizeof alphabet)-1;i>=0;i--) {
		inalphabet[alphabet[i]] = 1;
		decoder[alphabet[i]] = (char)i;
	}

	char_count = 0;
	bits = 0;
	for(rpos=0;rpos<l;rpos++) {
		c = src[rpos];

		if(c == '=') {
			break;
		}

		if(c > 255 || !inalphabet[c]) {
			continue;
		}

		bits += decoder[c];
		char_count++;
		if(char_count < 4) {
			bits <<= 6;
		} else {
			dest[wpos++] = (unsigned char)(bits >> 16);
			dest[wpos++] = (unsigned char)((bits >> 8) & 0xff);
			dest[wpos++] = (unsigned char)(bits & 0xff);
			bits = 0;
			char_count = 0;
		}
	}

	switch(char_count) {
		case 1:
			return -1;
		break;
		case 2:
			dest[wpos++] = (unsigned char)(bits >> 10);
		break;
		case 3:
			dest[wpos++] = (unsigned char)(bits >> 16);
			dest[wpos++] = (unsigned char)((bits >> 8) & 0xff);
		break;
		default:
		break;
	}

	return wpos;
}

int webserver_callback_http(struct libwebsocket_context *webcontext, struct libwebsocket *wsi, enum libwebsocket_callback_reasons reason, void *user, void *in, size_t len) {
	int n, m;
	unsigned char *p;
	static unsigned char buffer[4096];
	struct stat stat_buf;
	int size;
	struct per_session_data__http *pss = (struct per_session_data__http *)user;
	char *request = NULL;
	char *mimetype = NULL;

	switch(reason) {
		case LWS_CALLBACK_HTTP:
			if(pss->loggedin == 0 && webserver_authentication == 1 && webserver_username != NULL && webserver_password != NULL) {
				/* Check if authorization header field was given and extract username and password */
				char *pch = strtok((char *)webcontext->service_buffer, "\r\n");
				char encpass[1024] = {'\0'};
				char decpass[1024] = {'\0'};
				char username[512] = {'\0'};
				char password[512] = {'\0'};

				while(pch) {
					sscanf(pch, "Authorization: Basic%*[ \n\t]%s", encpass);
					if(strlen(encpass) > 0) {
						base64decode((unsigned char *)decpass, (unsigned char *)encpass, (int)strlen(encpass));
						break;
					}
					pch = strtok(NULL, "\r\n");
				}
				if(strlen(decpass) > 0) {
					int error = 0;
					pch = strtok(decpass, ":");
					if(pch != NULL) {
						strcpy(&username[0], pch);
					} else {
						error = 1;
					}
					pch = strtok(NULL, ":");
					if(pch != NULL) {
						strcpy(&password[0], pch);
					} else {
						error = 1;
					};
					if(error == 0 && strcmp(username, webserver_username) == 0 && strcmp(password, webserver_password) == 0) {
						pss->loggedin = 1;
					}
				}
				if(pss->loggedin == 0) {
					p = buffer;
					webserver_create_401(&p);
					libwebsocket_write(wsi, buffer, (size_t)(p-buffer), LWS_WRITE_HTTP);
					return -1;
				}
			}

			/* If the webserver didn't request a file, serve the index.html */
			if(strcmp((const char *)in, "/") == 0) {
				request = malloc(strlen(webserver_root)+strlen(webgui_tpl)+14);
				memset(request, '\0', strlen(webserver_root)+strlen(webgui_tpl)+14);
				/* Check if the webserver_root is terminated by a slash. If not, than add it */
				if(webserver_root[strlen(webserver_root)-1] == '/') {
	#ifdef __FreeBSD__
					sprintf(request, "%s%s/%s", webserver_root, webgui_tpl, "index.html");
	#else
					sprintf(request, "%s%s%s", webserver_root, webgui_tpl, "index.html");
	#endif
				} else {
					sprintf(request, "%s/%s%s", webserver_root, webgui_tpl, "/index.html");
				}
			} else {
				if(strstr((char *)in, "/send") > 0) {
					char out[strlen((char *)in)];
					webserver_urldecode(&((char *)in)[6], out);
					socket_write(sockfd, out);
					return -1;
				} else if(strstr((char *)in, "/config") > 0) {
					JsonNode *jsend = config_broadcast_create();
					char *output = json_stringify(jsend, NULL);

					p = buffer;
					size = (int)strlen(output);
					mimetype = malloc(11);
					strcpy(mimetype, "text/plain");
					webserver_create_header(&p, "200 OK", mimetype, (unsigned int)size);
					sfree((void *)&mimetype);

					libwebsocket_write(wsi, webcontext->service_buffer, (size_t)(p-webcontext->service_buffer), LWS_WRITE_HTTP);
					pss->bytes = (unsigned char *)output;
					pss->free = 1;
					json_delete(jsend);
					jsend = NULL;
					libwebsocket_callback_on_writable(webcontext, wsi);
					break;
				}

				size_t wlen = strlen(webserver_root)+strlen(webgui_tpl)+strlen((const char *)in)+2;
				request = malloc(wlen);
				memset(request, '\0', wlen);
				/* If a file was requested add it to the webserver path to create the absolute path */
				if(webserver_root[strlen(webserver_root)-1] == '/') {
					if(((char *)in)[0] == '/')
						sprintf(request, "%s%s%s", webserver_root, webgui_tpl, (char *)in);
					else
						sprintf(request, "%s%s/%s", webserver_root, webgui_tpl, (char *)in);
				} else {
					if(((char *)in)[0] == '/')
						sprintf(request, "%s/%s%s", webserver_root, webgui_tpl, (const char *)in);
					else
						sprintf(request, "%s/%s/%s", webserver_root, webgui_tpl, (const char *)in);
				}
			}

			char *dot = NULL;
			/* Retrieve the extension of the requested file and create a mimetype accordingly */
			dot = strrchr(request, '.');
			if(!dot || dot == request) {
				free(request);
				return -1;
			}

			ext = realloc(ext, strlen(dot)+1);
			memset(ext, '\0', strlen(dot)+1);
			strcpy(ext, dot+1);

			if(strcmp(ext, "html") == 0) {
				mimetype = malloc(10);
				memset(mimetype, '\0', 10);
				strcpy(mimetype, "text/html");
			} else if(strcmp(ext, "xml") == 0) {
				mimetype = malloc(10);
				memset(mimetype, '\0', 10);
				strcpy(mimetype, "text/xml");
			} else if(strcmp(ext, "png") == 0) {
				mimetype = malloc(10);
				memset(mimetype, '\0', 10);
				strcpy(mimetype, "image/png");
			} else if(strcmp(ext, "ico") == 0) {
				mimetype = malloc(13);
				memset(mimetype, '\0', 13);
				strcpy(mimetype, "image/x-icon");
			} else if(strcmp(ext, "css") == 0) {
				mimetype = malloc(9);
				memset(mimetype, '\0', 9);
				strcpy(mimetype, "text/css");
			} else if(strcmp(ext, "js") == 0) {
				mimetype = malloc(16);
				memset(mimetype, '\0', 16);
				strcpy(mimetype, "text/javascript");
			}

			if(webserver_cache) {
				p = buffer;
				/* If webserver caching is enabled, first load all files in the memory */
				if(fcache_get_size(request, &size) != 0) {
					if((fcache_add(request)) != 0) {
						logprintf(LOG_NOTICE, "(webserver) could not cache %s", request);
						webserver_create_404((const char *)in, &p);
						libwebsocket_write(wsi, buffer, (size_t)(p-buffer), LWS_WRITE_HTTP);
						sfree((void *)&mimetype);
						sfree((void *)&request);
						return -1;
					}
				}

				/* Check if a file was succesfully stored in memory */
				if(fcache_get_size(request, &size) == 0) {

					webserver_create_header(&p, "200 OK", mimetype, (unsigned int)size);
					libwebsocket_write(wsi, buffer, (size_t)(p-buffer), LWS_WRITE_HTTP);

					pss->bytes = fcache_get_bytes(request);
					pss->free = 0;
					libwebsocket_callback_on_writable(webcontext, wsi);

					sfree((void *)&mimetype);
					sfree((void *)&request);
					break;
				}
				sfree((void *)&mimetype);
				sfree((void *)&request);
			} else {
				/* check for the "send a big file by hand" example case */
				int fd = open(request, O_RDONLY);
				fstat(fd, &stat_buf);
				p = buffer;
				if(fd < 0) {
					webserver_create_404((const char *)in, &p);
					libwebsocket_write(wsi, buffer, (size_t)(p-buffer), LWS_WRITE_HTTP);
					free(mimetype);
					free(request);
					return -1;
				}

				webserver_create_header(&p, "200 OK", mimetype, (unsigned long)stat_buf.st_size);
				n = libwebsocket_write(wsi, buffer, (size_t)(p-buffer), LWS_WRITE_HTTP);
				if(n < 0) {
					close(pss->fd);
					return -1;
				}
				sfree((void *)&mimetype);
				sfree((void *)&request);
				if(stat_buf.st_size > LWS_MAX_SOCKET_IO_BUF) {
					pss->fd = fd;
					pss->bytes = NULL;
					pss->free = 0;
					libwebsocket_callback_on_writable(webcontext, wsi);
					break;
				} else {
					libwebsockets_serve_http_file_fragment(webcontext, wsi);
					break;
				}
			}
		break;
		case LWS_CALLBACK_HTTP_BODY:
		break;
		case LWS_CALLBACK_HTTP_BODY_COMPLETION:
		case LWS_CALLBACK_HTTP_FILE_COMPLETION:
		return -1;
		case LWS_CALLBACK_HTTP_WRITEABLE:
			if(pss->bytes) {
				wsi->u.http.filelen = strlen((char *)pss->bytes);
				n = LWS_MAX_SOCKET_IO_BUF;

				do {
					if(wsi->u.http.filelen-wsi->u.http.filepos < n) {
						n = (int)(wsi->u.http.filelen-wsi->u.http.filepos);
					} else {
						n = sizeof(buffer);
					}
					memcpy(buffer, &pss->bytes[wsi->u.http.filepos], (size_t)n);

					if(n > 0) {
						m = libwebsocket_write(wsi, buffer, (size_t)n, LWS_WRITE_HTTP);

						if(m < 0) {
							goto bail;
						}

						wsi->u.http.filepos += (long unsigned int)m;
						if(m != n) {
							wsi->u.http.filepos += (long unsigned int)(m - n);
						}
					}

					if(n < 0) {
						goto bail;
					}

					if(n == 0) {
						goto flush_bail;
					}

					if(wsi->u.http.filelen == wsi->u.http.filepos) {
						return -1;
					}
				} while (!lws_send_pipe_choked(wsi));
				libwebsocket_callback_on_writable(webcontext, wsi);
			} else {
				do {
					n = read(pss->fd, buffer, sizeof buffer);

					if(n < 0) {
						goto bail;
					}
					if(n == 0) {
						goto flush_bail;
					}

					m = libwebsocket_write(wsi, buffer, (size_t)n, LWS_WRITE_HTTP);
					if(m < 0) {
						goto bail;
					}
					if (m != n)
						lseek(pss->fd, m - n, SEEK_CUR);

				} while (!lws_send_pipe_choked(wsi));
				libwebsocket_callback_on_writable(webcontext, wsi);
			}
		break;
		flush_bail:
			if(lws_send_pipe_choked(wsi)) {
				libwebsocket_callback_on_writable(webcontext, wsi);
				break;
			}
		bail:
			if(pss->fd > 0) {
				close(pss->fd);
			}
			if(pss->free == 1) {
				sfree((void *)&pss->bytes);
			}
			return -1;
		case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
			if((int)(intptr_t)in > 0) {
				struct sockaddr_in address;
				int addrlen = sizeof(address);
				getpeername((int)(intptr_t)in, (struct sockaddr*)&address, (socklen_t*)&addrlen);
				if(socket_check_whitelist(inet_ntoa(address.sin_addr)) != 0) {
					logprintf(LOG_INFO, "rejected client, ip: %s, port: %d", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
					return -1;
				} else {
					logprintf(LOG_INFO, "client connected, ip %s, port %d", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
					return 0;
				}
			}
		break;
		case LWS_CALLBACK_ESTABLISHED:
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		case LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH:
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
		case LWS_CALLBACK_CLOSED:
		case LWS_CALLBACK_CLOSED_HTTP:
		case LWS_CALLBACK_CLIENT_RECEIVE:
		case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
		case LWS_CALLBACK_CLIENT_WRITEABLE:
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
		case LWS_CALLBACK_RECEIVE:
		case LWS_CALLBACK_SERVER_WRITEABLE:
		case LWS_CALLBACK_FILTER_HTTP_CONNECTION:
		case LWS_CALLBACK_GET_THREAD_ID:
		case LWS_CALLBACK_LOCK_POLL:
		case LWS_CALLBACK_UNLOCK_POLL:
		default:
		break;
	}

	return 0;
}

int webserver_callback_data(struct libwebsocket_context *webcontext, struct libwebsocket *wsi, enum libwebsocket_callback_reasons reason, void *user, void *in, size_t len) {
	int m;
	switch(reason) {
	case LWS_CALLBACK_RECEIVE:
			if((int)len < 4) {
				return -1;
			} else {
				if(json_validate((char *)in) == true) {
					JsonNode *json = json_decode((char *)in);
					char *message = NULL;

					if(json_find_string(json, "message", &message) != -1) {
						if(strcmp(message, "request config") == 0) {

							JsonNode *jsend = config_broadcast_create();
							char *output = json_stringify(jsend, NULL);
							size_t output_len = strlen(output);
							/* This PRE_PADDIGN and POST_PADDING is an requirement for LWS_WRITE_TEXT */
							sockWriteBuff = realloc(sockWriteBuff, LWS_SEND_BUFFER_PRE_PADDING + output_len + LWS_SEND_BUFFER_POST_PADDING);
							memset(sockWriteBuff, '\0', sizeof(*sockWriteBuff));
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
			return 0;
		break;
		case LWS_CALLBACK_SERVER_WRITEABLE: {
			if(syncBuff) {
				/* Push the incoming message to the webgui */
				size_t l = strlen(syncBuff);

				/* This PRE_PADDIGN and POST_PADDING is an requirement for LWS_WRITE_TEXT */
				sockWriteBuff = realloc(sockWriteBuff, LWS_SEND_BUFFER_PRE_PADDING + l + LWS_SEND_BUFFER_POST_PADDING);
				memset(sockWriteBuff, '\0', sizeof(*sockWriteBuff));
				memcpy(&sockWriteBuff[LWS_SEND_BUFFER_PRE_PADDING], syncBuff, l);
				m = libwebsocket_write(wsi, &sockWriteBuff[LWS_SEND_BUFFER_PRE_PADDING], l, LWS_WRITE_TEXT);
				/*
				 * It seems like libwebsocket_write already does memory freeing
				 */
				if (m < l) {
					logprintf(LOG_ERR, "(webserver) %d writing to socket", l);
					return -1;
				}
				return 0;
			}
		}
		break;
		case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
			if((int)(intptr_t)in > 0) {
				struct sockaddr_in address;
				int addrlen = sizeof(address);
				getpeername((int)(intptr_t)in, (struct sockaddr*)&address, (socklen_t*)&addrlen);
				if(socket_check_whitelist(inet_ntoa(address.sin_addr)) != 0) {
					logprintf(LOG_INFO, "rejected client, ip: %s, port: %d", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
					return -1;
				} else {
					logprintf(LOG_INFO, "client connected, ip %s, port %d", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
					return 0;
				}
			}
		break;
		case LWS_CALLBACK_HTTP:
		case LWS_CALLBACK_HTTP_FILE_COMPLETION:
		case LWS_CALLBACK_HTTP_WRITEABLE:
		case LWS_CALLBACK_ESTABLISHED:
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		case LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH:
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
		case LWS_CALLBACK_CLOSED:
		case LWS_CALLBACK_CLOSED_HTTP:
		case LWS_CALLBACK_CLIENT_RECEIVE:
		case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
		case LWS_CALLBACK_CLIENT_WRITEABLE:
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
		case LWS_CALLBACK_HTTP_BODY:
		case LWS_CALLBACK_HTTP_BODY_COMPLETION:
		case LWS_CALLBACK_FILTER_HTTP_CONNECTION:
		case LWS_CALLBACK_GET_THREAD_ID:
		case LWS_CALLBACK_LOCK_POLL:
		case LWS_CALLBACK_UNLOCK_POLL:
		default:
		break;
	}
	return 0;
}

void webserver_queue(char *message) {
	pthread_mutex_lock(&webqueue_lock);
	struct webqueue_t *wnode = malloc(sizeof(struct webqueue_t));
	wnode->message = malloc(strlen(message)+1);
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

	if(ssdp_seek(&ssdp_list) == -1) {
		logprintf(LOG_DEBUG, "no pilight ssdp connections found");
		server = malloc(10);
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
					webserver_queue(sockReadBuff);
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

void *webserver_broadcast(void *param) {
	pthread_mutex_lock(&webqueue_lock);
	while(webserver_loop) {
		if(webqueue_number > 0) {
			pthread_mutex_lock(&webqueue_lock);

			syncBuff = realloc(syncBuff, strlen(webqueue->message)+1);
			memset(syncBuff, '\0', sizeof(*syncBuff));
			strcpy(syncBuff, webqueue->message);

			libwebsocket_callback_on_writable_all_protocol(&libwebsocket_protocols[1]);

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
	pthread_mutexattr_t webqueue_attr;

	pthread_mutexattr_init(&webqueue_attr);
	pthread_mutexattr_settype(&webqueue_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&webqueue_lock, &webqueue_attr);
#endif

	int n = 0;
	struct lws_context_creation_info info;

	/* Check on what port the webserver needs to run */
	settings_find_number("webserver-port", &webserver_port);
	if(settings_find_string("webserver-root", &webserver_root) != 0) {
		/* If no webserver port was set, use the default webserver port */
		webserver_root = malloc(strlen(WEBSERVER_ROOT)+1);
		strcpy(webserver_root, WEBSERVER_ROOT);
	}
	if(settings_find_string("webgui-template", &webgui_tpl) != 0) {
		/* If no webserver port was set, use the default webserver port */
		webgui_tpl = malloc(strlen(WEBGUI_TEMPLATE)+1);
		strcpy(webgui_tpl, WEBGUI_TEMPLATE);
	}

	/* Do we turn on webserver caching. This means that all requested files are
	   loaded into the memory so they aren't read from the FS anymore */
	settings_find_number("webserver-cache", &webserver_cache);
	settings_find_number("webserver-authentication", &webserver_authentication);
	settings_find_string("webserver-password", &webserver_password);
	settings_find_string("webserver-username", &webserver_username);

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
		threads_register("webserver client", &webserver_clientize, (void *)NULL);
		threads_register("webserver broadcast", &webserver_broadcast, (void *)NULL);
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
