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
#include <stdarg.h>
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
unsigned short webserver_php = 1;
char *webserver_root = NULL;
char *webgui_tpl = NULL;
unsigned char alphabet[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
struct mg_server *mgserver[WEBSERVER_WORKERS];

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
	int i = 0;

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
	for(i=0;i<WEBSERVER_WORKERS;i++) {	
		mg_destroy_server(&mgserver[i]);
	}	
	fcache_gc();
	logprintf(LOG_DEBUG, "garbage collected webserver library");
	return 1;
}

struct filehandler_t {
	unsigned char *bytes;
	FILE *fp;
	unsigned int ptr;
	unsigned int length;
	unsigned short free;
} filehandler_t;

int webserver_which(const char *program) {
	char path[1024];
	strcpy(path, getenv("PATH"));
	char *pch = strtok(path, ":");
	while(pch) {
		char exec[strlen(pch)+8];
		strcpy(exec, pch);
		strcat(exec, "/");
		strcat(exec, program);
		if(access(exec, X_OK) != -1) {
			return 0;
		}
		pch = strtok(NULL, ":");
	}
	return -1;
}

int webserver_ishex(int x) {
	return(x >= '0' && x <= '9') || (x >= 'a' && x <= 'f') || (x >= 'A' && x <= 'F');
}

const char *rstrstr(const char* haystack, const char* needle) {
	char* loc = 0;
	char* found = 0;
	size_t pos = 0;

	while ((found = strstr(haystack + pos, needle)) != 0) {
		loc = found;
		pos = (size_t)((found - haystack) + 1);
	}

	return loc;
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

void webserver_alpha_random(char *s, const int len) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
	int i = 0;

    for(i = 0; i < len; ++i) {
        s[i] = alphanum[(unsigned int)rand() % (sizeof(alphanum) - 1)];
    }

    s[len] = 0;
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

void webserver_create_minimal_header(unsigned char **p, const char *message, unsigned int len) {
	*p += sprintf((char *)*p,
		"HTTP/1.1 %s\r\n"
		"Server: zfsguru\r\n",
		message);
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

char *webserver_shell(const char *format_str, struct mg_connection *conn, char *request, ...) {
	size_t n = 0;
	char *output = NULL;
	const char *type = NULL;
	const char *cookie = NULL;
	va_list ap;

	va_start(ap, request);
	n = (size_t)vsnprintf(NULL, 0, format_str, ap) + strlen(format_str) + 1; // EOL + dual NL
	va_end(ap);

	char *command[n];
	va_start(ap, request);
	vsprintf((char *)command, format_str, ap);
	va_end(ap);

	setenv("SCRIPT_FILENAME", request, 1);
	setenv("REDIRECT_STATUS", "200", 1);
	setenv("SERVER_PROTOCOL", "HTTP/1.1", 1);
	setenv("REMOTE_HOST", "127.0.0.1", 1);
	char sn[1024] = {'\0'};
	if(conn->remote_port != 80) {
		sprintf(sn, "http://%s:%d", conn->remote_ip, conn->remote_port);
	} else {
		sprintf(sn, "http://%s", conn->remote_ip);
	}
	setenv("SERVER_NAME", sn, 1);
	setenv("HTTPS", "off", 1);
	setenv("HTTP_ACCEPT", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8", 1);

	if(strcmp(conn->request_method, "POST") == 0) {
		setenv("REQUEST_METHOD", "POST", 1);
		if((type = mg_get_header(conn, "Content-Type")) != NULL) {
			setenv("CONTENT_TYPE", type, 1);
		}
		char len[10];
		sprintf(len, "%d", (int)conn->content_len);
		setenv("CONTENT_LENGTH", len, 1);
	}
	if((cookie = mg_get_header(conn, "Cookie")) != NULL) {
		setenv("HTTP_COOKIE", cookie, 1);
	}
	if(conn->query_string != NULL) {
		setenv("QUERY_STRING", conn->query_string, 1);
	}
	if(strcmp(conn->request_method, "GET") == 0) {
		setenv("REQUEST_METHOD", "GET", 1);
	}
	FILE *fp = NULL;
	if((fp = popen((char *)command, "r")) != NULL) {
		size_t total = 0;
		size_t chunk = 0;
		unsigned char buff[1024] = {'\0'};
		while(!feof(fp)) {
			chunk = fread(buff, sizeof(char), 1024, fp);
			total += chunk;
			output = realloc(output, total+1);
			if(!output) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			memcpy(&output[total-chunk], buff, chunk);
		}
		output[total] = '\0';
		unsetenv("SCRIPT_FILENAME");
		unsetenv("REDIRECT_STATUS");
		unsetenv("SERVER_PROTOCOL");
		unsetenv("REMOTE_HOST");
		unsetenv("SERVER_NAME");
		unsetenv("HTTPS");
		unsetenv("HTTP_ACCEPT");
		unsetenv("HTTP_COOKIE");
		unsetenv("REQUEST_METHOD");
		unsetenv("CONTENT_TYPE");
		unsetenv("CONTENT_LENGTH");
		unsetenv("QUERY_STRING");
		unsetenv("REQUEST_METHOD");

		pclose(fp);
		return output;
	}

	return NULL;
}

static int webserver_sockets_callback(struct mg_connection *c) {
	if(c->is_websocket) {
		mg_websocket_write(c, 1, (char *)c->callback_param, strlen((char *)c->callback_param));
	}
	return MG_REQUEST_PROCESSED;
}

static int webserver_auth_handler(struct mg_connection *conn) {
	if(webserver_authentication == 1 && webserver_username != NULL && webserver_password != NULL) {
		return mg_authorize_input(conn, webserver_username, webserver_password, mg_get_option(mgserver[0], "auth_domain"));
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
	struct filehandler_t *filehandler = (struct filehandler_t *)conn->connection_param;
	unsigned int chunk = WEBSERVER_CHUNK_SIZE;
	struct stat st;	

	if(!conn->is_websocket) {
		if(filehandler != NULL) {
			char buff[WEBSERVER_CHUNK_SIZE];
			if((filehandler->length-filehandler->ptr) < chunk) {
				chunk = (unsigned int)(filehandler->length-filehandler->ptr);
			}
			if(filehandler->fp != NULL) {
				chunk = (unsigned int)fread(buff, sizeof(char), WEBSERVER_CHUNK_SIZE, filehandler->fp);
				mg_send_data(conn, buff, (int)chunk);
			} else {
				mg_send_data(conn, &filehandler->bytes[filehandler->ptr], (int)chunk);
			}
			filehandler->ptr += chunk;

			if(filehandler->ptr == filehandler->length || conn->wsbits != 0) {
				if(filehandler->fp != NULL) {
					fclose(filehandler->fp);
					filehandler->fp = NULL;
				}
				if(filehandler->free) {
					sfree((void *)&filehandler->bytes);
				}
				sfree((void *)&filehandler);
				conn->connection_param = NULL;
				return MG_REQUEST_PROCESSED;
			} else {
				return MG_REQUEST_CALL_AGAIN;
			}
		}

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
		} else if(strcmp(&conn->uri[(rstrstr(conn->uri, "/")-conn->uri)], "/") == 0) {
			char indexes[255];
			strcpy(indexes, mg_get_option(mgserver[0], "index_files"));
			char *pch = strtok((char *)indexes, ",");
			/* Check if the webserver_root is terminated by a slash. If not, than add it */
			while(pch) {
				request = realloc(request, strlen(webserver_root)+strlen(webgui_tpl)+strlen(pch)+4);
				if(!request) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}			
				memset(request, '\0', strlen(webserver_root)+strlen(pch)+3);
				if(webserver_root[strlen(webserver_root)-1] == '/') {
#ifdef __FreeBSD__
					sprintf(request, "%s%s/%s%s", webserver_root, webgui_tpl, conn->uri, pch);
#else
					sprintf(request, "%s%s%s%s", webserver_root, webgui_tpl, conn->uri, pch);
#endif
				} else {
					sprintf(request, "%s/%s/%s%s", webserver_root, webgui_tpl, conn->uri, pch);
				}
				if(access(request, F_OK) == 0) {
					break;
				}
				pch = strtok(NULL, ",");
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
			} else if(strcmp(ext, "jpg") == 0) {
				mimetype = webserver_mimetype("image/jpg");
			} else if(strcmp(ext, "css") == 0) {
				mimetype = webserver_mimetype("text/css");
			} else if(strcmp(ext, "js") == 0) {
				mimetype = webserver_mimetype("text/javascript");
			} else if(strcmp(ext, "php") == 0) {
				mimetype = webserver_mimetype("application/x-httpd-php");
			} else {
				mimetype = webserver_mimetype("text/plain");
			}
		}
		sfree((void *)&ext);
		
		if(access(request, F_OK) == 0) {
			stat(request, &st);
			if(webserver_cache && st.st_size <= MAX_CACHE_FILESIZE && 
			   strcmp(mimetype, "application/x-httpd-php") != 0 &&
			   fcache_get_size(request, &size) != 0 && fcache_add(request) != 0) {
				goto filenotfound;
			}
		} else {
			goto filenotfound;
		}
		
		memset(buffer, '\0', 4096);
		p = buffer;

		const char *cl = NULL;
		if((cl = mg_get_header(conn, "Content-Length"))) {
			if(atoi(cl) > MAX_UPLOAD_FILESIZE) {
				sfree((void *)&mimetype);
				char line[1024] = {'\0'};
				mimetype = webserver_mimetype("text/plain");
				sprintf(line, "Webserver Warning: POST Content-Length of %d bytes exceeds the limit of %d bytes in Unknown on line 0", MAX_UPLOAD_FILESIZE, atoi(cl));
				webserver_create_header(&p, "200 OK", mimetype, (unsigned int)strlen(line));
				mg_write(conn, buffer, (int)(p-buffer));
				mg_write(conn, line, (int)strlen(line));
				sfree((void *)&mimetype);
				sfree((void *)&request);
				return MG_REQUEST_PROCESSED;
			}
		}

		/* If webserver caching is enabled, first load all files in the memory */
		if(strcmp(mimetype, "application/x-httpd-php") == 0 && webserver_php) {
			char *raw = NULL;
			if(strcmp(conn->request_method, "POST") == 0) {
				/* Store all (binary) post data in a file so we
				   can feed it directly into php-cgi */
				char file[20];
				strcpy(file, "/tmp/php");
				char name[11];
				webserver_alpha_random(name, 10);
				strcat(file, name);
				int f = open(file, O_TRUNC | O_WRONLY | O_CREAT);
				write(f, conn->content, conn->content_len);
				close(f);
				raw = webserver_shell("cat %s | php-cgi %s 2>&1 | base64", conn, request, file, request);
				unlink(file);
			} else {
				raw = webserver_shell("php-cgi %s  2>&1 | base64", conn, request, request);
			}

			if(raw != NULL) {
				char *output = malloc(strlen(raw)+1);
				memset(output, '\0', strlen(raw)+1);
				if(!output) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				memset(output, '\0', strlen(raw)+1);
				size_t olen = (size_t)base64decode((unsigned char *)output, (unsigned char *)raw, (int)strlen(raw));
				sfree((void *)&raw);

				char *ptr = strstr(output, "\n\r");
				char *xptr = strstr(output, "X-Powered-By:");
				char *sptr = strstr(output, "Status:");
				char *nptr = NULL;
				if(sptr) {
					nptr = sptr;
				} else {
					nptr = xptr;
				}

				if(ptr != NULL && nptr != NULL) {
					size_t pos = (size_t)(ptr-output);
					size_t xpos = (size_t)(nptr-output);
					char *header = malloc((pos-xpos)+(size_t)1);
					if(!header) {
						logprintf(LOG_ERR, "out of memory");
						exit(EXIT_FAILURE);
					}
					
					/* Extract header info from PHP output */
					strncpy(&header[0], &output[xpos], pos-xpos);
					header[(pos-xpos)] = '\0';
					
					/* Extract content info from PHP output */
					memmove(&output[xpos], &output[pos+3], olen-(pos+3));
					olen-=((pos+3)-xpos);
					
					/* Retrieve the PHP content type */
					char ite[pos-xpos];
					strcpy(ite, header);
					char *pch = strtok(ite, "\n\r");
					char type[255];
					while(pch) {
						if(sscanf(pch, "Content-type:%*[ ]%s%*[ \n\r]", type)) {
							break;
						}
						if(sscanf(pch, "Content-Type:%*[ ]%s%*[ \n\r]", type)) {
							break;
						}
						pch = strtok(NULL, "\n\r");
					}
					
					if(strstr(header, "Status: 302 Moved Temporarily") != NULL) {
						webserver_create_minimal_header(&p, "302 Moved Temporarily", (unsigned int)olen);
					} else {
						webserver_create_minimal_header(&p, "200 OK", (unsigned int)olen);
					}

					/* Merge HTML header with PHP header */
					char *hptr = strstr((char *)buffer, "\n\r");
					size_t hlen = (size_t)(hptr-(char *)buffer);
					pos = strlen(header);
					memcpy((char *)&buffer[hlen], header, pos);
					memcpy((char *)&buffer[hlen+pos], "\n\r\n\r", 4);

					if(strlen(type) > 0 && strstr(type, "text") != NULL) {
						mg_write(conn, buffer, (int)strlen((char *)buffer));
						mg_write(conn, output, (int)olen);
						sfree((void *)&output);
					} else {
						if(filehandler == NULL) {
							filehandler = malloc(sizeof(filehandler_t));
							filehandler->bytes = malloc(olen);
							memcpy(filehandler->bytes, output, olen);
							filehandler->length = (unsigned int)olen;
							filehandler->ptr = 0;
							filehandler->free = 1;
							filehandler->fp = NULL;
							conn->connection_param = filehandler;
						}
						sfree((void *)&output);
						chunk = WEBSERVER_CHUNK_SIZE;
						if(filehandler != NULL) {
							if((filehandler->length-filehandler->ptr) < chunk) {
								chunk = (filehandler->length-filehandler->ptr);
							}
							mg_send_data(conn, &filehandler->bytes[filehandler->ptr], (int)chunk);
							filehandler->ptr += chunk;

							sfree((void *)&mimetype);
							sfree((void *)&request);
							sfree((void *)&header);
							if(filehandler->ptr == filehandler->length || conn->wsbits != 0) {
								sfree((void *)&filehandler->bytes);
								sfree((void *)&filehandler);
								conn->connection_param = NULL;
								return MG_REQUEST_PROCESSED;
							} else {
								return MG_REQUEST_CALL_AGAIN;
							}
						}
					}
					sfree((void *)&header);
				}
				
				sfree((void *)&mimetype);
				sfree((void *)&request);
				return MG_REQUEST_PROCESSED;
			} else {
				logprintf(LOG_NOTICE, "(webserver) invalid php-cgi output from %s", request);
				webserver_create_404(conn->uri, &p);
				sfree((void *)&mimetype);
				sfree((void *)&request);

				return MG_REQUEST_PROCESSED;
			}
		} else {
			stat(request, &st);
			if(!webserver_cache || st.st_size > MAX_CACHE_FILESIZE) {
				FILE *fp = fopen(request, "rb");
				fseek(fp, 0, SEEK_END); 
				size = (int)ftell(fp);
				fseek(fp, 0, SEEK_SET);
				if(strstr(mimetype, "text") != NULL) {
					webserver_create_header(&p, "200 OK", mimetype, (unsigned int)size);
					mg_write(conn, buffer, (int)(p-buffer));
					size_t total = 0;
					chunk = 0;
					unsigned char buff[1024];
					while(total < size) {
						chunk = (unsigned int)fread(buff, sizeof(char), 1024, fp);
						mg_write(conn, buff, (int)chunk);
						total += chunk;
					}
					fclose(fp);
				} else {
					if(filehandler == NULL) {
						filehandler = malloc(sizeof(filehandler_t));
						filehandler->bytes = NULL;
						filehandler->length = (unsigned int)size;
						filehandler->ptr = 0;
						filehandler->free = 0;
						filehandler->fp = fp;
						conn->connection_param = filehandler;
					}
					char buff[WEBSERVER_CHUNK_SIZE];
					if(filehandler != NULL) {
						if((filehandler->length-filehandler->ptr) < chunk) {
							chunk = (filehandler->length-filehandler->ptr);
						}
						chunk = (unsigned int)fread(buff, sizeof(char), WEBSERVER_CHUNK_SIZE, fp);
						mg_send_data(conn, buff, (int)chunk);
						filehandler->ptr += chunk;

						sfree((void *)&mimetype);
						sfree((void *)&request);
						if(filehandler->ptr == filehandler->length || conn->wsbits != 0) {
							if(filehandler->fp != NULL) {
								fclose(filehandler->fp);
								filehandler->fp = NULL;
							}
							sfree((void *)&filehandler);
							conn->connection_param = NULL;
							return MG_REQUEST_PROCESSED;
						} else {
							return MG_REQUEST_CALL_AGAIN;
						}
					}
				}

				
				sfree((void *)&mimetype);
				sfree((void *)&request);
				return MG_REQUEST_PROCESSED;
			} else {
				if(fcache_get_size(request, &size) == 0) {
					if(strstr(mimetype, "text") != NULL) {
						webserver_create_header(&p, "200 OK", mimetype, (unsigned int)size);
						mg_write(conn, buffer, (int)(p-buffer));
						mg_write(conn, fcache_get_bytes(request), size);
						sfree((void *)&mimetype);
						sfree((void *)&request);
						return MG_REQUEST_PROCESSED;
					} else {
						if(filehandler == NULL) {
							filehandler = malloc(sizeof(filehandler_t));
							filehandler->bytes = fcache_get_bytes(request);
							filehandler->length = (unsigned int)size;
							filehandler->ptr = 0;
							filehandler->free = 0;
							filehandler->fp = NULL;
							conn->connection_param = filehandler;
						}
						chunk = WEBSERVER_CHUNK_SIZE;
						if(filehandler != NULL) {
							if((filehandler->length-filehandler->ptr) < chunk) {
								chunk = (filehandler->length-filehandler->ptr);
							}
							mg_send_data(conn, &filehandler->bytes[filehandler->ptr], (int)chunk);
							filehandler->ptr += chunk;

							sfree((void *)&mimetype);
							sfree((void *)&request);
							if(filehandler->ptr == filehandler->length || conn->wsbits != 0) {
								sfree((void *)&filehandler);
								conn->connection_param = NULL;
								return MG_REQUEST_PROCESSED;
							} else {
								return MG_REQUEST_CALL_AGAIN;
							}
						}
					}
				}
			}
			sfree((void *)&mimetype);
			sfree((void *)&request);
		}
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
	
filenotfound:
	logprintf(LOG_NOTICE, "(webserver) could not read %s", request);
	webserver_create_404(conn->uri, &p);
	mg_write(conn, buffer, (int)(p-buffer));
	sfree((void *)&mimetype);
	sfree((void *)&request);
	return MG_REQUEST_PROCESSED;	
}

int webserver_open_handler(struct mg_connection *conn) {
	char ip[17];
	strcpy(ip, conn->remote_ip);
	if(socket_check_whitelist(conn->remote_ip) != 0) {
		logprintf(LOG_INFO, "rejected client, ip: %s, port: %d", ip, conn->remote_port);
		return -1;
	} else {
		logprintf(LOG_INFO, "client connected, ip %s, port %d", ip, conn->remote_port);
		return 0;
	}
	return 1;
}

void *webserver_serve(void *srv) {
	for(;;) {
		mg_poll_server((struct mg_server *)srv, 1000);
	}
	return NULL;
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
	int i = 0;

	pthread_mutex_lock(&webqueue_lock);	

	while(webserver_loop) {
#ifdef __FreeBSD__
		pthread_mutex_lock(&webqueue_lock);		
#endif
		if(webqueue_number > 0) {
			pthread_mutex_lock(&webqueue_lock);

			for(i=0;i<WEBSERVER_WORKERS;i++) {	
				mg_iterate_over_connections(mgserver[i], webserver_sockets_callback, webqueue->message);
			}

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

	if(webserver_which("php-cgi") != 0) {
		webserver_php = 0;
		logprintf(LOG_ERR, "php support disabled due to missing php-cgi executable");
	}
	if(webserver_which("cat") != 0) {
		webserver_php = 0;
		logprintf(LOG_ERR, "php support disabled due to missing cat executable");
	}
	if(webserver_which("base64") != 0) {
		webserver_php = 0;
		logprintf(LOG_ERR, "php support disabled due to missing base64 executable");
	}

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

	char webport[10] = {'\0'};
	sprintf(webport, "%d", webserver_port);
	int i = 0;
	for(i=0;i<WEBSERVER_WORKERS;i++) {
		char id[2];
		sprintf(id, "%d", i);
		mgserver[i] = mg_create_server((void *)id);
		mg_set_option(mgserver[i], "listening_port", webport);
		mg_set_option(mgserver[i], "auth_domain", "zfsguru");
		mg_set_request_handler(mgserver[i], webserver_request_handler);
		mg_set_auth_handler(mgserver[i], webserver_auth_handler);
		mg_set_http_open_handler(mgserver[i], webserver_open_handler);
		mg_start_thread(webserver_serve, mgserver[i]);
		logprintf(LOG_DEBUG, "webserver started thread %d", i);
	}

	/* Register a seperate thread in which the webserver communicates
	   the main daemon as if it where a gui */
	threads_register("webserver client", &webserver_clientize, (void *)NULL);
	threads_register("webserver broadcast", &webserver_broadcast, (void *)NULL);

	logprintf(LOG_DEBUG, "webserver listening to port %s", webport);
	/* Main webserver loop */
	while(webserver_loop) {
		sleep(1);
	}

	return 0;
}
