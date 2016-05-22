/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/*
 * This webserver contains various code originally written by Sergey Lyubka
 * (from Cesenta) for Mongoose (https://github.com/cesanta/mongoose).
 * The code was either copied, adapted and/or stripped so it could be integrated
 * with the pilight eventpool and mbed TLS SSL library. 
 *
 * The following functions are therefor licensed under GPLv2.
 *
 * send_websocket_handshake_if_requested
 * send_websocket_handshake
 * http_parse_request
 * _urldecode
 * parse_http_headers
 * remove_double_dots_and_double_slashes
 * is_valid_http_method
 * *skip
 * websocket_write
 * authorize_input
 * http_get_header
 *
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
#include <signal.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <arpa/inet.h>
#ifdef _WIN32
#else
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
	#include <pwd.h>
#endif
#define __USE_UNIX98
#include <pthread.h>

#include "../storage/storage.h"
#include "threadpool.h"
#include "eventpool.h"
#include "sha256cache.h"
#include "pilight.h"
#include "network.h"
#include "gc.h"
#include "log.h"
#include "json.h"
#include "webserver.h"
#include "ssdp.h"

#include "../../mbedtls/mbedtls/sha1.h"

#ifdef WEBSERVER_HTTPS
#include "ssl.h"
#include "../../mbedtls/mbedtls/error.h"
#include "../../mbedtls/mbedtls/pk.h"
#include "../../mbedtls/mbedtls/net.h"
#include "../../mbedtls/mbedtls/x509_crt.h"
#include "../../mbedtls/mbedtls/ctr_drbg.h"
#include "../../mbedtls/mbedtls/entropy.h"
#include "../../mbedtls/mbedtls/ssl.h"
#include "../../mbedtls/mbedtls/ssl_cache.h"

static int https_port = WEBSERVER_HTTPS_PORT;
#endif

static struct eventpool_fd_t *http_server = NULL;
static struct eventpool_fd_t *https_server = NULL;

static int http_port = WEBSERVER_HTTP_PORT;
static int websockets = WEBGUI_WEBSOCKETS;
static int cache = 1;
static char *authentication_username = NULL;
static char *authentication_password = NULL;
static unsigned short loop = 1;
static char *root = NULL;
static unsigned short root_free = 0;

enum mg_result {
	MG_FALSE,
	MG_TRUE,
	MG_MORE
};

enum {
	WEBSOCKET_OPCODE_CONTINUATION = 0x0,
	WEBSOCKET_OPCODE_TEXT = 0x1,
	WEBSOCKET_OPCODE_BINARY = 0x2,
	WEBSOCKET_OPCODE_CONNECTION_CLOSE = 0x8,
	WEBSOCKET_OPCODE_PING = 0x9,
	WEBSOCKET_OPCODE_PONG = 0xa
};

typedef struct websocket_clients_t {
	struct connection_t *connection;
	struct websocket_clients_t *next;
} websocket_clients_t;

typedef struct fcache_t {
	char *name;
	unsigned long size;
	unsigned char *bytes;
	struct fcache_t *next;
} fcaches_t;

static struct fcache_t *fcache;

static pthread_mutex_t websocket_lock;
static pthread_mutexattr_t websocket_attr;
struct websocket_clients_t *websocket_clients = NULL;

static void *reason_socket_received_free(void *param) {
	struct reason_socket_received_t *data = param;
	FREE(data->buffer);
	FREE(data);
	return NULL;
}

static void *reason_webserver_connected_free(void *param) {
	struct reason_webserver_connected_t *data = param;
	FREE(data);
	return NULL;
}

#ifdef WEBSERVER_HTTPS
static int initialize_ssl(struct connection_t *conn) {
	char buffer[BUFFER_SIZE];
	int ret = 0;

	if((ret = mbedtls_ssl_setup(&conn->ssl, &ssl_server_conf)) != 0) {
		mbedtls_strerror(ret, (char *)&buffer, BUFFER_SIZE);
		logprintf(LOG_ERR, "mbedtls_ssl_setup failed: %s", buffer);
		return -1;
	}

	return 0;
}
#endif

int webserver_gc(void) {
	struct websocket_clients_t *node = NULL;

	loop = 0;

	if(root_free == 1) {
		FREE(root);
	}

	while(websocket_clients) {
		node = websocket_clients;
		websocket_clients = websocket_clients->next;
		FREE(node);
	}

	if(websocket_clients != NULL) {
		FREE(websocket_clients);
	}

	struct fcache_t *tmp = fcache;
	while(fcache) {
		tmp = fcache;
		FREE(tmp->name);
		FREE(tmp->bytes);
		fcache = fcache->next;
		FREE(tmp);
	}
	if(fcache != NULL) {
		FREE(fcache);
	}

	sha256cache_gc();
	logprintf(LOG_DEBUG, "garbage collected webserver library");
	return 1;
}

void webserver_create_header(char **p, const char *message, char *mimetype, unsigned long len) {
	*p += sprintf((char *)*p,
		"HTTP/1.0 %s\r\n"
		"Server: pilight\r\n"
		"Keep-Alive: timeout=15, max=100\r\n"
		"Content-Type: %s\r\n",
		message, mimetype);
	*p += sprintf((char *)*p,
		"Content-Length: %lu\r\n\r\n",
		len);
}

static void create_404_header(const char *in, char **p) {
	char mimetype[] = "text/html";
	webserver_create_header(p, "404 Not Found", mimetype, (unsigned long)(204+strlen((const char *)in)));
	*p += sprintf((char *)*p, "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\x0d\x0a"
		"<html><head>\x0d\x0a"
		"<title>404 Not Found</title>\x0d\x0a"
		"</head><body>\x0d\x0a"
		"<h1>Not Found</h1>\x0d\x0a"
		"<p>The requested URL %s was not found on this server.</p>\x0d\x0a"
		"</body></html>",
		(const char *)in);
}

const char *http_get_header(const struct connection_t *conn, const char *s) {
	int i = 0;

	for(i = 0; i < conn->num_headers; i++) {
    if(strcmp(s, conn->http_headers[i].name) == 0) {
      return conn->http_headers[i].value;
		}
	}

  return NULL;
}

void send_auth_request(struct connection_t *conn) {
	char *a = "HTTP/1.1 401 Unauthorized\r\n"
		"WWW-Authenticate: Basic realm=\"pilight\"\r\n\r\n";
  conn->status_code = 401;

	eventpool_fd_write(conn->fd, a, strlen(a));
}

int authorize_input(struct connection_t *c, char *username, char *password) {
  const char *hdr = NULL;
	char **array = NULL, *decoded = NULL;
	int n = 0;

  if(c == NULL) {
		return MG_FALSE;
	}

	if((hdr = http_get_header(c, "Authorization")) == NULL ||
	   (strncmp(hdr, "Basic ", 6) != 0 &&
		 strncmp(hdr, "basic ", 6) != 0)) {
		return MG_FALSE;
	}

	char user[strlen(hdr)+1];
	strcpy(user, &hdr[6]);

	if((decoded = base64decode(user, strlen(user), NULL)) == NULL) {
		return MG_FALSE;
	}

	if((n = explode(decoded, ":", &array)) == 2) {
		if(strlen(array[1]) < 64) {
			if((array[1] = REALLOC(array[1], 65)) == NULL) {
				OUT_OF_MEMORY
			}
		}

		if(sha256cache_get_hash(array[1]) == NULL) {
			sha256cache_add(array[1]);
		}

		if(strcmp(sha256cache_get_hash(array[1]), password) == 0 && strcmp(array[0], username) == 0) {
			array_free(&array, n);
			FREE(decoded);
			return MG_TRUE;
		}
	}
	FREE(decoded);
	array_free(&array, n);

  return MG_FALSE;
}

static int auth_handler(struct connection_t *conn) {
	if(authentication_username != NULL && authentication_password != NULL) {
		return authorize_input(conn, authentication_username, authentication_password);
	} else {
		return MG_TRUE;
	}
}

size_t websocket_write(struct connection_t *conn, int opcode, const char *data, unsigned long long data_len) {
	unsigned char *copy = NULL;
	size_t copy_len = 0;
	int index = 2;

	if((copy = REALLOC(copy, data_len + 10)) == NULL) {
		OUT_OF_MEMORY
	}

	copy[0] = 0x80 + (opcode & 0x0f);
	if(data_len <= 125) {
		copy[1] = data_len;
	} else if(data_len < 65535) {
		copy[1] = 126;
		copy[2] = (data_len >> 8) & 255;
		copy[3] = (data_len) & 255;
		index = 4;
	} else {
		copy[1] = 127;
		copy[2] = (data_len >> 56) & 255;
		copy[3] = (data_len >> 48) & 255;
		copy[4] = (data_len >> 40) & 255;
		copy[5] = (data_len >> 32) & 255;
		copy[6] = (data_len >> 24) & 255;
		copy[7] = (data_len >> 16) & 255;
		copy[8] = (data_len >> 8) & 255;
		copy[9] = (data_len) & 255;
		index = 10;
	}
	if(data != NULL) {
		memcpy(&copy[index], data, data_len);
	}
	copy_len = data_len + index;

	if(copy_len > 0) {
		eventpool_fd_write(conn->fd, (char *)copy, copy_len);
	}

	FREE(copy);
	return data_len;
}

static void *webserver_send(void *param) {
	struct threadpool_tasks_t *task = param;
	struct reason_socket_send_t *data = task->userdata;
	struct connection_t c;
	memset(&c, 0, sizeof(struct connection_t));

	if(strcmp(data->type, "websocket") == 0) {
		c.fd = data->fd;
		websocket_write(&c, WEBSOCKET_OPCODE_TEXT, data->buffer, strlen(data->buffer));
	}

	return NULL;
}

static void write_chunk(struct connection_t *conn, char *buf, int len) {
  char chunk_size[50];
  int n = snprintf(chunk_size, sizeof(chunk_size), "%X\r\n", len);

	eventpool_fd_write(conn->fd, chunk_size, n);
	eventpool_fd_write(conn->fd, buf, len);
	eventpool_fd_write(conn->fd, "\r\n", 2);
}

static size_t send_data(struct connection_t *conn, void *data, unsigned long data_len) {
	if(conn->flags == 0) {
		char *a = "HTTP/1.1 200 OK\r\nKeep-Alive: timeout=15, max=100\r\nTransfer-Encoding: chunked\r\n\r\n";
		eventpool_fd_write(conn->fd, a, strlen(a));
		conn->flags = 1;
	}
	write_chunk(conn, data, data_len);
  return 0;
}

static int parse_rest(struct connection_t *conn) {
	char **array = NULL, **array1 = NULL;
	struct JsonNode *jobject = json_mkobject();
	struct JsonNode *jcode = json_mkobject();
	struct JsonNode *jvalues = json_mkobject();
	int a = 0, b = 0, c = 0, has_protocol = 0;

	if(strcmp(conn->request_method, "POST") == 0) {
		conn->query_string = conn->content;
	}
	if(strcmp(conn->request_method, "GET") == 0 && conn->query_string == NULL) {
		conn->query_string = conn->content;
	}
	if(conn->query_string != NULL) {
		char *dev = NULL;
		char decoded[strlen(conn->query_string)+1];

		if(urldecode(conn->query_string, decoded) == -1) {
			char *z = "{\"message\":\"failed\",\"error\":\"cannot decode url\"}";
			send_data(conn, z, strlen(z));
			return MG_TRUE;
		}

		char state[16], *p = NULL;
		a = explode((char *)decoded, "&", &array), b = 0, c = 0;
		memset(state, 0, 16);

		if(a > 1) {
			int type = 0; //0 = SEND, 1 = CONTROL
			json_append_member(jobject, "code", jcode);

			if(strcmp(conn->uri, "/send") == 0) {
				type = 0;
				json_append_member(jobject, "action", json_mkstring("send"));
			} else if(strcmp(conn->uri, "/control") == 0) {
				type = 1;
				json_append_member(jobject, "action", json_mkstring("control"));
			}

			for(b=0;b<a;b++) {
				c = explode(array[b], "=", &array1);
				if(c == 2) {
					if(strcmp(array1[0], "protocol") == 0) {
						struct JsonNode *jprotocol = json_mkarray();
						json_append_element(jprotocol, json_mkstring(array1[1]));
						json_append_member(jcode, "protocol", jprotocol);
						has_protocol = 1;
					} else if(strcmp(array1[0], "state") == 0) {
						strcpy(state, array1[1]);
					} else if(strcmp(array1[0], "device") == 0) {
						dev = array1[1];
						if(devices_select(ORIGIN_WEBSERVER, array1[1], NULL) != 0) {
							char *z = "{\"message\":\"failed\",\"error\":\"device does not exists\"}";
							send_data(conn, z, strlen(z));
							goto clear;
						}
					} else if(strncmp(array1[0], "values", 6) == 0) {
						char name[255], *ptr = name;
						if(sscanf(array1[0], "values[%254[a-z]]", ptr) != 1) {
							char *z = "{\"message\":\"failed\",\"error\":\"values should be passed like this \'values[dimlevel]=10\'\"}";
							send_data(conn, z, strlen(z));
							goto clear;
						} else {
							if(isNumeric(array1[1]) == 0) {
								json_append_member(jvalues, name, json_mknumber(atof(array1[1]), nrDecimals(array1[1])));
							} else {
								json_append_member(jvalues, name, json_mkstring(array1[1]));
							}
						}
					} else if(isNumeric(array1[1]) == 0) {
						json_append_member(jcode, array1[0], json_mknumber(atof(array1[1]), nrDecimals(array1[1])));
					} else {
						json_append_member(jcode, array1[0], json_mkstring(array1[1]));
					}
				}
			}
			if(type == 0) {
				if(has_protocol == 0) {
					char *z = "{\"message\":\"failed\",\"error\":\"no protocol was send\"}";
					send_data(conn, z, strlen(z));
					goto clear;
				}
				if(pilight.send != NULL) {
					if(pilight.send(jobject, ORIGIN_WEBSERVER) == 0) {
						char *z = "{\"message\":\"success\"}";
						send_data(conn, z, strlen(z));
						goto clear;
					}
				}
			} else if(type == 1) {
				if(pilight.control != NULL) {
					if(dev == NULL) {
						char *z = "{\"message\":\"failed\",\"error\":\"no device was send\"}";
						send_data(conn, z, strlen(z));
						goto clear;
					} else {
						if(strlen(state) > 0) {
							p = state;
						}
						if(pilight.control(dev, p, json_first_child(jvalues), ORIGIN_WEBSERVER) == 0) {
							char *z = "{\"message\":\"success\"}";
							send_data(conn, z, strlen(z));
							goto clear;
						}
					}
				}
			}
			json_delete(jvalues);
			json_delete(jobject);
		}
		array_free(&array, a);
	}
	char *z = "{\"message\":\"failed\"}";
	send_data(conn, z, strlen(z));
	return MG_TRUE;

clear:
	array_free(&array1, c);
	array_free(&array, a);
	json_delete(jvalues);
	json_delete(jobject);

	return MG_TRUE;
}

static int file_callback(struct eventpool_fd_t *node, int event) {
	struct connection_t *conn = node->userdata;
	struct eventpool_fd_t *tmp = NULL;
	char buffer[WEBSERVER_CHUNK_SIZE];
	int bytes = 0;

	switch(event) {
		case EV_CONNECT_SUCCESS: {
			eventpool_fd_enable_read(node);
		} break;
		case EV_READ: {
			bytes = read(node->fd, buffer, WEBSERVER_CHUNK_SIZE);
			if(bytes == 0) {
				if(conn->flags == 1) {
					conn->pull = 1;
					eventpool_fd_write(conn->fd, "0\r\n\r\n", 5);
				}
				return -1;
			}
			if(bytes < 0) {
				if(errno == EINTR || errno == EAGAIN) {
					eventpool_fd_enable_read(node);
					return 0;
				}
				perror("read");
				return -1;
			}

			if(cache == 1) {
				int match = 0;
				struct fcache_t *tmp = fcache;
				while(tmp) {
					if(strcmp(tmp->name, conn->request) == 0) {
						match = 1;
						break;
					}
					tmp = tmp->next;
				}
				struct fcache_t *node = NULL;
				if(match == 0) {
					node = MALLOC(sizeof(struct fcache_t));
					if(node == NULL) {
						OUT_OF_MEMORY
					}
					node->size = 0;
					node->bytes = NULL;
					if((node->name = MALLOC(strlen(conn->request)+1)) == NULL) {
						OUT_OF_MEMORY
					}
					strcpy(node->name, conn->request);
					node->next = fcache;
					fcache = node;
				} else {
					node = tmp;
				}
				if((node->bytes = REALLOC(node->bytes, node->size+bytes)) == NULL) {
					OUT_OF_MEMORY
				}
				memcpy(&node->bytes[node->size], buffer, bytes);
				node->size += bytes;
			}
			send_data(conn, &buffer, bytes);
			eventpool_fd_enable_read(node);
		} break;
		case EV_DISCONNECTED: {
			if(eventpool_fd_select(conn->fd, &tmp) == 0) {
				tmp->stage = EVENTPOOL_STAGE_DISCONNECT;
			}
			eventpool_fd_remove(node);
		} break;
	}
	return 0;
}

static int request_handler(struct connection_t *conn) {
	char *ext = NULL;
	char *mimetype = NULL;
	char buffer[4096], *p = buffer;

	memset(buffer, '\0', 4096);

	if(conn->is_websocket == 0) {
		if(conn->uri != NULL && strlen(conn->uri) > 0) {
			if(strcmp(conn->uri, "/send") == 0 || strcmp(conn->uri, "/control") == 0) {
				return parse_rest(conn);
			} else if(strcmp(conn->uri, "/config") == 0) {
				char media[15];
				int internal = CONFIG_USER;
				strcpy(media, "web");
				if(conn->query_string != NULL) {
					sscanf(conn->query_string, "media=%14s%*[ \n\r]", media);
					if(strstr(conn->query_string, "internal") != NULL) {
						internal = CONFIG_INTERNAL;
					}
				}
				struct JsonNode *jsend = config_print(internal, media);
				if(jsend != NULL) {
					char *output = json_stringify(jsend, NULL);
					send_data(conn, output, strlen(output));
					json_delete(jsend);
					json_free(output);
				}
				jsend = NULL;
				return MG_TRUE;
			} else if(strcmp(conn->uri, "/values") == 0) {
				char media[15];
				strcpy(media, "web");
				if(conn->query_string != NULL) {
					sscanf(conn->query_string, "media=%14s%*[ \n\r]", media);
				}
				struct JsonNode *jsend = values_print(media);
				if(jsend != NULL) {
					char *output = json_stringify(jsend, NULL);
					send_data(conn, output, strlen(output));
					json_delete(jsend);
					json_free(output);
				}
				jsend = NULL;
				return MG_TRUE;
			} else if(strcmp(&conn->uri[(rstrstr(conn->uri, "/")-conn->uri)], "/") == 0) {
				char indexes[6][255] = {"index.html","index.htm","index.shtml","index.cgi","index.php"};

				unsigned q = 0;
				/* Check if the root is terminated by a slash. If not, than add it */
				for(q=0;q<5;q++) {
					size_t l = strlen(root)+strlen(conn->uri)+strlen(indexes[q])+4;
					if((conn->request = REALLOC(conn->request, l)) == NULL) {
						OUT_OF_MEMORY
					}
					memset(conn->request, '\0', l);
					if(root[strlen(root)-1] == '/') {
#ifdef __FreeBSD__
						sprintf(conn->request, "%s/%s%s", root, conn->uri, indexes[q]);
#else
						sprintf(conn->request, "%s%s%s", root, conn->uri, indexes[q]);
#endif
					} else {
						sprintf(conn->request, "%s/%s%s", root, conn->uri, indexes[q]);
					}
					if(access(conn->request, F_OK) == 0) {
						break;
					}
				}
			} else if(root != NULL && conn->uri != NULL) {
				size_t wlen = strlen(root)+strlen(conn->uri)+2;
				if((conn->request = MALLOC(wlen)) == NULL) {
					OUT_OF_MEMORY
				}
				memset(conn->request, '\0', wlen);
				/* If a file was requested add it to the webserver path to create the absolute path */
				if(root[strlen(root)-1] == '/') {
					if(conn->uri[0] == '/')
						sprintf(conn->request, "%s%s", root, conn->uri);
					else
						sprintf(conn->request, "%s/%s", root, conn->uri);
				} else {
					if(conn->uri[0] == '/')
						sprintf(conn->request, "%s/%s", root, conn->uri);
					else
						sprintf(conn->request, "%s/%s", root, conn->uri);
				}
			}
			if(conn->request == NULL) {
				return MG_FALSE;
			}

			char *dot = NULL;
			/* Retrieve the extension of the requested file and create a mimetype accordingly */
			dot = strrchr(conn->request, '.');
			if(dot == NULL || dot == conn->request) {
				mimetype = strdup("text/plain");
			} else {
				if((ext = REALLOC(ext, strlen(dot)+1)) == NULL) {
					OUT_OF_MEMORY
				}
				memset(ext, '\0', strlen(dot)+1);
				strcpy(ext, dot+1);

				if(strcmp(ext, "html") == 0) {
					mimetype = strdup("text/html");
				} else if(strcmp(ext, "xml") == 0) {
					mimetype = strdup("text/xml");
				} else if(strcmp(ext, "png") == 0) {
					mimetype = strdup("image/png");
				} else if(strcmp(ext, "gif") == 0) {
					mimetype = strdup("image/gif");
				} else if(strcmp(ext, "ico") == 0) {
					mimetype = strdup("image/x-icon");
				} else if(strcmp(ext, "jpg") == 0) {
					mimetype = strdup("image/jpg");
				} else if(strcmp(ext, "css") == 0) {
					mimetype = strdup("text/css");
				} else if(strcmp(ext, "js") == 0) {
					mimetype = strdup("text/javascript");
				} else if(strcmp(ext, "php") == 0) {
					mimetype = strdup("application/x-httpd-php");
				} else {
					mimetype = strdup("text/plain");
				}
			}
			FREE(ext);

			memset(buffer, '\0', 4096);
			p = buffer;

			if(access(conn->request, F_OK) != 0) {
				FREE(mimetype);
				goto filenotfound;
			}

			const char *cl = NULL;
			if((cl = http_get_header(conn, "Content-Length"))) {
				if(atoi(cl) > MAX_UPLOAD_FILESIZE) {
					char line[1024] = {'\0'};
					FREE(mimetype);
					sprintf(line, "Webserver Warning: POST Content-Length of %d bytes exceeds the limit of %d bytes in Unknown on line 0", MAX_UPLOAD_FILESIZE, atoi(cl));
					webserver_create_header(&p, "200 OK", "text/plain", strlen(line));
					eventpool_fd_write(conn->fd, buffer, (int)(p-buffer));
					eventpool_fd_write(conn->fd, line, (int)strlen(line));
					FREE(conn->request);
					conn->request = NULL;
					return MG_TRUE;
				}
			}

			int match = 0;
			if(cache == 1) {
				struct fcache_t *tmp = fcache;
				while(tmp) {
					if(strcmp(tmp->name, conn->request) == 0) {
						match = 1;
						break;
					}
					tmp = tmp->next;
				}
				if(match == 1) {
					send_data(conn, tmp->bytes, tmp->size);
					eventpool_fd_write(conn->fd, "0\r\n\r\n", 5);
					return MG_TRUE;
				}
			}
			if(match == 0) {
				int fd = open(conn->request, O_RDWR | O_NONBLOCK);
				if(fd < 0) {
					goto filenotfound;
				}

				eventpool_fd_add(conn->request, fd, file_callback, NULL, conn);
			}

			FREE(mimetype);
			return MG_MORE;
		}
	} else if(websockets == 1) {
		char input[conn->content_len+1];
		strncpy(input, conn->content, conn->content_len);
		input[conn->content_len] = '\0';
		if(json_validate(input) == true) {
			struct reason_socket_received_t *data = MALLOC(sizeof(struct reason_socket_received_t));
			if(data == NULL) {
				OUT_OF_MEMORY
			}
			data->fd = conn->fd;
			if((data->buffer = MALLOC(strlen(input)+1)) == NULL) {
				OUT_OF_MEMORY
			}
			strcpy(data->buffer, input);
			strcpy(data->type, "websocket");
			eventpool_trigger(REASON_SOCKET_RECEIVED, reason_socket_received_free, data);
		}
		return MG_TRUE;
	}

filenotfound:
	logprintf(LOG_WARNING, "(webserver) could not read %s", conn->request);
	create_404_header(conn->uri, &p);
	FREE(mimetype);

	eventpool_fd_write(conn->fd, buffer, (int)(p-buffer));

	return MG_TRUE;
}

static void *broadcast(void *param) {
	if(loop == 0) {
		return NULL;
	}

	pthread_mutex_lock(&websocket_lock);
	struct websocket_clients_t *clients = websocket_clients;
	struct threadpool_tasks_t *task = param;
	char out[1025];
	int i = 0, len = 0;
	switch(task->reason) {
		case REASON_CONFIG_UPDATE: {
			struct reason_config_update_t *data = task->userdata;
			len += snprintf(out, 1024,
				"{\"origin\":\"update\",\"type\":%d,\"devices\":[",
				data->type
			);
			for(i=0;i<data->nrdev;i++) {
				len += snprintf(&out[len], 1024-len, "\"%s\",", data->devices[i]);
			}
			len += snprintf(&out[len-1], 1024-len, "],\"values\":{");
			len -= 1;
			for(i=0;i<data->nrval;i++) {
				if(data->values[i].type == JSON_NUMBER) {
					len += snprintf(&out[len], 1024-len,
						"\"%s\":%.*f,",
						data->values[i].name, data->values[i].decimals, data->values[i].number_
					);
				} else if(data->values[i].type == JSON_STRING) {
					len += snprintf(&out[len], 1024-len,
						"\"%s\":\"%s\",",
						data->values[i].name, data->values[i].string_
					);
				}
			}
			len += snprintf(&out[len-1], 1024-len, "}}");
			len -= 1;
		} break;
		case REASON_BROADCAST_CORE:
			strncpy(out, (char *)task->userdata, 1024);
			len = strlen(task->userdata);
		break;
		default:
		return NULL;
	}
	while(clients) {
		websocket_write(clients->connection, 1, out, len);
		clients = clients->next;
	}
	pthread_mutex_unlock(&websocket_lock);
	return NULL;
}

static char *skip(char **buf, const char *delimiters) {
  char *p, *begin_word, *end_word, *end_delimiters;

  begin_word = *buf;
  end_word = begin_word + strcspn(begin_word, delimiters);
  end_delimiters = end_word + strspn(end_word, delimiters);

  for(p = end_word; p < end_delimiters; p++) {
    *p = '\0';
  }

  *buf = end_delimiters;

  return begin_word;
}

static int is_valid_http_method(const char *s) {
  return !strcmp(s, "GET") || !strcmp(s, "POST") || !strcmp(s, "HEAD") ||
    !strcmp(s, "CONNECT") || !strcmp(s, "PUT") || !strcmp(s, "DELETE") ||
    !strcmp(s, "OPTIONS") || !strcmp(s, "PROPFIND") || !strcmp(s, "MKCOL");
}

static void remove_double_dots_and_double_slashes(char *s) {
  char *p = s;

  while(*s != '\0') {
    *p++ = *s++;
    if(s[-1] == '/' || s[-1] == '\\') {
      // Skip all following slashes, backslashes and double-dots
      while(s[0] != '\0') {
        if(s[0] == '/' || s[0] == '\\') {
					s++;
				} else if(s[0] == '.' && s[1] == '.') {
					s += 2;
				} else {
					break;
				}
      }
    }
  }
  *p = '\0';
}

static void parse_http_headers(char **buf, struct connection_t *c) {
	size_t i;
	c->num_headers = 0;
	for(i = 0; i < 1024; i++) {
		c->http_headers[i].name = skip(buf, ": ");
		c->http_headers[i].value = skip(buf, "\r\n");
		if(c->http_headers[i].name[0] == '\0')
			break;
		c->num_headers = i + 1;
	}
}

static int _urldecode(const char *src, int src_len, char *dst, int dst_len, int is_form_url_encoded) {
  int i, j, a, b;

#define HEXTOI(x) (isdigit(x) ? x - '0' : x - 'W')

  for(i = j = 0; i < src_len && j < dst_len - 1; i++, j++) {
    if(src[i] == '%' && i < src_len - 2 &&
       isxdigit(*(const unsigned char *)(src + i + 1)) &&
       isxdigit(*(const unsigned char *)(src + i + 2))) {
      a = tolower(*(const unsigned char *)(src + i + 1));
      b = tolower(*(const unsigned char *)(src + i + 2));
      dst[j] = (char)((HEXTOI(a) << 4) | HEXTOI(b));
      i += 2;
    } else if(is_form_url_encoded && src[i] == '+') {
      dst[j] = ' ';
    } else {
      dst[j] = src[i];
    }
  }

  dst[j] = '\0'; // Null-terminate the destination

  return i >= src_len ? j : -1;
}

int http_parse_request(char *buffer, struct connection_t *c) {
	int is_request = 0, n = 0;

	while(*buffer != '\0' && isspace(*(unsigned char *)buffer)) {
		buffer++;
  }
	c->request_method = skip(&buffer, " ");
	c->uri = skip(&buffer, " ");
	c->http_version = skip(&buffer, "\r\n");

  is_request = is_valid_http_method(c->request_method);
  if((is_request && memcmp(c->http_version, "HTTP/", 5) != 0) ||
     (!is_request && memcmp(c->request_method, "HTTP/", 5) != 0)) {
  } else {
    if(is_request == 1) {
			c->http_version += 5;
    } else {
			c->status_code = atoi(c->uri);
    }
    parse_http_headers(&buffer, c);

    if((c->query_string = strchr(c->uri, '?')) != NULL) {
      *(char *)c->query_string++ = '\0';
    }
    n = (int)strlen(c->uri);
    _urldecode(c->uri, n, (char *)c->uri, n + 1, 0);

    if(*c->uri == '/' || *c->uri == '.') {
      remove_double_dots_and_double_slashes((char *)c->uri);
    }
  }

	return 0;
}

static void websocket_client_add(struct connection_t *c) {
	pthread_mutex_lock(&websocket_lock);
	struct websocket_clients_t *node = MALLOC(sizeof(struct websocket_clients_t));
	if(node == NULL) {
		OUT_OF_MEMORY
	}
	node->connection = c;

	node->next = websocket_clients;
	websocket_clients = node;
	pthread_mutex_unlock(&websocket_lock);
}

static void websocket_client_remove(struct connection_t *c) {
	pthread_mutex_lock(&websocket_lock);
	struct websocket_clients_t *currP, *prevP;

	prevP = NULL;

	for(currP = websocket_clients; currP != NULL; prevP = currP, currP = currP->next) {
		if(currP->connection->fd == c->fd) {
			if(prevP == NULL) {
				websocket_clients = currP->next;
			} else {
				prevP->next = currP->next;
			}

			FREE(currP);
			break;
		}
	}
	pthread_mutex_unlock(&websocket_lock);
}

static void send_websocket_handshake(struct connection_t *conn, const char *key) {
  static const char *magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  unsigned char sha[20];
	char buf[500], *b64_sha = NULL, *p = (char *)sha;
	mbedtls_sha1_context ctx;

	memset(&sha, '\0', 20);

  snprintf(buf, sizeof(buf), "%s%s", key, magic);
	mbedtls_sha1_init(&ctx);
	mbedtls_sha1_starts(&ctx);
	mbedtls_sha1_update(&ctx, (unsigned char *)buf, strlen(buf));
	mbedtls_sha1_finish(&ctx, sha);
  b64_sha = base64encode(p, 20);
  int i = sprintf(buf,
              "HTTP/1.1 101 Web Socket Protocol Handshake\r\n"
              "Connection: Upgrade\r\n"
              "Upgrade: websocket\r\n"
              "Sec-WebSocket-Accept: %s\r\n\r\n", b64_sha);
  eventpool_fd_write(conn->fd, buf, i);
	mbedtls_sha1_free(&ctx);
	FREE(b64_sha);
}

static void send_websocket_handshake_if_requested(struct connection_t *conn) {
	const char *ver = http_get_header(conn, "Sec-WebSocket-Version");
	const char *key = http_get_header(conn, "Sec-WebSocket-Key");
  if(ver != NULL && key != NULL) {
		conn->is_websocket = 1;
		websocket_client_add(conn);
		send_websocket_handshake(conn, key);
  }
}

int websocket_read(struct connection_t *c, unsigned char *buf, int buf_len) {
	unsigned int i = 0, j = 0;
	unsigned char mask[4];
	unsigned int packet_length = 0;
	unsigned int length_code = 0;
	int index_first_mask = 0;
	int index_first_data_byte = 0;
	int opcode = buf[0] & 0xF;

	memset(&mask, '\0', 4);
	length_code = ((unsigned char)buf[1]) & 0x7F;
	index_first_mask = 2;
	if(length_code == 126) {
		index_first_mask = 4;
	} else if(length_code == 127) {
		index_first_mask = 10;
	}
	memcpy(mask, &buf[index_first_mask], 4);
	index_first_data_byte = index_first_mask + 4;
	packet_length = buf_len - index_first_data_byte;
	for(i = index_first_data_byte, j = 0; i < buf_len; i++, j++) {
		buf[j] = buf[i] ^ mask[j % 4];
	}
	buf[packet_length] = '\0';

	switch(opcode) {
		case WEBSOCKET_OPCODE_PONG: {
			struct timeval tv;
			gettimeofday(&tv, NULL);
			c->ping = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;
		} break;
		case WEBSOCKET_OPCODE_PING: {
			websocket_write(c, WEBSOCKET_OPCODE_PONG, NULL, 0);
		} break;
		case WEBSOCKET_OPCODE_CONNECTION_CLOSE:
			websocket_write(c, WEBSOCKET_OPCODE_CONNECTION_CLOSE, NULL, 0);
			return -1;
		break;
		case WEBSOCKET_OPCODE_TEXT:
			c->content_len = packet_length;
			c->content = (char *)buf;
			return 0;
		break;
	}

	return packet_length;
}

static int client_send(struct eventpool_fd_t *node) {
	int ret = 0;
	struct connection_t *c = NULL;
	if(node->userdata != NULL) {
		c = (struct connection_t *)node->userdata;
	}

#ifdef WEBSERVER_HTTPS
	int is_ssl = 0;
	if(c != NULL) {
		is_ssl = c->is_ssl;
	}
	if(is_ssl == 0) {
#endif
		int len = WEBSERVER_CHUNK_SIZE;
		if(node->send_iobuf.len < len) {
			len = node->send_iobuf.len;
		}
		if(node->data.socket.port > 0 && strlen(node->data.socket.ip) > 0) {
			ret = (int)sendto(node->fd, node->send_iobuf.buf, len, 0,
					(struct sockaddr *)&node->data.socket.addr, sizeof(node->data.socket.addr));
		} else {
			ret = (int)send(node->fd, node->send_iobuf.buf, len, 0);
		}
		if(c != NULL) {
			if((node->send_iobuf.len-ret) == 0) {
				c->push = 1;
			}
		}
		return ret;
#ifdef WEBSERVER_HTTPS
	} else {
		char buffer[BUFFER_SIZE];
		int len = WEBSERVER_CHUNK_SIZE;
		if(node->send_iobuf.len < len) {
			len = node->send_iobuf.len;
		}
		struct connection_t *c = (struct connection_t *)node->userdata;

		ret = mbedtls_ssl_write(&c->ssl, (unsigned char *)node->send_iobuf.buf, len);
		if(ret <= 0) {
			if(ret == MBEDTLS_ERR_NET_CONN_RESET) {
				mbedtls_strerror(ret, (char *)&buffer, BUFFER_SIZE);
				logprintf(LOG_NOTICE, "mbedtls_ssl_write failed: %s %d", buffer, node->fd);
				return -1;
			}
			if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
				mbedtls_strerror(ret, (char *)&buffer, BUFFER_SIZE);
				logprintf(LOG_NOTICE, "mbedtls_ssl_write failed: %s %d", buffer, node->fd);
				return -1;
			}
			return 0;
		} else {
			if(c != NULL) {
				if((node->send_iobuf.len-ret) == 0) {
					c->push = 1;
				}
			}
			return ret;
		}
	}
#endif
}

static int client_callback(struct eventpool_fd_t *node, int event) {
#ifdef WEBSERVER_HTTPS
	struct sockaddr_in servaddr;
	socklen_t socklen = sizeof(servaddr);
#endif
	char buffer[BUFFER_SIZE];
	int bytes = 0;
	memset(buffer, '\0', BUFFER_SIZE);

	switch(event) {
		case EV_CONNECT_SUCCESS:
			eventpool_fd_enable_read(node);
			struct connection_t *c = MALLOC(sizeof(struct connection_t));
			if(c == NULL) {
				OUT_OF_MEMORY
			}
			memset(c, 0, sizeof(struct connection_t));
			node->userdata = (void *)c;
			c->request = NULL;
			c->is_websocket = 0;
			c->fd = node->fd;
			c->flags = 0;
			c->ping = 0;
			c->push = 0;
			c->pull = 0;
#ifdef WEBSERVER_HTTPS
			c->is_ssl = 0;
			c->handshake = 0;

			if(getsockname(node->fd, (struct sockaddr *)&servaddr, &socklen) == 0) {
				if(ntohs(servaddr.sin_port) == https_port) {
					c->is_ssl = 1;
				}
			}
			if(c->is_ssl == 1) {
				if(initialize_ssl(c) == -1) {
					return -1;
				}
				mbedtls_ssl_session_reset(&c->ssl);
				mbedtls_ssl_set_bio(&c->ssl, &node->fd, mbedtls_net_send, mbedtls_net_recv, NULL);
			}
#endif
		break;
		case EV_READ: {
			struct connection_t *c = (struct connection_t *)node->userdata;
#ifdef WEBSERVER_HTTPS
			if(c->is_ssl == 0) {
#endif
				if((bytes = recv(node->fd, buffer, BUFFER_SIZE, 0)) <= 0) {
					return -1;
				}
#ifdef WEBSERVER_HTTPS
			} else {
				if(c->handshake == 0) {
					if((bytes = mbedtls_ssl_handshake(&c->ssl)) < 0) {
						if(bytes != MBEDTLS_ERR_SSL_WANT_READ && bytes != MBEDTLS_ERR_SSL_WANT_WRITE) {
							mbedtls_strerror(bytes, (char *)&buffer, BUFFER_SIZE);
							logprintf(LOG_ERR, "mbedtls_ssl_handshake failed: %s", buffer);
							return -1;
						} else {
							eventpool_fd_enable_read(node);
							return 0;
						}
					}
					c->handshake = 1;
				}
				if((bytes = mbedtls_ssl_read(&c->ssl, (unsigned char *)buffer, BUFFER_SIZE)) < 0) {
					eventpool_fd_enable_read(node);
					return 0;
				} else if(bytes == 0) {
					return -1;
				}
			}
#endif
			if(c->is_websocket == 0) {
				http_parse_request(buffer, c);
				send_websocket_handshake_if_requested(c);
				if(auth_handler(c) == MG_FALSE) {
					send_auth_request(c);
					return -1;
				}
			} else {
				unsigned char *p = (unsigned char *)buffer;
				if(websocket_read(c, p, bytes) == -1) {
					return -1;
				}
			}
			int x = request_handler(c);
			if(x == MG_MORE) {
				return 0;
			}
			if(x == MG_TRUE) {
				if(c->is_websocket == 1) {
					eventpool_fd_enable_read(node);
					return 0;
				}
				return -1;
			}
			eventpool_fd_enable_read(node);
		} break;
		case EV_DISCONNECTED: {
			if(node->userdata != NULL) {
				struct connection_t *c = (struct connection_t *)node->userdata;
				if(c->is_websocket == 1) {
					websocket_client_remove(c);
					websocket_write(c, WEBSOCKET_OPCODE_CONNECTION_CLOSE, NULL, 0);
				}
#ifdef WEBSERVER_HTTPS
				if(c->is_ssl == 1) {
					mbedtls_ssl_free(&c->ssl);
				}
#endif
				if(c->connection_param != NULL) {
					FREE(c->connection_param);
				}
				if(c->request != NULL) {
					FREE(c->request);
				}
				FREE(node->userdata);
			}
			eventpool_fd_remove(node);
		} break;
	}
	return 0;
}

static int server_callback(struct eventpool_fd_t *node, int event) {
	struct sockaddr_in servaddr;
	socklen_t socklen = sizeof(servaddr);
	char buffer[BUFFER_SIZE];
	int socket_client = 0;

	switch(event) {
		case EV_LISTEN_SUCCESS: {
			static struct linger linger = { 0, 0 };
#ifdef _WIN32
			unsigned int lsize = sizeof(struct linger);
#else
			socklen_t lsize = sizeof(struct linger);
#endif
			setsockopt(node->fd, SOL_SOCKET, SO_LINGER, (void *)&linger, lsize);

			if(getsockname(node->fd, (struct sockaddr *)&servaddr, (socklen_t *)&socklen) == -1) {
				perror("getsockname");
			} else {
#ifdef WEBSERVER_HTTPS
				if(node->data.socket.port == https_port) {
					logprintf(LOG_INFO, "secured webserver started on port: %d", ntohs(servaddr.sin_port));
				} else {
#endif
					logprintf(LOG_INFO, "regular webserver started on port: %d", ntohs(servaddr.sin_port));
#ifdef WEBSERVER_HTTPS
				}
#endif
			}
		} break;
		case EV_READ: {
			if((socket_client = accept(node->fd, (struct sockaddr *)&servaddr, (socklen_t *)&socklen)) < 0) {
				logprintf(LOG_NOTICE, "failed to accept client");
				return -1;
			}

			memset(&buffer, '\0', INET_ADDRSTRLEN+1);
			inet_ntop(AF_INET, (void *)&(servaddr.sin_addr), buffer, INET_ADDRSTRLEN+1);

			if(whitelist_check(buffer) != 0) {
				logprintf(LOG_INFO, "rejected client, ip: %s, port: %d", buffer, ntohs(servaddr.sin_port));
				eventpool_fd_remove(node);
			} else {
				logprintf(LOG_DEBUG, "new client, ip: %s, port: %d", buffer, ntohs(servaddr.sin_port));
				logprintf(LOG_DEBUG, "client fd: %d", socket_client);

				struct reason_webserver_connected_t *data = MALLOC(sizeof(struct reason_webserver_connected_t));
				if(data == NULL) {
					OUT_OF_MEMORY
				}
				data->fd = socket_client;

				eventpool_trigger(REASON_WEBSERVER_CONNECTED, reason_webserver_connected_free, data);

				eventpool_fd_add("webserver client", socket_client, client_callback, client_send, NULL);
			}
		} break;
		case EV_BIND_FAILED: {
			int x = 0;
			if((x = getpeername(node->fd, (struct sockaddr *)&servaddr, &socklen)) == 0) {
				logprintf(LOG_ERR, "cannot bind to webserver port %d, address already in use?", ntohs(servaddr.sin_port));
			} else {
				logprintf(LOG_ERR, "cannot bind to webserver port, address already in use?");
			}
			return -1;
		} break;
		case EV_DISCONNECTED: {
			if(node->userdata != NULL) {
				struct connection_t *c = (struct connection_t *)node->userdata;
				if(c->is_websocket == 1) {
					websocket_write(c, WEBSOCKET_OPCODE_CONNECTION_CLOSE, NULL, 0);
				}
			}
			eventpool_fd_remove(node);
		} break;
	}
	return 0;
}

static void *adhoc_mode(void *param) {
	if(https_server != NULL) {
		eventpool_fd_remove(https_server);
		logprintf(LOG_INFO, "shut down secure webserver due to adhoc mode");
		https_server = NULL;
	}
	if(http_server != NULL) {
		eventpool_fd_remove(http_server);
		logprintf(LOG_INFO, "shut down regular webserver due to adhoc mode");
		http_server = NULL;
	}

	return NULL;
}

void *webserver_restart(void *param) {
	if(https_server != NULL) {
		eventpool_fd_remove(https_server);
		https_server = NULL;
	}
	if(http_server != NULL) {
		eventpool_fd_remove(http_server);
		http_server = NULL;
	}

#ifdef WEBSERVER_HTTPS
	if(https_port > 0) {
		if(ssl_server_init_status() == 0) {
			https_server = eventpool_socket_add("webserver https", NULL, https_port, AF_INET, SOCK_STREAM, 0, EVENTPOOL_TYPE_SOCKET_SERVER, server_callback, client_send, NULL);
		} else {
			logprintf(LOG_NOTICE, "could not initialize mbedtls library, secure webserver not started");
		}
	}
#endif

	if(http_port > 0) {
		http_server = eventpool_socket_add("webserver http", NULL, http_port, AF_INET, SOCK_STREAM, 0, EVENTPOOL_TYPE_SOCKET_SERVER, server_callback, client_send, NULL);
	}

	return NULL;
}

int webserver_start(void) {
	double itmp = 0.0;
	int webserver_enabled = 0;

	pthread_mutexattr_init(&websocket_attr);
	pthread_mutexattr_settype(&websocket_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&websocket_lock, &websocket_attr);

	if(settings_select_number(ORIGIN_WEBSERVER, "webserver-http-port", &itmp) == 0) { http_port = (int)itmp; }
	if(settings_select_number(ORIGIN_WEBSERVER, "webgui-websockets", &itmp) == 0) { websockets = (int)itmp; }
	if(settings_select_number(ORIGIN_WEBSERVER, "webserver-cache", &itmp) == 0) { cache = (int)itmp; }
	if(settings_select_number(ORIGIN_WEBSERVER, "webserver-enable", &itmp) == 0) { webserver_enabled = (int)itmp; }
#ifdef WEBSERVER_HTTPS
	if(settings_select_number(ORIGIN_WEBSERVER, "webserver-https-port", &itmp) == 0) { https_port = (int)itmp; }
#endif

	if(settings_select_string(ORIGIN_WEBSERVER, "webserver-root", &root) != 0) {
		if((root = MALLOC(strlen(WEBSERVER_ROOT)+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy(root, WEBSERVER_ROOT);
		root_free = 1;
	}

	settings_select_string_element(ORIGIN_WEBSERVER, "webserver-authentication", 0, &authentication_username);
	settings_select_string_element(ORIGIN_WEBSERVER, "webserver-authentication", 1, &authentication_password);

	eventpool_callback(REASON_CONFIG_UPDATE, broadcast);
	eventpool_callback(REASON_BROADCAST_CORE, broadcast);
	eventpool_callback(REASON_ADHOC_CONNECTED, adhoc_mode);
	eventpool_callback(REASON_ADHOC_DISCONNECTED, webserver_restart);
	eventpool_callback(REASON_SOCKET_SEND, webserver_send);

#ifdef WEBSERVER_HTTPS
	if(https_port > 0 && webserver_enabled == 1) {
		if(ssl_server_init_status() == 0) {
			https_server = eventpool_socket_add("webserver https", NULL, https_port, AF_INET, SOCK_STREAM, 0, EVENTPOOL_TYPE_SOCKET_SERVER, server_callback, client_send, NULL);
		} else {
			logprintf(LOG_NOTICE, "could not initialize mbedtls library, secure webserver not started");
		}
	}
#endif

	if(http_port > 0 && webserver_enabled == 1) {
		http_server = eventpool_socket_add("webserver http", NULL, http_port, AF_INET, SOCK_STREAM, 0, EVENTPOOL_TYPE_SOCKET_SERVER, server_callback, client_send, NULL);
	}

	return 0;
}
