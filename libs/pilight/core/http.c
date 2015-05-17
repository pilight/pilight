/*
	Copyright (C) 2013 - 2014 CurlyMo

	This file is part of pilight.

	pilight is FREE software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the FREE Software
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
#include "../../polarssl/polarssl/ssl.h"
#include "../../polarssl/polarssl/entropy.h"
#include "../../polarssl/polarssl/ctr_drbg.h"

#define USERAGENT			"pilight"
#define HTTP_POST			1
#define HTTP_GET			0

char *http_process_request(char *url, int method, char **type, int *code, int *size, const char *contype, char *post) {
	struct sockaddr_in serv_addr;
	int sockfd = 0, bytes = 0;
	int has_code = 0, has_type = 0;
	int pos = 0;
	size_t bufsize = BUFFER_SIZE;
	char ip[INET_ADDRSTRLEN+1], *content = NULL, *host = NULL, *auth = NULL, *auth64 = NULL;
	char *page = NULL, *tok = NULL;
	char recvBuff[BUFFER_SIZE+1], *header = MALLOC(bufsize);
	unsigned short port = 0, sslfree = 0, entropyfree = 0;
	size_t len = 0, tlen = 0, plen = 0;

	entropy_context entropy;
	ctr_drbg_context ctr_drbg;
	ssl_context ssl;

	*size = 0;

	if(header == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	
	memset(header, '\0', bufsize);
	memset(recvBuff, '\0', BUFFER_SIZE+1);
	memset(&serv_addr, '\0', sizeof(struct sockaddr_in));

	/* Check which port we need to use based on the http(s) protocol */
	if(strncmp(url, "http://", 7) == 0) {
		port = 80;
		plen = 8;
	} else if(strncmp(url, "https://", 8) == 0) {
		port = 443;
		plen = 9;
	} else {
		logprintf(LOG_ERR, "an url should start with either http:// or https://", url);
		*code = -1;
		goto exit;
	}

	/* Split the url into a host and page part */
	len = strlen(url);
	if((tok = strstr(&url[plen], "/"))) {
		tlen = (size_t)(tok-url)-plen+1;
		if((host = MALLOC(tlen+1)) == NULL) {
			fprintf(stderr, "out of memory\n");
			exit(EXIT_FAILURE);
		}
		strncpy(host, &url[plen-1], tlen);
		host[tlen] = '\0';
		if((page = MALLOC(len-tlen)) == NULL) {
			fprintf(stderr, "out of memory\n");
			exit(EXIT_FAILURE);
		}		
		strcpy(page, &url[tlen+(plen-1)]);
	} else {
		tlen = strlen(url)-(plen-1);
		if((host = MALLOC(tlen+1)) == NULL) {
			fprintf(stderr, "out of memory\n");
			exit(EXIT_FAILURE);
		}
		strncpy(host, &url[(plen-1)], tlen);
		host[tlen] = '\0';
		if((page = MALLOC(2)) == NULL) {
			fprintf(stderr, "out of memory\n");
			exit(EXIT_FAILURE);
		}
		strcpy(page, "/");
	}
	if((tok = strstr(host, "@"))) {
		size_t pglen = strlen(page);
		if(strcmp(page, "/") == 0) {
			pglen -= 1;
		}
		tlen = (size_t)(tok-host);
		if((auth = MALLOC(tlen+1)) == NULL) {
			fprintf(stderr, "out of memory\n");
			exit(EXIT_FAILURE);
		}
		strncpy(auth, &host[0], tlen);
		auth[tlen] = '\0';
		strncpy(&host[0], &url[plen+tlen], len-(plen+tlen+pglen));
		host[len-(plen+tlen+pglen)] = '\0';
		auth64 = base64encode(auth, strlen(auth));
	}

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(0x202, &wsa) != 0) {
		logprintf(LOG_ERR, "could not initialize new socket");
		*code = -1;
		goto exit;
	}
#endif

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		logprintf(LOG_ERR, "could not http create socket");
		*code = -1;
		goto exit;
	}
  setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, 0, 0);

	char *w = ip;
	if(host2ip(host, w) == -1) {
		*code = -1;
		goto exit;
	}

	serv_addr.sin_family = AF_INET;
	if(inet_pton(AF_INET, ip, (void *)(&(serv_addr.sin_addr.s_addr))) <= 0) {
		logprintf(LOG_ERR, "%s is not a valid ip address", ip);
		*code = -1;
		goto exit;
	}
	serv_addr.sin_port = htons(port);

	/* Proper socket timeout testing */
	switch(socket_timeout_connect(sockfd, (struct sockaddr *)&serv_addr, 3)) {
		case -1:
			logprintf(LOG_ERR, "could not connect to http socket (%s)", url);
			*code = -1;
			goto exit;
		case -2:
			logprintf(LOG_ERR, "http socket connection timeout (%s)", url);
			*code = -1;
			goto exit;
		case -3:
			logprintf(LOG_ERR, "error in http socket connection", url);
			*code = -1;
			goto exit;
		default:
		break;
	}

	if(method == HTTP_POST) {
		len = (size_t)snprintf(&header[0], bufsize, "POST %s HTTP/1.0\r\n", page);
		if(len >= bufsize) {
			bufsize += BUFFER_SIZE;
			if((header = REALLOC(header, bufsize)) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
		}
		len += (size_t)snprintf(&header[len], bufsize - len, "Host: %s\r\n", host);
		if(len >= bufsize) {
			bufsize += BUFFER_SIZE;
			if((header = REALLOC(header, bufsize)) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
		}
		if(auth64 != NULL) {
			len += (size_t)snprintf(&header[len], bufsize - len, "Authorization: Basic %s\r\n", auth64);
			if(len >= bufsize) {
				bufsize += BUFFER_SIZE;
				if((header = REALLOC(header, bufsize)) == NULL) {
					fprintf(stderr, "out of memory\n");
					exit(EXIT_FAILURE);
				}
			}
		}
		len += (size_t)snprintf(&header[len], bufsize - len, "User-Agent: %s\r\n", USERAGENT);
		if(len >= bufsize) {
			bufsize += BUFFER_SIZE;
			if((header = REALLOC(header, bufsize)) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
		}
		len += (size_t)snprintf(&header[len], bufsize - len, "Content-Type: %s\r\n", contype);
		if(len >= bufsize) {
			bufsize += BUFFER_SIZE;
			if((header = REALLOC(header, bufsize)) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
		}
		len += (size_t)snprintf(&header[len], bufsize - len, "Content-Length: %d\r\n\r\n", (int)strlen(post));
		if(len >= bufsize) {
			bufsize += BUFFER_SIZE;
			if((header = REALLOC(header, bufsize)) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
		}
		len += (size_t)snprintf(&header[len], bufsize - len, "%s", post);
		if(len >= bufsize) {
			bufsize += BUFFER_SIZE;
			if((header = REALLOC(header, bufsize)) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
		}
	} else if(method == HTTP_GET) {
		len = (size_t)snprintf(&header[0], bufsize, "GET %s HTTP/1.0\r\n", page);
		if(len >= bufsize) {
			bufsize += BUFFER_SIZE;
			if((header = REALLOC(header, bufsize)) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
		}
		len += (size_t)snprintf(&header[len], bufsize - len, "Host: %s\r\n", host);
		if(len >= bufsize) {
			bufsize += BUFFER_SIZE;
			if((header = REALLOC(header, bufsize)) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
		}
		if(auth64 != NULL) {
			len += (size_t)snprintf(&header[len], bufsize - len, "Authorization: Basic %s\r\n", auth64);
			if(len >= bufsize) {
				bufsize += BUFFER_SIZE;
				if((header = REALLOC(header, bufsize)) == NULL) {
					fprintf(stderr, "out of memory\n");
					exit(EXIT_FAILURE);
				}
			}
		}
		len += (size_t)snprintf(&header[len], bufsize - len, "User-Agent: %s\r\n", USERAGENT);
		if(len >= bufsize) {
			bufsize += BUFFER_SIZE;
			if((header = REALLOC(header, bufsize)) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
		}
		len += (size_t)snprintf(&header[len], bufsize - len, "Connection: close\r\n\r\n");
		if(len >= bufsize) {
			bufsize += BUFFER_SIZE;
			if((header = REALLOC(header, bufsize)) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
		}
	}

	if(port == 443) {
		memset(&ssl, '\0', sizeof(ssl_context));
		entropy_init(&entropy);
		entropyfree = 1;
		if((ctr_drbg_init(&ctr_drbg, entropy_func, &entropy, (const unsigned char *)USERAGENT, 6)) != 0) {
			logprintf(LOG_ERR, "ctr_drbg_init failed");
			*code = -1;
			goto exit;
		}

		if((ssl_init(&ssl)) != 0) {
			logprintf(LOG_ERR, "ssl_init failed");
			*code = -1;
			goto exit;
		}
		sslfree = 1;

		ssl_set_endpoint(&ssl, SSL_IS_CLIENT);
		ssl_set_rng(&ssl, ctr_drbg_random, &ctr_drbg);
		ssl_set_bio(&ssl, net_recv, &sockfd, net_send, &sockfd);

		int ret = 0;
		while((ret = ssl_handshake(&ssl)) != 0) {
			if(ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE) {
				logprintf(LOG_ERR, "ssl_handshake failed");
				*code = -1;
				goto exit;
			}
		}

		while((ret = ssl_write(&ssl, (const unsigned char *)header, len)) <= 0) {
			if(ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE) {
				if(ret == -76) {
					logprintf(LOG_ERR, "ssl_write timed out");
				} else {
					logprintf(LOG_ERR, "ssl_write failed");
				}
				*code = -1;
				goto exit;
			}
		}
	} else {
		if((bytes = send(sockfd, header, len, 0)) <= 0) {
			logprintf(LOG_ERR, "sending header to http server failed");
			*code = -1;
			goto exit;
		}
	}

	char *nl = NULL;
	char *tp = *type;
	memset(recvBuff, '\0', sizeof(recvBuff));

	while(1) {
		if(port == 443) {
			bytes = ssl_read(&ssl, (unsigned char *)recvBuff, BUFFER_SIZE);
			if(bytes == POLARSSL_ERR_NET_WANT_READ || bytes == POLARSSL_ERR_NET_WANT_WRITE) {
				continue;
			}
			if(bytes == POLARSSL_ERR_SSL_PEER_CLOSE_NOTIFY) {
				break;
			}
		} else {
			bytes = recv(sockfd, recvBuff, BUFFER_SIZE, 0);
		}

		if(bytes <= 0) {
			if(*size == 0) {
				logprintf(LOG_ERR, "http(s) read failed (%s)", url);
			}
			break;
		}

		if((content = REALLOC(content, (size_t)(*size+bytes+1))) == NULL) {
			fprintf(stderr, "out of memory\n");
			exit(EXIT_FAILURE);
		}

		memset(&content[*size], '\0', (size_t)(bytes+1));
		strncpy(&content[*size], recvBuff, (size_t)(bytes));
		*size += bytes;

		char **array = NULL;
		char *p = recvBuff;
		/* Let's first normalize the HEADER terminator */
		str_replace("\r\n", "\n\r", &p);
		unsigned int n = explode(recvBuff, "\n\r", &array), q = 0;
		int z = 0;
		for(q=0;q<n;q++) {
			if(has_code == 0 && sscanf(array[q], "HTTP/1.%d%*[ ]%d%*s%*[ \n\r]", &z, code)) {
				has_code = 1;
			}
			// ;%*[ A-Za-z0-9\\/=+- \n\r]
			if(has_type == 0 && sscanf(array[q], "Content-%*[tT]ype:%*[ ]%[A-Za-z\\/+-]", tp)) {
				has_type = 1;
			}
		}
		array_free(&array, n);
		memset(recvBuff, '\0', sizeof(recvBuff));
	}
	if(content != NULL) {
		/* Remove the header */
		if((nl = strstr(content, "\r\n\r\n"))) {
			pos = (nl-content)+4;
			memmove(&content[0], &content[pos], (size_t)(*size-pos));
			*size-=pos;
		}
		/* Remove the footer */
		if((nl = strstr(content, "0\r\n\r\n"))) {
			*size -= 5;
		}
	}

exit:
	if(port == 443) {
		if(sslfree == 1) {
			ssl_free(&ssl);
		}
		if(entropyfree == 1) {
			entropy_free(&entropy);
		}
	}
	if(header) FREE(header);
	if(auth) FREE(auth);
	if(auth64) FREE(auth64);
	if(page) FREE(page);
	if(host) FREE(host);
	if(sockfd > 0) {
		close(sockfd);
	}

	if(*size > 0) {
		content[*size] = '\0';
		return content;
	} else {
		return NULL;
	}
	return NULL;
}

char *http_get_content(char *url, char **type, int *code, int *size) {
	return http_process_request(url, HTTP_GET, type, code, size, NULL, NULL);
}

char *http_post_content(char *url, char **type, int *code, int *size, const char *contype, char *post) {
	return http_process_request(url, HTTP_POST, type, code, size, contype, post);
}
