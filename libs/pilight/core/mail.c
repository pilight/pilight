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
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#ifdef _WIN32
	#if _WIN32_WINNT < 0x0501
		#undef _WIN32_WINNT
		#define _WIN32_WINNT 0x0501
	#endif
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

#include "socket.h"
#include "../../polarssl/polarssl/ssl.h"
#include "../../polarssl/polarssl/entropy.h"
#include "../../polarssl/polarssl/ctr_drbg.h"

#include "network.h"
#include "log.h"
#include "mail.h"

#define USERAGENT			"pilight"

#define UNSUPPORTED		0
#define AUTHPLAIN			1
#define STARTTLS			2

static int have_ssl = 0;
static int sockfd = 0;
static ssl_context ssl;

static size_t sd_read(char *buffer) {
	char c = 0;
	size_t l = 0;
	int isEOL = 0, isEOF = 0;

	while(isEOL == 0 && isEOF == 0) {
			if((have_ssl == 0 && recv(sockfd, &c, 1, 0) < 1)
				 || (have_ssl == 1 && ssl_read(&ssl, (unsigned char *)&c, 1) < 1)) {
				isEOF = 1;
			} else if(c == '\n') {
					isEOL = 1;
			} else if( c != '\r') {
					buffer[l] = c;
			}
			l++;
	}
	buffer[l] = '\0';
	return l;
}

static size_t sd_write(char *content) {
	size_t len = strlen(content);

	if(have_ssl == 0) {
		return (write(sockfd, content, len) != len);
	} else {
		return (int)ssl_write(&ssl, (unsigned char *)content, (size_t)len) != len;
	}
}

int sendmail(char *host, char *login, char *pass, unsigned short port, struct mail_t *mail) {
	struct sockaddr_in serv_addr;
	char recvBuff[BUFFER_SIZE], *out = NULL, ip[INET_ADDRSTRLEN+1], testme[256], ch = 0;
	int authtype = UNSUPPORTED, entropyfree = 0, sslfree = 0, error = 0, val = 0;
	size_t len = 0;
	entropy_context entropy;
	ctr_drbg_context ctr_drbg;

	memset(&recvBuff, '\0', BUFFER_SIZE);
	memset(&testme, '\0', 256);
	memset(&ip, '\0', INET_ADDRSTRLEN+1);
	memset(&serv_addr, '\0', sizeof(serv_addr));
	memset(&ssl, '\0', sizeof(ssl_context));

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		logprintf(LOG_ERR, "could not initialize new socket");
		exit(EXIT_FAILURE);
	}
#endif

	/* Try to open a new socket */
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		logprintf(LOG_ERR, "could not create socket");
		return -1;
	}

	switch(port) {
		case 465:
			have_ssl = 1;
		break;
		case 25:
		case 587:
		default:
			have_ssl = 0;
		break;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);

	char *p = ip;
	if(host2ip(host, p) == -1) {
		logprintf(LOG_NOTICE, "SMTP: couldn't resolve smpt host name \"%s\"", host);
		error = -1;
		goto close;
	}

	inet_pton(AF_INET, ip, &serv_addr.sin_addr);
	switch(socket_timeout_connect(sockfd, (struct sockaddr *)&serv_addr, 3)) {
		case -1:
			logprintf(LOG_NOTICE, "could not connect to mail server @%s:%d", host, port);
			error = -1;
			goto close;
		case -2:
			logprintf(LOG_NOTICE, "mail server connection timeout @%s:%d", host, port);
			error = -1;
			goto close;
		case -3:
			logprintf(LOG_NOTICE, "Error in mail server socket connection @%s:%d", host, port);
			error = -1;
			goto close;
		default:
		break;
	}

starttls:
	if(have_ssl == 1) {
		entropy_init(&entropy);
		entropyfree = 1;
		if((ctr_drbg_init(&ctr_drbg, entropy_func, &entropy, (const unsigned char *)"pilight", 6)) != 0) {
			logprintf(LOG_NOTICE, "SMTP: ctr_drbg_init failed");
			error = -1;
			goto close;
		}

		if((ssl_init(&ssl)) != 0) {
			logprintf(LOG_NOTICE, "SMTP: ssl_init failed");
			error = -1;
			goto close;
		}
		sslfree = 1;

		ssl_set_endpoint(&ssl, SSL_IS_CLIENT);
		ssl_set_rng(&ssl, ctr_drbg_random, &ctr_drbg);
		ssl_set_bio(&ssl, net_recv, &sockfd, net_send, &sockfd);

		int ret = 0;
		while((ret = ssl_handshake(&ssl)) != 0) {
			if(ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE) {
				logprintf(LOG_NOTICE, "SMTP: ssl_handshake failed");
				error = -1;
				goto close;
			}
		}
	}

	if(authtype != STARTTLS) {
		memset(recvBuff, '\0', sizeof(recvBuff));
		if(sd_read(recvBuff) <= 0) {
			logprintf(LOG_NOTICE, "SMTP: didn't see identification");
			error = -1;
			goto close;
		}
		if(pilight.debuglevel == 1) {
			fprintf(stderr, "SMTP: %s", recvBuff);
		}
		if(strncmp(recvBuff, "220", 3) != 0) {
			logprintf(LOG_NOTICE, "SMTP: didn't see identification");
			error = -1;
			goto close;
		}
		if(strncmp(recvBuff, "4xx", 1) == 0) {
			logprintf(LOG_NOTICE, "SMTP: connection protocol issues - abort");
			error = -1;
			goto close;
		}
		if(strncmp(recvBuff, "5xx", 1) == 0) {
			logprintf(LOG_NOTICE, "SMTP: negative response from remote server - abort");
			error = -1;
			goto close;
		}
	}

	len = strlen("EHLO ")+strlen(USERAGENT)+3;
	if((out = REALLOC(out, len+1)) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	len = (size_t)snprintf(out, len, "EHLO %s\r\n", USERAGENT);
	if(pilight.debuglevel == 1) {
		fprintf(stderr, "SMTP: %s", out);
	}
	if(sd_write(out) != 0) {
		logprintf(LOG_NOTICE, "SMTP: failed to send EHLO");
		error = -1;
		goto close;
	}

	memset(recvBuff, '\0', sizeof(recvBuff));
	while(sd_read(recvBuff) > 0) {
		if(pilight.debuglevel == 1) {
			fprintf(stderr, "SMTP: %s", recvBuff);
		}
		if(sscanf(recvBuff, "%d%c%[^\r\n]", &val, &ch, testme) == 0) {
			error = -1;
			goto close;
		}
		if(val != 250) {
			logprintf(LOG_NOTICE, "SMTP: expected 250 got %d", val);
			error = -1;
			goto close;
		}
		if(strncmp(testme, "AUTH", 4) == 0) {
			if(strstr(testme, "PLAIN") != NULL || strstr(testme, "plain") != NULL) {
				authtype = AUTHPLAIN;
			}
		}
		/*
		 * The server wants to continue our conversation in encrypted mode
		 */
		if(strncmp(testme, "STARTTLS", 8) == 0) {
			authtype = STARTTLS;
		}

		/* The 250 list often contains several lines.
		 * Each line starts with "250-..." except the last.
		 * The last line is "250 ..." without the minus sign.
		 */
		if(ch != '-') {
			break;
		}
		memset(recvBuff, '\0', sizeof(recvBuff));
	}

	/*
	 * Tell the server we accept the inventation to communicate encrypted
	 */
	if(authtype == STARTTLS) {
		len = strlen("STARTTLS\r\n")+1;
		if((out = REALLOC(out, len+1)) == NULL) {
			fprintf(stderr, "out of memory\n");
			exit(EXIT_FAILURE);
		}
		strcpy(out, "STARTTLS\r\n");
		if(pilight.debuglevel == 1) {
			fprintf(stderr, "SMTP: %s", out);
		}
		if(sd_write(out) != 0) {
			logprintf(LOG_NOTICE, "SMTP: failed to send STARTTLS");
			error = -1;
			goto close;
		}

		while(sd_read(recvBuff) > 0) {
			if(pilight.debuglevel == 1) {
				fprintf(stderr, "SMTP: %s", recvBuff);
			}
			if(strncmp(recvBuff, "220", 3) == 0) {
				/*
				 * We restart our communcation encrypted by resending our EHLO
				 */
				if(pilight.debuglevel == 1) {
					fprintf(stderr, "SMTP: restart communication encrypted");
				}
				have_ssl = 1;
				goto starttls;
			}
			memset(recvBuff, '\0', sizeof(recvBuff));
		}
	}

	if(authtype == UNSUPPORTED) {
		logprintf(LOG_NOTICE, "SMTP: no supported authentication method");
		error = -1;
		goto close;
	}

	len = strlen(login)+strlen(pass)+2;

	char *authstr = MALLOC(len), *hash = NULL;
	if(authstr == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	memset(authstr, '\0', len);
	strncpy(&authstr[1], login, strlen(login));
	strncpy(&authstr[2+strlen(login)], pass, len-strlen(login)-2);
	hash = base64encode(authstr, len);
	FREE(authstr);

	len = strlen("AUTH PLAIN ")+strlen(hash)+3;
	if((out = REALLOC(out, len+4)) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	memset(out, '\0', len);
	snprintf(out, len, "AUTH PLAIN %s\r\n", hash);
	FREE(hash);

	/* Don't put the password in the log */
	if(pilight.debuglevel == 1) {
		fprintf(stderr, "SMTP: AUTH PLAIN xxxxxx");
	}
	if(sd_write(out) != 0) {
		logprintf(LOG_NOTICE, "SMTP: failed to send AUTH PLAIN");
		goto close;
	}

	memset(recvBuff, '\0', sizeof(recvBuff));
	while(sd_read(recvBuff) > 0) {
		if(pilight.debuglevel == 1) {
			fprintf(stderr, "SMTP: %s", recvBuff);
		}
		if(strncmp(recvBuff, "235", 3) == 0) {
			break;
		}
		if(strncmp(recvBuff, "4xx", 1) == 0) {
			logprintf(LOG_NOTICE, "SMTP: connection protocol issues - abort");
			error = -1;
			goto close;
		}
		if(strncmp(recvBuff, "5xx", 1) == 0) {
			logprintf(LOG_NOTICE, "SMTP: negative response from remote server - abort");
			error = -1;
			goto close;
		}
		memset(recvBuff, '\0', sizeof(recvBuff));
	}

	len = strlen("MAIL FROM: <>\r\n")+strlen(mail->from)+1;
	if((out = REALLOC(out, len+1)) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	len = (size_t)snprintf(out, len, "MAIL FROM: <%s>\r\n", mail->from);
	if(pilight.debuglevel == 1) {
		fprintf(stderr, "SMTP: %s", out);
	}
	if(sd_write(out) != 0) {
		logprintf(LOG_NOTICE, "SMTP: failed to send MAIL FROM");
		error = -1;
		goto close;
	}

	memset(recvBuff, '\0', sizeof(recvBuff));
	while(sd_read(recvBuff) > 0) {
		if(pilight.debuglevel == 1) {
			fprintf(stderr, "SMTP: %s", recvBuff);
		}
		if(strncmp(recvBuff, "250", 3) == 0) {
			break;
		}
		if(strncmp(recvBuff, "4xx", 1) == 0) {
			logprintf(LOG_NOTICE, "SMTP: connection protocol issues - abort");
			error = -1;
			goto close;
		}
		if(strncmp(recvBuff, "5xx", 1) == 0) {
			logprintf(LOG_NOTICE, "SMTP: negative response from remote server - abort");
			error = -1;
			goto close;
		}
		memset(recvBuff, '\0', sizeof(recvBuff));
	}

	len = strlen("RCPT TO: <>\r\n")+strlen(mail->to)+1;
	if((out = REALLOC(out, len+1)) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	snprintf(out, len, "RCPT TO: <%s>\r\n", mail->to);
	if(pilight.debuglevel == 1) {
		fprintf(stderr, "SMTP: %s", out);
	}
	if(sd_write(out) != 0) {
		logprintf(LOG_NOTICE, "SMTP: failed to send RCPT");
		error = -1;
		goto close;
	}

	memset(recvBuff, '\0', sizeof(recvBuff));
	while(sd_read(recvBuff) > 0) {
		if(pilight.debuglevel == 1) {
			fprintf(stderr, "SMTP: %s", recvBuff);
		}
		if(strncmp(recvBuff, "250", 3) == 0) {
			break;
		}
		if(strncmp(recvBuff, "4xx", 1) == 0) {
			logprintf(LOG_NOTICE, "SMTP: connection protocol issues - abort");
			error = -1;
			goto close;
		}
		if(strncmp(recvBuff, "5xx", 1) == 0) {
			logprintf(LOG_NOTICE, "SMTP: negative response from remote server - abort");
			error = -1;
			goto close;
		}
		memset(recvBuff, '\0', sizeof(recvBuff));
	}

	len = strlen("DATA\r\n")+1;
	if((out = REALLOC(out, len+1)) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	strcpy(out, "DATA\r\n");
	if(pilight.debuglevel == 1) {
		fprintf(stderr, "SMTP: %s", out);
	}
	if(sd_write(out) != 0) {
		logprintf(LOG_NOTICE, "SMTP: failed to send DATA");
		error = -1;
		goto close;
	}

	memset(recvBuff, '\0', sizeof(recvBuff));
	while(sd_read(recvBuff) > 0) {
		if(pilight.debuglevel == 1) {
			fprintf(stderr, "SMTP: %s", recvBuff);
		}
		if(strncmp(recvBuff, "354", 3) == 0) {
			break;
		}
		if(strncmp(recvBuff, "4xx", 1) == 0) {
			logprintf(LOG_NOTICE, "SMTP: connection protocol issues - abort");
			error = -1;
			goto close;
		}
		if(strncmp(recvBuff, "5xx", 1) == 0) {
			logprintf(LOG_NOTICE, "SMTP: negative response from remote server - abort");
			error = -1;
			goto close;
		}
		memset(recvBuff, '\0', sizeof(recvBuff));
	}

	len = 255+strlen(mail->to)+strlen(mail->from)+strlen(mail->subject)+strlen(mail->message);
	if((out = REALLOC(out, len+1)) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	len = (size_t)snprintf(out, len, "Subject: %s\r\n"
										"From: <%s>\r\n"
										"To: <%s>\r\n"
										"Content-Type: text/plain\r\n"
										"Mime-Version: 1.0\r\n"
										"X-Mailer: Emoticode smtp_send\r\n"
										"Content-Transfer-Encoding: 7bit\r\n\r\n"
										"%s"
										"\r\n.\r\n",
										mail->subject, mail->from, mail->to, mail->message);
	if(pilight.debuglevel == 1) {
		fprintf(stderr, "SMTP: %s", out);
	}
	if(sd_write(out) != 0) {
		logprintf(LOG_NOTICE, "SMTP: failed to send MESSAGE");
		error = -1;
		goto close;
	}

	memset(recvBuff, '\0', sizeof(recvBuff));
	while(sd_read(recvBuff) > 0) {
		if(pilight.debuglevel == 1) {
			fprintf(stderr, "SMTP: %s", recvBuff);
		}
		if(strncmp(recvBuff, "250", 3) == 0) {
			break;
		}
		if(strncmp(recvBuff, "4xx", 1) == 0) {
			logprintf(LOG_NOTICE, "SMTP: connection protocol issues - abort");
			error = -1;
			goto close;
		}
		if(strncmp(recvBuff, "5xx", 1) == 0) {
			logprintf(LOG_NOTICE, "SMTP: negative response from remote server - abort");
			error = -1;
			goto close;
		}
		memset(recvBuff, '\0', sizeof(recvBuff));
	}

	len = strlen("RSET\r\n");
	if((out = REALLOC(out, len+1)) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	strcpy(out, "RSET\r\n");
	if(pilight.debuglevel == 1) {
		fprintf(stderr, "SMTP: %s", out);
	}
	if(sd_write(out) != 0) {
		logprintf(LOG_NOTICE, "SMTP: failed to send RSET");
		error = -1;
		goto close;
	}

	memset(recvBuff, '\0', sizeof(recvBuff));
	while(sd_read(recvBuff) > 0) {
		if(pilight.debuglevel == 1) {
			fprintf(stderr, "SMTP: %s", recvBuff);
		}
		if(strncmp(recvBuff, "250", 3) == 0) {
			break;
		}
		if(strncmp(recvBuff, "4xx", 1) == 0) {
			logprintf(LOG_NOTICE, "SMTP: connection protocol issues - abort");
			error = -1;
			goto close;
		}
		if(strncmp(recvBuff, "5xx", 1) == 0) {
			logprintf(LOG_NOTICE, "SMTP: negative response from remote server - abort");
			error = -1;
			goto close;
		}
		memset(recvBuff, '\0', sizeof(recvBuff));
	}

	len = strlen("QUIT\r\n");
	if((out = REALLOC(out, len+1)) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	strcpy(out, "QUIT\r\n");
	if(pilight.debuglevel == 1) {
		fprintf(stderr, "SMTP: %s", out);
	}
	if(sd_write(out) != 0) {
		logprintf(LOG_NOTICE, "failed to send QUIT");
		error = -1;
		goto close;
	}

close:
	if(have_ssl == 1) {
		if(sslfree == 1) {
			ssl_free(&ssl);
		}
		if(entropyfree == 1) {
			entropy_free(&entropy);
		}
	}
	if(out != NULL) {
		FREE(out);
	}
	if(sockfd > 0) {
		close(sockfd);
	}
	return error;
}
