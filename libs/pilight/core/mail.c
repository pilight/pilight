/*
	Copyright (C) 2013 - 2015 CurlyMo

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

#include "ssl.h"
#include "eventpool.h"
#include "../../mbedtls/mbedtls/ssl.h"

#include "network.h"
#include "log.h"
#include "mail.h"

#define USERAGENT			"pilight"

#define UNSUPPORTED		0
#define AUTHPLAIN			1
#define STARTTLS			2

#define SMTP_STEP_SEND_HELLO		0
#define SMTP_STEP_RECV_HELLO		1
#define SMTP_STEP_SEND_STARTTLS	2
#define SMTP_STEP_RECV_STARTTLS	3
#define SMTP_STEP_SEND_AUTH			4
#define SMTP_STEP_RECV_AUTH			5
#define SMTP_STEP_SEND_FROM			6
#define SMTP_STEP_RECV_FROM			7
#define SMTP_STEP_SEND_TO				8
#define SMTP_STEP_RECV_TO				9
#define SMTP_STEP_SEND_DATA			10
#define SMTP_STEP_RECV_DATA			11
#define SMTP_STEP_SEND_BODY			12
#define SMTP_STEP_RECV_BODY			13
#define SMTP_STEP_SEND_RESET		14
#define SMTP_STEP_RECV_RESET		15
#define SMTP_STEP_SEND_QUIT			16
#define SMTP_STEP_RECV_QUIT			17
#define SMTP_STEP_END						99

typedef struct request_t {
	const char *host;
  const char *login;
	const char *pass;
	unsigned short port;

	char *content;
	int content_len;
	int step;
	int authtype;
	int reading;
	int bytes_read;
	void (*callback)(int);

	struct mail_t *mail;

	int is_ssl;
	int handshake;
	mbedtls_ssl_context ssl;
} request_t;

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
					if(c->callback != NULL) {
						c->callback(-1);
					}
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
				if(c->callback != NULL) {
					c->callback(-1);
				}
				return -1;
			}
			if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
				mbedtls_strerror(ret, (char *)&buffer, BUFFER_SIZE);
				logprintf(LOG_NOTICE, "mbedtls_ssl_write failed: %s", buffer);
				if(c->callback != NULL) {
					c->callback(-1);
				}
				return -1;
			}
			return 0;
		} else {
			return ret;
		}
	}
}

static int client_callback(struct eventpool_fd_t *node, int event) {
	char buffer[BUFFER_SIZE], testme[256], ch = 0;
	int bytes = 0, val = 0, n = 0;
	memset(&buffer, '\0', BUFFER_SIZE);

	struct request_t *request = (struct request_t *)node->userdata;
	switch(event) {
		case EV_CONNECT_SUCCESS: {
			if(request->is_ssl == 1) {
				int ret = 0;
				if((ret = mbedtls_ssl_setup(&request->ssl, &ssl_client_conf)) != 0) {
						mbedtls_strerror(ret, (char *)&buffer, BUFFER_SIZE);
						logprintf(LOG_ERR, "mbedtls_ssl_setup failed: %s", buffer);
						if(request->callback != NULL) {
							request->callback(-1);
						}
						return -1;
					}

				mbedtls_ssl_session_reset(&request->ssl);
				mbedtls_ssl_set_bio(&request->ssl, &node->fd, mbedtls_net_send, mbedtls_net_recv, NULL);
			}
			eventpool_fd_enable_write(node);
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
					if(request->callback != NULL) {
						request->callback(-1);
					}
					return -1;
				}

				buffer[bytes] = '\0';
			} else {
				if((bytes = recv(node->fd, buffer, BUFFER_SIZE, 0)) <= 0) {
					if(request->callback != NULL) {
						request->callback(-1);
					}
					return -1;
				}
			}

			if(request->reading == 1) {
				if((request->content = REALLOC(request->content, request->bytes_read+bytes+1)) == NULL) {
					OUT_OF_MEMORY
				}
				strncpy(&request->content[request->bytes_read], buffer, request->bytes_read+bytes);
				request->bytes_read += bytes;
			}
			request->reading = 1;

			if(pilight.debuglevel >= 2) {
				fprintf(stderr, "SMTP: %s\n", buffer);
			}
			switch(request->step) {
				case SMTP_STEP_RECV_HELLO: {
					char **array = NULL;
					int i = 0;
					n = explode(request->content, "\r\n", &array);
					for(i=0;i<n;i++) {
						if(sscanf(array[i], "%d%c%[^\r\n]", &val, &ch, testme) == 0) {
							continue;
						}
						if(val == 250) {
							if(strncmp(testme, "AUTH", 4) == 0) {
								if(strstr(testme, "PLAIN") != NULL || strstr(testme, "plain") != NULL) {
									request->authtype = AUTHPLAIN;
								}
							}
							if(strncmp(testme, "STARTTLS", 8) == 0) {
								request->authtype = STARTTLS;
							}
						}
						/*
						 * The 250 list often contains several lines.
						 * Each line starts with "250-..." except the last.
						 * The last line is "250 ..." without the minus sign.
						 */
						if(val == 250 && ch == 32) {
							request->reading = 0;
							request->bytes_read = 0;
							if(request->authtype == UNSUPPORTED) {
								logprintf(LOG_NOTICE, "SMTP: no supported authentication method");
								array_free(&array, n);
								if(request->callback != NULL) {
									request->callback(-1);
								}
								return -1;
							}
							if(request->authtype == STARTTLS) {
								request->step = SMTP_STEP_SEND_STARTTLS;
							} else {
								request->step = SMTP_STEP_SEND_AUTH;
							}
							eventpool_fd_enable_write(node);
							break;
						}
					}
					if(request->reading == 1) {
						eventpool_fd_enable_read(node);
					}
					array_free(&array, n);
				} break;
				case SMTP_STEP_RECV_STARTTLS: {
					if(strncmp(buffer, "220", 3) == 0) {
						request->is_ssl = 1;

						int ret = 0;
						if((ret = mbedtls_ssl_setup(&request->ssl, &ssl_client_conf)) != 0) {
							mbedtls_strerror(ret, (char *)&buffer, BUFFER_SIZE);
							logprintf(LOG_ERR, "mbedtls_ssl_setup failed: %s", buffer);
							request->callback(-1);
							return -1;
						}

						mbedtls_ssl_session_reset(&request->ssl);
						mbedtls_ssl_set_bio(&request->ssl, &node->fd, mbedtls_net_send, mbedtls_net_recv, NULL);

						request->step = SMTP_STEP_SEND_HELLO;
						request->bytes_read = 0;
						eventpool_fd_enable_write(node);
					}
				} break;
				case SMTP_STEP_RECV_AUTH: {
					if(strncmp(buffer, "235", 3) == 0) {
						request->step = SMTP_STEP_SEND_FROM;
					}
					if(strncmp(buffer, "451", 3) == 0) {
						logprintf(LOG_NOTICE, "SMTP: protocol violation while authenticating");
						if(request->callback != NULL) {
							request->callback(-1);
						}
						return -1;
					}
					if(strncmp(buffer, "501", 3) == 0) {
						logprintf(LOG_NOTICE, "SMTP: cannot decode response");
						if(request->callback != NULL) {
							request->callback(-1);
						}
						return -1;
					}
					if(strncmp(buffer, "535", 3) == 0) {
						logprintf(LOG_NOTICE, "SMTP: authentication failed: wrong user/password");
						if(request->callback != NULL) {
							request->callback(-1);
						}
						return -1;
					}
					request->bytes_read = 0;
					request->step = SMTP_STEP_SEND_FROM;
					eventpool_fd_enable_write(node);
				} break;
				case SMTP_STEP_RECV_FROM: {
					if(strncmp(buffer, "250", 3) != 0) {
						if(request->callback != NULL) {
							request->callback(-1);
						}
						return -1;
					}
					request->bytes_read = 0;
					request->step = SMTP_STEP_SEND_TO;
					eventpool_fd_enable_write(node);
				} break;
				case SMTP_STEP_RECV_TO: {
					if(strncmp(buffer, "250", 3) != 0) {
						if(request->callback != NULL) {
							request->callback(-1);
						}
						return -1;
					}
					request->bytes_read = 0;
					request->step = SMTP_STEP_SEND_DATA;
					eventpool_fd_enable_write(node);
				} break;
				case SMTP_STEP_RECV_DATA: {
					if(strncmp(buffer, "354", 3) != 0) {
						if(request->callback != NULL) {
							request->callback(-1);
						}
						return -1;
					}
					request->bytes_read = 0;
					request->step = SMTP_STEP_SEND_BODY;
					eventpool_fd_enable_write(node);
				} break;
				case SMTP_STEP_RECV_BODY: {
					if(strncmp(buffer, "250", 3) != 0) {
						if(request->callback != NULL) {
							request->callback(-1);
						}
						return -1;
					}
					request->bytes_read = 0;
					request->step = SMTP_STEP_SEND_RESET;
					eventpool_fd_enable_write(node);
				} break;
				case SMTP_STEP_RECV_RESET: {
					if(strncmp(buffer, "250", 3) != 0) {
						if(request->callback != NULL) {
							request->callback(-1);
						}
						return -1;
					}
					request->bytes_read = 0;
					request->step = SMTP_STEP_SEND_QUIT;
					eventpool_fd_enable_write(node);
				} break;
				case SMTP_STEP_RECV_QUIT: {
					logprintf(LOG_INFO, "SMTP: successfully send mail");
					if(request->callback != NULL) {
						request->callback(0);
					}
					return -1;
				} break;
			}
		} break;
		case EV_WRITE: {
			switch(request->step) {
				case SMTP_STEP_SEND_HELLO: {
					request->content_len = strlen("EHLO ")+strlen(USERAGENT)+3;
					if((request->content = REALLOC(request->content, request->content_len+1)) == NULL) {
						OUT_OF_MEMORY
					}
					request->content_len = (size_t)snprintf(request->content, request->content_len, "EHLO %s\r\n", USERAGENT);
					eventpool_fd_write(node->fd, request->content, request->content_len);
					request->step = SMTP_STEP_RECV_HELLO;
					eventpool_fd_enable_read(node);
					if(pilight.debuglevel >= 2) {
						fprintf(stderr, "SMTP: %s\n", request->content);
					}
				} break;
				case SMTP_STEP_SEND_STARTTLS: {
					request->content_len = strlen("STARTTLS\r\n");
					if((request->content = REALLOC(request->content, request->content_len+1)) == NULL) {
						OUT_OF_MEMORY
					}
					strncpy(request->content, "STARTTLS\r\n", request->content_len);
					eventpool_fd_write(node->fd, request->content, request->content_len);
					request->step = SMTP_STEP_RECV_STARTTLS;
					eventpool_fd_enable_read(node);
					if(pilight.debuglevel >= 2) {
						fprintf(stderr, "SMTP: %s\n", request->content);
					}
				} break;
				case SMTP_STEP_SEND_AUTH: {
					request->content_len = strlen(request->login)+strlen(request->pass)+2;
					char *authstr = MALLOC(request->content_len), *hash = NULL;
					if(authstr == NULL) {
						OUT_OF_MEMORY
					}
					memset(authstr, '\0', request->content_len);
					strncpy(&authstr[1], request->login, strlen(request->login));
					strncpy(&authstr[2+strlen(request->login)], request->pass, request->content_len-strlen(request->login)-2);
					hash = base64encode(authstr, request->content_len);
					FREE(authstr);

					request->content_len = strlen("AUTH PLAIN ")+strlen(hash)+3;
					if((request->content = REALLOC(request->content, request->content_len+4)) == NULL) {
						OUT_OF_MEMORY
					}
					memset(request->content, '\0', request->content_len);
					request->content_len = snprintf(request->content, request->content_len, "AUTH PLAIN %s\r\n", hash);
					FREE(hash);

					eventpool_fd_write(node->fd, request->content, request->content_len);
					request->step = SMTP_STEP_RECV_AUTH;
					eventpool_fd_enable_read(node);
					if(pilight.debuglevel >= 2) {
						fprintf(stderr, "SMTP: AUTH PLAIN XXX\n");
					}
				} break;
				case SMTP_STEP_SEND_FROM: {
					request->content_len = strlen("MAIL FROM: <>\r\n")+strlen(request->mail->from)+1;
					if((request->content = REALLOC(request->content, request->content_len+1)) == NULL) {
						OUT_OF_MEMORY
					}
					request->content_len = (size_t)snprintf(request->content, request->content_len, "MAIL FROM: <%s>\r\n", request->mail->from);
					eventpool_fd_write(node->fd, request->content, request->content_len);
					request->step = SMTP_STEP_RECV_FROM;
					eventpool_fd_enable_read(node);
					if(pilight.debuglevel >= 2) {
						fprintf(stderr, "SMTP: %s\n", request->content);
					}
				} break;
				case SMTP_STEP_SEND_TO: {
					request->content_len = strlen("RCPT TO: <>\r\n")+strlen(request->mail->to)+1;
					if((request->content = REALLOC(request->content, request->content_len+1)) == NULL) {
						OUT_OF_MEMORY
					}
					request->content_len = (size_t)snprintf(request->content, request->content_len, "RCPT TO: <%s>\r\n", request->mail->to);
					eventpool_fd_write(node->fd, request->content, request->content_len);
					request->step = SMTP_STEP_RECV_TO;
					eventpool_fd_enable_read(node);
					if(pilight.debuglevel >= 2) {
						fprintf(stderr, "SMTP: %s\n", request->content);
					}
				} break;
				case SMTP_STEP_SEND_DATA: {
					request->content_len = strlen("DATA\r\n");
					if((request->content = REALLOC(request->content, request->content_len+1)) == NULL) {
						OUT_OF_MEMORY
					}
					strncpy(request->content, "DATA\r\n", request->content_len);
					eventpool_fd_write(node->fd, request->content, request->content_len);
					request->step = SMTP_STEP_RECV_DATA;
					eventpool_fd_enable_read(node);
					if(pilight.debuglevel >= 2) {
						fprintf(stderr, "SMTP: %s\n", request->content);
					}
				} break;
				case SMTP_STEP_SEND_BODY: {
					request->content_len = 255;
					request->content_len += strlen(request->mail->to);
					request->content_len += strlen(request->mail->from);
					request->content_len += strlen(request->mail->subject);
					request->content_len += strlen(request->mail->message);

					if((request->content = REALLOC(request->content, request->content_len+1)) == NULL) {
						OUT_OF_MEMORY
					}
					request->content_len = (size_t)snprintf(request->content, request->content_len,
						"Subject: %s\r\n"
						"From: <%s>\r\n"
						"To: <%s>\r\n"
						"Content-Type: text/plain\r\n"
						"Mime-Version: 1.0\r\n"
						"X-Mailer: Emoticode smtp_send\r\n"
						"Content-Transfer-Encoding: 7bit\r\n\r\n"
						"%s"
						"\r\n.\r\n",
						request->mail->subject, request->mail->from, request->mail->to, request->mail->message);

						eventpool_fd_write(node->fd, request->content, request->content_len);
						request->step = SMTP_STEP_RECV_BODY;
						eventpool_fd_enable_read(node);
					if(pilight.debuglevel >= 2) {
						fprintf(stderr, "SMTP: %s\n", request->content);
					}
				} break;
				case SMTP_STEP_SEND_RESET: {
					request->content_len = strlen("RSET\r\n");
					if((request->content = REALLOC(request->content, request->content_len+1)) == NULL) {
						OUT_OF_MEMORY
					}
					strncpy(request->content, "RSET\r\n", request->content_len);
					eventpool_fd_write(node->fd, request->content, request->content_len);
					request->step = SMTP_STEP_RECV_RESET;
					eventpool_fd_enable_read(node);
					if(pilight.debuglevel >= 2) {
						fprintf(stderr, "SMTP: %s\n", request->content);
					}
				} break;
				case SMTP_STEP_SEND_QUIT: {
					request->content_len = strlen("QUIT\r\n");
					if((request->content = REALLOC(request->content, request->content_len+1)) == NULL) {
						OUT_OF_MEMORY
					}
					strncpy(request->content, "QUIT\r\n", request->content_len);
					eventpool_fd_write(node->fd, request->content, request->content_len);
					request->step = SMTP_STEP_RECV_QUIT;
					eventpool_fd_enable_read(node);
					if(pilight.debuglevel >= 2) {
						fprintf(stderr, "SMTP: %s\n", request->content);
					}
				} break;
			}
		} break;
		case EV_DISCONNECTED: {
			if(request->is_ssl == 1) {
				mbedtls_ssl_free(&request->ssl);
			}
			if(request->content != NULL) {
				FREE(request->content);
			}
			if(request->mail != NULL) {
				if(request->mail->subject != NULL) {
					FREE(request->mail->subject);
				}
				if(request->mail->message != NULL) {
					FREE(request->mail->message);
				}
				if(request->mail->to != NULL) {
					FREE(request->mail->to);
				}
				if(request->mail->from != NULL) {
					FREE(request->mail->from);
				}
				FREE(request->mail);
			}
			FREE(request);
			eventpool_fd_remove(node);
		} break;
	}
	return 0;
}

int sendmail(char *host, char *login, char *pass, unsigned short port, struct mail_t *mail, void (*callback)(int)) {
	struct request_t *request = NULL;
	if((request = MALLOC(sizeof(struct request_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(request, 0, sizeof(struct request_t));
	request->host = host;
	request->login = login;
	request->pass = pass;
	request->port = port;
	request->mail = mail;
	request->authtype = UNSUPPORTED;
	request->callback = callback;

	switch(port) {
		case 465:
			request->is_ssl = 1;
		break;
		case 25:
		case 587:
			request->is_ssl = 0;
		break;
		default:
			logprintf(LOG_ERR, "port %d is not an valid SMTP port", port);
			FREE(request);
			if(request->callback != NULL) {
				request->callback(-1);
			}
			return -1;
		break;
	}
	if(request->is_ssl == 1 && ssl_client_init_status() == -1) {
		logprintf(LOG_ERR, "secure mails require a properly initialized ssl library");
		FREE(request);
		if(request->callback != NULL) {
			request->callback(-1);
		}
		return -1;
	}

	eventpool_socket_add("mail", host, port, AF_INET, SOCK_STREAM, 0, EVENTPOOL_TYPE_SOCKET_CLIENT, client_callback, client_send, request);
	return 0;
}
