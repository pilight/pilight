/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#define MSG_NOSIGNAL 0
#else
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <netdb.h>
	#include <arpa/inet.h>
#endif

#include "pilight.h"
#include "socket.h"
#include "log.h"
#include "network.h"
#include "webserver.h"
#include "ssl.h"
#include "../../mbedtls/mbedtls/ssl.h"
#include "../../mbedtls/mbedtls/error.h"

#define USERAGENT			"pilight"
#define HTTP_POST			1
#define HTTP_GET			0

typedef struct request_t {
	char *host;
  char *uri;
  char *query_string;
	char *auth64;
	int port;
	int status_code;
	int reading;
	void *userdata;

	int request_method;

  char *content;
	char mimetype[255];
  size_t content_len;
	size_t bytes_read;

	int is_ssl;
	int handshake;
	mbedtls_ssl_context ssl;
	void (*callback)(int code, char *data, int size, char *type, void *userdata);
} request_t;

static int prepare_request(struct request_t **request, int method, char *url, const char *contype, char *post, void (*callback)(int, char *, int, char *, void *), void *userdata) {
	char *tok = NULL, *auth = NULL;
	int plen = 0, len = 0, tlen = 0;

	if(((*request) = MALLOC(sizeof(struct request_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset((*request), 0, sizeof(struct request_t));
	(*request)->request_method = method;
	/* Check which port we need to use based on the http(s) protocol */
	if(strncmp(url, "http://", 7) == 0) {
		(*request)->port = 80;
		plen = 8;
	} else if(strncmp(url, "https://", 8) == 0) {
		(*request)->port = 443;
		plen = 9;
		if(ssl_client_init_status() == -1) {
			logprintf(LOG_ERR, "HTTPS URL's require a properly initialized SSL library");
			return -1;
		}
	} else {
		logprintf(LOG_ERR, "A URL should start with either http:// or https://");
		FREE((*request));
		return -1;
	}

	/* Split the url into a host and page part */
	len = strlen(url);
	if((tok = strstr(&url[plen], "/"))) {
		tlen = (size_t)(tok-url)-plen+1;
		if(((*request)->host = MALLOC(tlen+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strncpy((*request)->host, &url[plen-1], tlen);
		(*request)->host[tlen] = '\0';
		if(((*request)->uri = MALLOC(len-tlen)) == NULL) {
			OUT_OF_MEMORY
		}
		strncpy((*request)->uri, &url[tlen+(plen-1)], (len-tlen)-1);
	} else {
		tlen = strlen(url)-(plen-1);
		if(((*request)->host = MALLOC(tlen+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strncpy((*request)->host, &url[(plen-1)], tlen);
		(*request)->host[tlen] = '\0';
		if(((*request)->uri = MALLOC(2)) == NULL) {
			OUT_OF_MEMORY
		}
		strncpy((*request)->uri, "/", 1);
	}
	if((tok = strstr((*request)->host, "@"))) {
		size_t pglen = strlen((*request)->uri);
		if(strcmp((*request)->uri, "/") == 0) {
			pglen -= 1;
		}
		tlen = (size_t)(tok-(*request)->host);
		if((auth = MALLOC(tlen+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strncpy(auth, &(*request)->host[0], tlen);
		auth[tlen] = '\0';
		strncpy(&(*request)->host[0], &url[plen+tlen], len-(plen+tlen+pglen));
		(*request)->host[len-(plen+tlen+pglen)] = '\0';
		(*request)->auth64 = base64encode(auth, strlen(auth));
		FREE(auth);
	}
	if(method == HTTP_POST) {
		strncpy((*request)->mimetype, contype, sizeof((*request)->mimetype));
		if(((*request)->content = REALLOC((*request)->content, strlen(post)+1)) == NULL) {
			OUT_OF_MEMORY
		}
		strcpy((*request)->content, post);
		(*request)->content_len = strlen(post);
	}
	(*request)->userdata = userdata;
	(*request)->callback = callback;
	return 0;
}

static void append_to_header(char **header, char *data, ...) {
	va_list ap, apcpy;
	int pos = 0, bytes = 0;
	if(*header != NULL) {
		pos = strlen(*header);
	}
	va_copy(apcpy, ap);
	va_start(apcpy, data);
#ifdef _WIN32
	bytes = _vscprintf(data, apcpy);
#else
	bytes = vsnprintf(NULL, 0, data, apcpy);
#endif
	if(bytes == -1) {
		fprintf(stderr, "ERROR: improperly formatted logprintf message %s\n", data);
	} else {
		va_end(apcpy);
		if((*header = REALLOC(*header, pos+bytes+1)) == NULL) {
			OUT_OF_MEMORY
		}
		va_start(ap, data);
		vsprintf(&(*header)[pos], data, ap);
		va_end(ap);
	}
}

static int client_send(struct eventpool_fd_t *node) {
	int ret = 0;
	int is_ssl = 0;
	if(node->userdata != NULL) {
		struct request_t *c = (struct request_t *)node->userdata;
		is_ssl = c->is_ssl;
	}
	if(is_ssl == 0) {
		if(node->data.socket.port > 0 && strlen(node->data.socket.ip) > 0) {
			ret = (int)sendto(node->fd, node->send_iobuf.buf, node->send_iobuf.len, 0,
					(struct sockaddr *)&node->data.socket.addr, sizeof(node->data.socket.addr));
		} else {
			ret = (int)send(node->fd, node->send_iobuf.buf, node->send_iobuf.len, 0);
		}
		return ret;
	} else {
		char buffer[BUFFER_SIZE];
		struct request_t *c = (struct request_t *)node->userdata;
		int len = WEBSERVER_CHUNK_SIZE;
		if(node->send_iobuf.len < len) {
			len = node->send_iobuf.len;
		}

		if(c->handshake == 0) {
			if((ret = mbedtls_ssl_handshake(&c->ssl)) < 0) {
				if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
					mbedtls_strerror(ret, (char *)&buffer, BUFFER_SIZE);
					logprintf(LOG_ERR, "mbedtls_ssl_handshake failed: %s", buffer);
					return -1;
				} else {
					return 0;
				}
			}
			c->handshake = 1;
		}

		ret = mbedtls_ssl_write(&c->ssl, (unsigned char *)node->send_iobuf.buf, len);
		if(ret <= 0) {
			if(ret == MBEDTLS_ERR_NET_CONN_RESET) {
				mbedtls_strerror(ret, (char *)&buffer, BUFFER_SIZE);
				logprintf(LOG_NOTICE, "mbedtls_ssl_write failed: %s", buffer);
				return -1;
			}
			if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
				mbedtls_strerror(ret, (char *)&buffer, BUFFER_SIZE);
				logprintf(LOG_NOTICE, "mbedtls_ssl_write failed: %s", buffer);
				return -1;
			}
			return 0;
		} else {
			return ret;
		}
	}
}

static int client_callback(struct eventpool_fd_t *node, int event) {
	char *header = NULL, buffer[BUFFER_SIZE], *data = NULL;
	int bytes = 0, n = 0;
	memset(&buffer, '\0', BUFFER_SIZE);

	struct request_t *request = (struct request_t *)node->userdata;
	switch(event) {
		case EV_CONNECT_SUCCESS: {
			if(request->port == 443) {
				request->is_ssl = 1;
				int ret = 0;

				if((ret = mbedtls_ssl_setup(&request->ssl, &ssl_client_conf)) != 0) {
					mbedtls_strerror(ret, (char *)&buffer, BUFFER_SIZE);
					logprintf(LOG_ERR, "mbedtls_ssl_setup failed: %s", buffer);
					return -1;
				}

				mbedtls_ssl_session_reset(&request->ssl);
				mbedtls_ssl_set_bio(&request->ssl, &node->fd, mbedtls_net_send, mbedtls_net_recv, NULL);
			}

			if(request->request_method == HTTP_POST) {
				append_to_header(&header, "POST %s HTTP/1.0\r\n", request->uri);
				append_to_header(&header, "Host: %s\r\n", request->host);
				if(request->auth64 != NULL) {
					append_to_header(&header, "Authorization: Basic %s\r\n", request->auth64);
				}
				append_to_header(&header, "User-Agent: %s\r\n", USERAGENT);
				append_to_header(&header, "Content-Type: %s\r\n", request->mimetype);
				append_to_header(&header, "Content-Length: %lu\r\n\r\n", request->content_len);
				append_to_header(&header, "%s", request->content);
			} else if(request->request_method == HTTP_GET) {
				append_to_header(&header, "GET %s HTTP/1.0\r\n", request->uri);
				append_to_header(&header, "Host: %s\r\n", request->host);
				if(request->auth64 != NULL) {
					append_to_header(&header, "Authorization: Basic %s\r\n", request->auth64);
				}
				append_to_header(&header, "User-Agent: %s\r\n", USERAGENT);
				append_to_header(&header, "Connection: close\r\n\r\n");
			}

			eventpool_fd_enable_write(node);
			eventpool_fd_write(node->fd, header, strlen(header));
			if(header != NULL) {
				FREE(header);
			}
			eventpool_fd_enable_read(node);
		} break;
		case EV_READ: {
			if(request->is_ssl == 1) {
				bytes = mbedtls_ssl_read(&request->ssl, (unsigned char *)buffer, BUFFER_SIZE);

				if(bytes == MBEDTLS_ERR_SSL_WANT_READ || bytes == MBEDTLS_ERR_SSL_WANT_WRITE) {
					eventpool_fd_enable_read(node);
					return 0;
				}
				if(bytes == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || bytes <= 0) {
					mbedtls_strerror(bytes, (char *)&buffer, BUFFER_SIZE);
					logprintf(LOG_ERR, "mbedtls_ssl_read failed: %s", buffer);
					return -1;
				}

				buffer[bytes] = '\0';
			} else {
				if((bytes = recv(node->fd, buffer, BUFFER_SIZE, 0)) <= 0) {
					return -1;
				}
			}

			char **array = NULL;
			if(request->reading == 1) {
				data = buffer;
				if((request->content = REALLOC(request->content, request->bytes_read+bytes+1)) == NULL) {
					OUT_OF_MEMORY
				}
				strncpy(&request->content[request->bytes_read], data, request->bytes_read+bytes);
				request->bytes_read += bytes;
			} else {
				if((n = explode(buffer, "\r\n\r\n", &array)) == 2 || n == 1) {
					struct connection_t c;
					char *location = NULL;
					const char *p = location;
					http_parse_request(array[0], &c);
					if(c.status_code == 301 && (p = http_get_header(&c, "Location")) != NULL) {
						if(request->callback != NULL) {
							request->callback(c.status_code, location, strlen(location), NULL, request->userdata);
						}
						array_free(&array, n);
						return -1;
					}
					request->status_code = c.status_code;
					if(http_get_header(&c, "Content-Type") != NULL) {
						int len = 0, i = 0;
						strcpy(request->mimetype, http_get_header(&c, "Content-Type"));
						len = strlen(request->mimetype);
						for(i=0;i<len;i++) {
							if(request->mimetype[i] == ';') {
								request->mimetype[i] = '\0';
								break;
							}
						}
					}
					if(http_get_header(&c, "Content-Length") != NULL) {
						request->content_len = atoi(http_get_header(&c, "Content-Length"));
					}
					if(n == 2) {
						request->bytes_read = strlen(array[1]);
						data = array[1];
						if((request->content = REALLOC(request->content, request->bytes_read+1)) == NULL) {
							OUT_OF_MEMORY
						}
						strncpy(request->content, data, request->bytes_read);
					}
				}
			}

			if(request->bytes_read < request->content_len) {
				request->reading = 1;
				eventpool_fd_enable_read(node);
			} else if(request->content != NULL) {
				/* Remove the header */
				int pos = 0;
				char *nl = NULL;
				if((nl = strstr(request->content, "\r\n\r\n"))) {
					pos = (nl-request->content)+4;
					memmove(&request->content[0], &request->content[pos], (size_t)(request->content_len-pos));
					request->content_len -= pos;
				}
				/* Remove the footer */
				if((nl = strstr(request->content, "0\r\n\r\n"))) {
					request->content_len -= 5;
				}
				request->content[request->content_len] = '\0';
				if(request->callback != NULL) {
					request->callback(request->status_code, request->content, request->content_len, request->mimetype, request->userdata);
				}
			}
			array_free(&array, n);
		} break;
		case EV_DISCONNECTED: {
			if(request->host != NULL) {
				FREE(request->host);
			}
			if(request->uri != NULL) {
				FREE(request->uri);
			}
			if(request->content != NULL) {
				FREE(request->content);
			}
			if(request->auth64 != NULL) {
				FREE(request->auth64);
			}
			if(request->is_ssl == 1) {
				mbedtls_ssl_free(&request->ssl);
			}
			FREE(request);
			eventpool_fd_remove(node);
		} break;
	}
	return 0;
}

char *http_post_content(char *url, const char *contype, char *post, void (*callback)(int, char *, int, char *, void *), void *userdata) {
	struct request_t *request = NULL;
	if(prepare_request(&request, HTTP_POST, url, contype, post, callback, userdata) == 0) {
		eventpool_socket_add("http post", request->host, request->port, AF_INET, SOCK_STREAM, 0, EVENTPOOL_TYPE_SOCKET_CLIENT, client_callback, client_send, request);
	}
	return NULL;
}

char *http_get_content(char *url, void (*callback)(int, char *, int, char *, void *), void *userdata) {
	struct request_t *request = NULL;
	if(prepare_request(&request, HTTP_GET, url, NULL, NULL, callback, userdata) == 0) {
		eventpool_socket_add("http get", request->host, request->port, AF_INET, SOCK_STREAM, 0, EVENTPOOL_TYPE_SOCKET_CLIENT, client_callback, client_send, request);
	}
	return NULL;
}
