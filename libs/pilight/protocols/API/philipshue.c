/*
	Copyright (C) 2014 CurlyMo

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
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <sys/stat.h>
#ifndef _WIN32
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
#endif
#include <pthread.h>

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

#include "../../core/threads.h"
#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../../core/socket.h"
#include "../../core/network.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/json.h"
#include "../../core/gc.h"
#include "philipshue.h"

#define INTERVAL 	60
#define HTTP_GET	0
#define HTTP_POST	1
#define HTTP_PUT	2
#define HTTP_DELETE	3

typedef struct settings_t {
	char *bridgeip;
	char *username;
	int lightid;
	protocol_threads_t *thread;
	struct settings_t *next;
} settings_t;

static pthread_mutex_t lock;
static pthread_mutexattr_t attr;

static struct settings_t *settings = NULL;
static unsigned short loop = 1;
static unsigned short threads = 0;

/*****************************************************************************************************
 *
 * hue_request adds very simple HTTP GET/POST/PUT/DELETE to connect to Philips Hue Bridge Rest API
 *
 * host   = bridge IP Address
 * page   = the request (for example "/api/lights" )
 * method = HTTP_GET|HTTP_POST|HTTP_PUT|HTTP_DELETE
 * type   = should always return "application/json" otherwise something went wrong
 * code   = return code (should alway have 200)
 * size   = returned content size
 * post   = post data used in POST and PUT requests
 *
 *****************************************************************************************************/

char *hue_request(char *host, char *page, int method, char **type, int *code, int *size, char *post) {
	struct sockaddr_in serv_addr;
	int sockfd = 0, bytes = 0;
	int has_code = 0, has_type = 0;
	int pos = 0;
	size_t bufsize = BUFFER_SIZE;
	char ip[INET_ADDRSTRLEN+1], *content = NULL;
	char recvBuff[BUFFER_SIZE+1], *header = MALLOC(bufsize);
	unsigned short port = 80;
	size_t len = 0;

	*size = 0;

	if(header == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}

	memset(header, '\0', bufsize);
	memset(recvBuff, '\0', BUFFER_SIZE+1);
	memset(&serv_addr, '\0', sizeof(struct sockaddr_in));

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
		logprintf(LOG_ERR, "%s is not a valid bridgeip address", ip);
		*code = -1;
		goto exit;
	}
	serv_addr.sin_port = htons(port);

	if(socket_timeout_connect(sockfd, (struct sockaddr *)&serv_addr, 3) < 0) {
		logprintf(LOG_ERR, "Cannot connect to hue bridge %s", host);
		*code = -1;
		goto exit;
	}

	if(method == HTTP_POST) {
		len = (size_t)snprintf(&header[0], bufsize, "POST %s HTTP/1.0\r\nContent-Length: %d\r\n\r\n%s", page,(int)strlen(post), post);
		if(len >= bufsize) {
			bufsize += BUFFER_SIZE;
			if((header = REALLOC(header, bufsize)) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
		}
	} else if(method == HTTP_PUT) {
		len = (size_t)snprintf(&header[0], bufsize, "PUT %s HTTP/1.0\r\nContent-Length: %d\r\n\r\n%s", page,(int)strlen(post), post);
		if(len >= bufsize) {
			bufsize += BUFFER_SIZE;
			if((header = REALLOC(header, bufsize)) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
		}
	} else if(method == HTTP_GET) {
		len = (size_t)snprintf(&header[0], bufsize, "GET %s HTTP/1.0\r\n\r\n", page);
		if(len >= bufsize) {
			bufsize += BUFFER_SIZE;
			if((header = REALLOC(header, bufsize)) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
		}
	} else if(method == HTTP_DELETE) {
		len = (size_t)snprintf(&header[0], bufsize, "DELETE %s HTTP/1.0\r\n\r\n", page);
		if(len >= bufsize) {
			bufsize += BUFFER_SIZE;
			if((header = REALLOC(header, bufsize)) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
		}
	}

	if((bytes = send(sockfd, header, len, 0)) <= 0) {
		logprintf(LOG_ERR, "http send failed (%s)", header);
		*code = -1;
		goto exit;
	}

	char *nl = NULL;
	char *tp = *type;
	memset(recvBuff, '\0', sizeof(recvBuff));

	while(1) {
		bytes = recv(sockfd, recvBuff, BUFFER_SIZE, 0);

		if(bytes <= 0) {
			if(*size == 0) {
				logprintf(LOG_ERR, "http read failed (%s%s)", host, page);
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
	if(header) FREE(header);
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

char *hue_get(char *host, char *page, char **type, int *code, int *size) {
	return hue_request(host, page, HTTP_GET, type, code, size, NULL);
}

char *hue_post(char *host, char *page, char **type, int *code, int *size, char *post) {
	return hue_request(host, page, HTTP_POST, type, code, size, post);
}

char *hue_put(char *host, char *page, char **type, int *code, int *size, char *post) {
	return hue_request(host, page, HTTP_PUT, type, code, size, post);
}

char *hue_delete(char *host, char *page, char **type, int *code, int *size) {
	return hue_request(host, page, HTTP_DELETE, type, code, size, NULL);
}

static void *philipshueParse(void *param) {
	struct protocol_threads_t *thread = (struct protocol_threads_t *)param;
	struct JsonNode *json = (struct JsonNode *)thread->param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct JsonNode *jchild1 = NULL;
	struct JsonNode *node = NULL;
	struct JsonNode *jdata = NULL;
	struct JsonNode *jmain = NULL;
	struct JsonNode *jstate = NULL;
	struct JsonNode *jarray = NULL;
	struct settings_t *wnode = MALLOC(sizeof(struct settings_t));

	int interval = 60, event = 0;
	int firstrun = 1, nrloops = 0, timeout = 0;

	char url[1024];
	char *data = NULL;
	char typebuf[255], *tp = typebuf;
	char *bridgeip = NULL;
	char *username = NULL;
	int ret = 0, size = 0;

	memset(&typebuf, '\0', 255);

	if(wnode == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		json_find_string(jchild, "bridgeip", &bridgeip);
		json_find_string(jchild, "username", &username);
		while(jchild) {
			jchild1 = json_first_child(jchild);
			while(jchild1) {
				if(strcmp(jchild1->key, "lightid") == 0 && jchild1->tag == JSON_NUMBER) {
					wnode->lightid = (int)jchild1->number_;
				}
				jchild1 = jchild1->next;
			}
			jchild = jchild->next;
		}
	}

	if(bridgeip != NULL) {
		if((wnode->bridgeip = MALLOC(strlen(bridgeip)+1)) == NULL) {
			fprintf(stderr, "out of memory\n");
			exit(EXIT_FAILURE);
		}
		strcpy(wnode->bridgeip, bridgeip);
	} else {
		wnode->bridgeip = NULL;
		logprintf(LOG_ERR, "philipshue Cannot find Hue Brigde IP");
		return (void *)NULL;
	}
	if(username != NULL) {
		if((wnode->username = MALLOC(strlen(username)+1)) == NULL) {
			fprintf(stderr, "out of memory\n");
			exit(EXIT_FAILURE);
		}
		strcpy(wnode->username, username);
	} else {
		wnode->username = NULL;
		logprintf(LOG_ERR, "philipshue Cannot find Hue Brigde username");
		return (void *)NULL;
	}

	threads++;

	wnode->thread = thread;
	wnode->next = settings;
	settings = wnode;

	while(loop) {
		event = protocol_thread_wait(thread, INTERVAL, &nrloops);
		pthread_mutex_lock(&lock);
		if(loop == 0) {
			break;
		}

		timeout += INTERVAL;

		if(wnode->lightid == 1 ) {  // only running one loop/thread for lightid 1 to poll status for all lights from hue bridge every 60 seconds
			if(timeout >= interval || event != ETIMEDOUT || firstrun == 1) {
				timeout = 0;
				data = NULL;
				sprintf(url, "/api/%s/lights", username);
				data = hue_get(bridgeip, url, &tp, &ret, &size);
				if(ret == 200) {
					if(strstr(typebuf, "application/json") != NULL) {
						if(json_validate(data) == true) {
							if((jdata = json_decode(data)) != NULL) {
								if(jdata->tag == JSON_OBJECT) {
									json_foreach(jmain, jdata) {  // loop through lights { "1":{...}, "2":{...}, "3":{...},...}
									/*
									 * Example jdata:
									 *
									 *	{"1": {
									 *		 "state": {
									 *			"on": false,
									 *			"bri": 253,
									 *			"hue": 8625,
									 *			"sat": 252,
									 *			"effect": "none",
									 *			"xy": [ 0.5837, 0.3879 ],
									 *			"ct": 500,
									 *			"alert": "none",
									 *			"colormode": "xy",
									 *			"reachable": true
									 *		  },
									 *		  "type": "Extended color light",
									 *		  "name": "Hue Lamp",
									 *		  ...
									 *		  },
									 *	 "2": {...},
									 *	 ...
									 *	}
									 *
									 */

										if(isNumeric(jmain->key) != 0) { // this should not happen
											logprintf(LOG_ERR, "url = %s result key = %s is not a valid number. HTTP response = %s", url, jmain->key, data);
											continue;
										}

										JsonNode *code = json_mkobject();
										json_append_member(code, "lightid", json_mknumber(atof(jmain->key), 0));
										json_append_member(code, "bridgeip", json_mkstring(bridgeip));
										json_append_member(code, "username", json_mkstring(username));

										if((jstate = json_find_member(jmain, "state")) != NULL) {  // {"1":{"state":{...},"type":...,"name":...,...},...}
											int state = 1;
											if((node = json_find_member(jstate, "on")) != NULL && node->tag == JSON_BOOL) {
												if(! node->bool_)
													state = 0; // state=off if on=false
												json_append_member(code, "on", json_mkstring(node->bool_ ? "true" : "false"));
											}
											if((node = json_find_member(jstate, "bri")) != NULL && node->tag == JSON_NUMBER) {
												json_append_member(code, "bri", json_mknumber(node->number_, 0));
												json_append_member(code, "dimlevel", json_mknumber(node->number_ / 17, 0)); // dimlevel 15 = bri 255 = bri (17 * 15)
											}
											if((node = json_find_member(jstate, "hue")) != NULL && node->tag == JSON_NUMBER) {
												json_append_member(code, "hue", json_mknumber(node->number_, 0));
											}
											if((node = json_find_member(jstate, "sat")) != NULL && node->tag == JSON_NUMBER) {
												json_append_member(code, "sat", json_mknumber(node->number_, 0));
											}
											if((node = json_find_member(jstate, "effect")) != NULL && node->tag == JSON_STRING) {
												json_append_member(code, "effect", json_mkstring(node->string_));
											}
											if((node = json_find_member(jstate, "xy")) != NULL && node->tag == JSON_ARRAY) {
												jarray = node->children.head; // example array "xy":[0.5471,0.4144]
												if(jarray->tag == JSON_NUMBER)
													json_append_member(code, "x", json_mknumber(jarray->number_, 4));
												jarray = jarray->next;
												if(jarray->tag == JSON_NUMBER)
													json_append_member(code, "y", json_mknumber(jarray->number_, 4));
												json_delete(jarray);
											}
											if((node = json_find_member(jstate, "ct")) != NULL && node->tag == JSON_NUMBER) {
												json_append_member(code, "ct", json_mknumber(node->number_, 0));
											}
											if((node = json_find_member(jstate, "alert")) != NULL && node->tag == JSON_STRING) {
												json_append_member(code, "alert", json_mkstring(node->string_));
											}
											if((node = json_find_member(jstate, "colormode")) != NULL && node->tag == JSON_STRING) {
												json_append_member(code, "colormode", json_mkstring(node->string_));
											}
											if((node = json_find_member(jstate, "reachable")) != NULL && node->tag == JSON_BOOL) {
												if(! node->bool_)
													state = 0; // state=off is reachable false
												json_append_member(code, "reachable", json_mkstring(node->bool_ ? "true" : "false"));
											}
											if(state) {
												json_append_member(code, "state", json_mkstring("on"));
											} else {
												json_append_member(code, "state", json_mkstring("off"));
											}
											/* end state json object */
										} else {
											logprintf(LOG_NOTICE, "api philipshue json has no state key");
										}

										if(((jstate = json_find_member(jmain, "type")) != NULL)  && jstate->tag == JSON_STRING) {
											json_append_member(code, "type", json_mkstring(jstate->string_));
										} else {
											logprintf(LOG_NOTICE, "api philipshue json has no type key");
										}

										if(((jstate = json_find_member(jmain, "name")) != NULL)  && jstate->tag == JSON_STRING) {
											json_append_member(code, "name", json_mkstring(jstate->string_));
										} else {
											logprintf(LOG_NOTICE, "api philipshue json has no name key");
										}

										philipshue->message = json_mkobject();
										json_append_member(philipshue->message, "message", code);
										json_append_member(philipshue->message, "origin", json_mkstring("receiver"));
										json_append_member(philipshue->message, "protocol", json_mkstring(philipshue->id));

										if(pilight.broadcast != NULL) {
											pilight.broadcast(philipshue->id, philipshue->message, PROTOCOL);
										}
										json_delete(node);
										json_delete(jstate);
										json_delete(philipshue->message);
										philipshue->message = NULL;
									}
								}
								json_delete(jdata);
							} else {
								logprintf(LOG_NOTICE, "philipshue - response from api json could not be parsed");
							}
						}  else {
							logprintf(LOG_NOTICE, "philipshue - response was not in a valid json format");
						}
					} else {
						logprintf(LOG_NOTICE, "philipshue - response was not in a valid json format");
					}
				} else {
					logprintf(LOG_NOTICE, "philipshue - could not reach philipshue bridge, HTTP %d", ret);
				}
				if(data) {
					FREE(data);
				}
			}
			firstrun = 0;
		} else {
			logprintf(LOG_NOTICE, "philipshue - lightid = %d, only running one thread for lightid 1 to get status from Philips Hue Bridge.", wnode->lightid);
			break;
		}

		pthread_mutex_unlock(&lock);
	}

	pthread_mutex_unlock(&lock);

	threads--;
	return (void *)NULL;
}

static struct threadqueue_t *initDev(JsonNode *jdevice) {
	loop = 1;
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	json_free(output);

	struct protocol_threads_t *node = protocol_thread_init(philipshue, json);
	return threads_register("philipshue", &philipshueParse, (void *)node, 0);
}

static int createCode(JsonNode *code) {
	char *bridgeip = NULL;
	char *username = NULL;
	char *resource = NULL;
	struct JsonNode *node = NULL;
	char url[1024];

	char *post = NULL;
	char *data = NULL;
	char typebuf[255], *tp = typebuf;
	int ret = 0, size = 0;
	memset(&typebuf, '\0', 255);

	struct JsonNode *jdata = NULL;
	struct JsonNode *jmain = NULL;
	struct JsonNode *jchild = NULL;

	if(json_find_string(code, "bridgeip", &bridgeip) == 0 &&
			json_find_string(code, "username", &username) == 0 &&
			json_find_member(code, "newuser") == NULL ){

		if (strstr(progname, "daemon") == NULL && json_find_string(code, "resource", &resource) == 0) {
			sprintf(url, "/api/%s/%s", username, resource);
			logprintf(LOG_NOTICE, "philipshue - get data http://%s%s", bridgeip, url);
			data = hue_get(bridgeip, url, &tp, &ret, &size);  // running HTTP GET

			if((data != NULL) && (ret == 200) &&
					(strstr(typebuf, "application/json") != NULL) &&
					(json_validate(data) == true) &&
					((jdata = json_decode(data)) != NULL)) {
				/* show result from rest call */
				if (size <= 2) {
					logprintf(LOG_NOTICE, "philipshue - result empty");
				} else {
					json_foreach(jmain, jdata) {
						if(json_find_member(jmain, "error") != NULL) {
							logprintf(LOG_ERR, "%s", json_stringify(jmain, NULL));
						} else {
							logprintf(LOG_NOTICE, "%s = %s", jmain->key, json_stringify(jmain, NULL));
						}
					}
				}
			} else {
				logprintf(LOG_ERR, "philipshue - Invalid Hue Bridge response code=%d, type=%s, response data=%s", ret, typebuf, data);
			}
		}

		if(strstr(progname, "daemon") != NULL && json_find_string(code, "resource", &resource) != 0) {
				logprintf(LOG_DEBUG, "philipshue - Running on Philips Hue Bridge IP = %s", bridgeip);

				JsonNode *postcode = json_mkobject();
				if((node = json_find_member(code, "on")) != NULL && node->tag == JSON_STRING) { // if on == "true" then bool true else bool false
					json_append_member(postcode,  "on", json_mkbool((strcmp(node->string_, "true") == 0 )? 1 : 0));
				}
				/* Handle situation when using -t or -f or --on=<true|false> together */
				if((node = json_find_member(code, "on")) != NULL && node->tag == JSON_NUMBER) { // with -t option on == 1
					postcode = json_mkobject();  // just to be sure that only one on exists
					json_append_member(postcode,  "on", json_mkbool(1));  // on=true
				}
				if((node = json_find_member(code, "off")) != NULL && node->tag == JSON_NUMBER) { // with -f option off == 1
					postcode = json_mkobject();  // just to be sure that only one on exists
					json_append_member(postcode,  "on", json_mkbool(0));  // on=false
				}
				if((node = json_find_member(code, "bri")) != NULL && node->tag == JSON_NUMBER) {
					json_append_member(postcode,  "bri", json_mknumber(node->number_, 0));
				}
				if((node = json_find_member(code, "hue")) != NULL && node->tag == JSON_NUMBER) {
					json_append_member(postcode,  "hue", json_mknumber(node->number_, 0));
				}
				if((node = json_find_member(code, "sat")) != NULL && node->tag == JSON_NUMBER) {
					json_append_member(postcode,  "sat", json_mknumber(node->number_, 0));
				}
				if((node = json_find_member(code, "effect")) != NULL && node->tag == JSON_STRING) {
					json_append_member(postcode,  "effect", json_mkstring(node->string_));
				}
				JsonNode *jarray = json_mkarray(); // example array "xy":[0.5471,0.4144]
				if((node = json_find_member(code, "x")) != NULL && node->tag == JSON_NUMBER) {
					json_append_element(jarray, json_mknumber(node->number_, 4));
				}
				if((node = json_find_member(code, "y")) != NULL && node->tag == JSON_NUMBER) {
					json_append_element(jarray, json_mknumber(node->number_, 4));
				}
				if(json_find_member(code, "x") && json_find_member(code, "y")) { // make sure that both x any y are set
					json_append_member(postcode, "xy", jarray);
				}
				if((node = json_find_member(code, "ct")) != NULL && node->tag == JSON_NUMBER) {
					json_append_member(postcode,  "ct", json_mknumber(node->number_, 0));
				}
				if((node = json_find_member(code, "alert")) != NULL && node->tag == JSON_STRING) {
					json_append_member(postcode,  "alert", json_mkstring(node->string_));
				}
				if((node = json_find_member(code, "colormode")) != NULL && node->tag == JSON_STRING) {
					json_append_member(postcode,  "colormode", json_mkstring(node->string_));
				}
				if((node = json_find_member(code, "dimlevel")) != NULL && node->tag == JSON_NUMBER) {
					json_append_member(postcode,  "bri", json_mknumber(node->number_ * 17, 0)); // dimlevel 15 = bri 255 = bri (17 * 15)
				}
				/* now check that lightid or group or scene else exist */
				if ((node = json_find_member(code, "scene")) != NULL && node->tag == JSON_STRING){
					postcode = json_mkobject();  // erase all other options before applying scene id
					json_append_member(postcode, "scene", json_mkstring(node->string_));
					sprintf(url, "/api/%s/groups/0/action", username);  //apply scene id on all lights = group 0
				} else if((node = json_find_member(code, "group")) != NULL && node->tag == JSON_NUMBER) {
					sprintf(url, "/api/%s/groups/%g/action", username, node->number_);
				} else if((node = json_find_member(code, "lightid")) != NULL && node->tag == JSON_NUMBER) {
					sprintf(url, "/api/%s/lights/%g/state", username, node->number_);
				} else {
					logprintf(LOG_ERR, "Missing option lightid or group or scene, code = %s", json_stringify(code, NULL));
					return EXIT_FAILURE;
				}
				post = json_stringify(postcode, NULL);
				logprintf(LOG_DEBUG, "philipshue - post data = %s to http://%s%s", post, bridgeip, url);

				data = hue_put(bridgeip, url, &tp, &ret, &size, post);  // running HTTP PUT

				if((data != NULL) && (ret == 200) &&
						(strstr(typebuf, "application/json") != NULL) &&
						(json_validate(data) == true) &&
						((jdata = json_decode(data)) != NULL)) {
					/* show response from rest call success or error*/
					json_foreach(jmain, jdata) {
						logprintf(LOG_DEBUG, "philipshue - response %s", json_stringify(jmain, NULL));
					}

				} else {
					logprintf(LOG_ERR, "philipshue - Invalid Hue Bridge response code=%d, type=%s, response data=%d", ret, typebuf, data);
				}
			}
	} else if (strstr(progname, "daemon") == NULL &&
			json_find_string(code, "bridgeip", &bridgeip) == 0 &&
			json_find_member(code, "newuser") != NULL) {

		/* Create and authorize new user for pilight.
		 * Run pilight-send -p philipshue --ip=<ip> --newuser
		 * Press Link Button on Bridge.
		 * The bridge creates a randomly generated username.
		 * Run the command again, the bridge displays success and displays the username.
		 */
		logprintf(LOG_NOTICE, "Create and authorize new user for pilight on Hue Bridge ...");
		sprintf(url, "/api");
		JsonNode *postcode = json_mkobject();
		json_append_member(postcode,  "devicetype", json_mkstring("pilight"));
		post = json_stringify(postcode, NULL);
		logprintf(LOG_NOTICE, "philipshue - post data = %s to url http://%s%s", post, bridgeip, url);
		data = hue_post(bridgeip, url, &tp, &ret, &size, post);  // running HTTP PUT

		logprintf(LOG_NOTICE, "response from hue bridge = %s", data);
		if((data != NULL) && (ret == 200) &&
				(strstr(typebuf, "application/json") != NULL) &&
				(json_validate(data) == true) &&
				((jdata = json_decode(data)) != NULL)) {
			/* show response from rest call success or error */
			json_foreach(jmain, jdata) {
				jchild = json_first_child(jmain);
				if(strcmp(jchild->key, "error") == 0) {
					if( (node = json_find_member(jchild, "description")) != NULL)
						logprintf(LOG_NOTICE, "description = %s", node->string_);
					logprintf(LOG_NOTICE, "----> Press Link button and run again pilight-send -p philipshue --ip=%s --newuser ", bridgeip);
				}
				if(strcmp(jchild->key, "success") == 0) {
					if( (node = json_find_member(jchild, "username")) != NULL)
						logprintf(LOG_NOTICE, "----> Successfully created and authorized username =  %s <--- Use this username", node->string_);
				}
			}
		}
	} else if (strstr(progname, "daemon") != NULL &&
				json_find_member(code, "newuser") != NULL) {

		/* doining nothing on daemon when creating new user */
		logprintf(LOG_NOTICE, "Create and authorize new user for pilight on Hue Bridge.");
		logprintf(LOG_NOTICE, "Press Link button and run pilight-send -p philipshue --ip=%s --newuser ", bridgeip);
	} else {
		logprintf(LOG_ERR, "philipshue: insufficient number of arguments %s",  json_stringify(code, NULL));
		return EXIT_FAILURE;
	}

	if(data) FREE(data);
	return EXIT_SUCCESS;
}

static void threadGC(void) {
	loop = 0;
	protocol_thread_stop(philipshue);
	while(threads > 0) {
		usleep(10);
	}
	protocol_thread_free(philipshue);

	struct settings_t *tmp = NULL;
	while(settings) {
		tmp = settings;
		if(settings->bridgeip) FREE(settings->bridgeip);
		if(settings->username) FREE(settings->username);
		settings = settings->next;
		FREE(tmp);
	}
}

static void printHelp(void) {
	printf("\t -i --bridgeip=<ipaddress>\t\tPhilips Hue Bridge IP Address\n");
	printf("\t -l --lightid=<lightid>\t\t\tlightid (1,2,3,..)\n");
	printf("\t -u --username=<username>\t\tusername for bridge api\n");
	printf("\t -o --on=<true|false>\t\t\tset light state on to true|false\n");
	printf("\t -b --bri=<0..254>\t\t\tbrightness of a light from 0 to 254, this value is changed by dimlevel\n");
	printf("\t -h --hue=<0..65535>\t\t\thue color value red=0 / yellow=12750 / green=25500 / blue=46920 / purple=56100 / red=65280\n");
	printf("\t -s --sat=<0..255>\t\t\tsaturation from 0 to 255\n");
	printf("\t -e --effect=<none|colorloop>\t\teffect none|colorloop (endless color looping mode until it is stopped by sending none)\n");
	printf("\t -x --x=<0..0.9999>\t\t\tx value in xy color points\n");
	printf("\t -y --y=<0..0.9999>\t\t\ty value in xy color points\n");
	printf("\t -c --ct=<153..500>\t\t\tcolor temperature (warmest color is 500)\n");
	printf("\t -a --alert=<none|select>\t\talert=select makes the light do a blink in its current color\n");
	printf("\t -g --group=<0..>\t\t\trunning on group of lights. The group must have been created before (group 0 always includes all lights)\n");
	printf("\t -z --scene=<scene-id>\t\t\tapply a predefined scene id (use option --resource=scenes to list all scenes on hue bridge)\n");
	printf("\t -r --resource=<resource>\t\tlist resource lights|groups|config|schedules|scenes|sensors|rules\n");
	printf("\t -t --on\t\t\t\tturn light or group on\n");
	printf("\t -f --off\t\t\t\tturn light or group off\n");
	printf("\t -n --newuser\t\t\t\tcreate and authorize user on Hue Bridge. Press link button on bridge and run pilight-send -p philipshue --bridgeip=<ip> --newuser again.\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void philipshueInit(void) {
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&lock, &attr);

	protocol_register(&philipshue);
	protocol_set_id(philipshue, "philipshue");
	protocol_device_add(philipshue, "philipshue", "Philips Hue API");
	philipshue->devtype = DIMMER;
	philipshue->hwtype = API;

	/*
	 *	"on": false,
	 *	"bri": 253,
	 *	"hue": 8625,
	 *	"sat": 252,
	 *	"effect": "none",
	 *	"x":  0.5837,     //  "xy": [ 0.5837, 0.3879 ],
	 *	"y":  0.3879,
	 *	"ct": 500,
	 *	"alert": "none",
	 */
	options_add(&philipshue->options, 'i', "bridgeip", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, NULL);
	options_add(&philipshue->options, 'l', "lightid",  OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, NULL);
	options_add(&philipshue->options, 'u', "username", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "^[a-zA-Z0-9]+$");
	options_add(&philipshue->options, 'o', "on",    OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&philipshue->options, 'b', "bri", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&philipshue->options, 'h', "hue", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");
	options_add(&philipshue->options, 's', "sat", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&philipshue->options, 'e', "effect", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, "none|colorloop");
	options_add(&philipshue->options, 'x', "x", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&philipshue->options, 'y', "y", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&philipshue->options, 'c', "ct", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&philipshue->options, 'a', "alert", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, "none|select");
	options_add(&philipshue->options, 'g', "group", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&philipshue->options, 'z', "scene", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&philipshue->options, 'r', "resource", OPTION_HAS_VALUE, DEVICES_OPTIONAL, JSON_STRING, NULL, NULL);
	options_add(&philipshue->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&philipshue->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&philipshue->options, 'n', "newuser", OPTION_NO_VALUE, DEVICES_OPTIONAL, JSON_STRING, NULL, NULL);

	options_add(&philipshue->options, 'd', "dimlevel", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^([0-9]{1,})$");
	options_add(&philipshue->options, 0, "dimlevel-minimum", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "^([0-9]{1}|[1][0-5])$");
	options_add(&philipshue->options, 0, "dimlevel-maximum", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)15, "^([0-9]{1}|[1][0-5])$");

	philipshue->createCode=&createCode;
	philipshue->initDev=&initDev;
	philipshue->threadGC=&threadGC;
	philipshue->printHelp=&printHelp;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "philipshue";
	module->version = "0.1";
	module->reqversion = "7.0";
	module->reqcommit = "84";
}

void init(void) {
	philipshueInit();
}
#endif
