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
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <pthread.h>
#include <ctype.h>

#include "settings.h"
#include "config.h"
#include "gc.h"
#include "log.h"
#include "options.h"
#include "socket.h"
#include "json.h"
#include "webserver.h"
#include "wiringPi.h"
#include "irq.h"
#include "hardware.h"
#include "pilight.h"

typedef enum {
	RECEIVER,
	SENDER,
	CONTROLLER,
	NODE,
	GUI,
	WEB
} client_type_t;

char clients[6][11] = {
	"receiver\0",
	"sender\0",
	"controller\0",
	"node\0",
	"gui\0",
	"web\0"
};

typedef enum {
	WELCOME,
	IDENTIFY,
	REJECT,
	REQUEST,
	CONFIG,
	FORWARD
} steps_t;

/* The pid_file and pid of this daemon */
char *pid_file;
pid_t pid;
/* The to call when a new code is send or received */
char *process_file;
/* The duration of the received pulse */
int duration = 0;
/* The number of receivers connected */
int receivers = 0;
/* Daemonize or not */
int nodaemon = 0;
/* Are we already running */
int running = 1;
/* How many times does the code need to be resend */
int send_repeat;
/* How many times does a code need to received*/
int receive_repeat = RECEIVE_REPEATS;
/* Are we currently sending code */
int sending = 0;
/* If we have accepted a client, handshakes will store the type of client */
short handshakes[MAX_CLIENTS];
/* Which mode are we running in: 1 = server, 2 = client */
unsigned short runmode = 1;
/* Socket identifier to the server if we are running as client */
int sockfd = 0;
/* In the client running in incognito mode */
unsigned short incognito_mode = 0;
/* Use the lirc_rpi module or plain GPIO */
const char *hw_mode = HW_MODE;
/* On what pin are we sending */
int gpio_out = GPIO_OUT_PIN;
/* On what pin are we receiving */
int gpio_in = GPIO_IN_PIN;
/* Do we enable the webserver */
int webserver_enable = WEBSERVER_ENABLE;
/* On what port does the webserver run */
int webserver_port = WEBSERVER_PORT;
/* The webroot of pilight */
char *webserver_root;
/* Thread pointers */
pthread_t pth1, pth2, pth3, pth4;
/* While loop conditions */
unsigned short main_loop = 1;
/* Only update the config file on exit when it's valid */
unsigned short valid_config = 1;
/* How many received repeats did we encounter */
int repeats = 0;
/* Reset repeats after a certian amount of time */
unsigned long first = 0;
unsigned long second = 0;
struct timeval tv;

/* http://stackoverflow.com/a/3535143 */
void escape_characters(char* dest, const char* src) {
	char c;

	while((c = *(src++))) {
		switch(c) {
			case '\a':
				*(dest++) = '\\';
				*(dest++) = 'a';
			break;
			case '\b':
				*(dest++) = '\\';
				*(dest++) = 'b';
			break;
			case '\t':
				*(dest++) = '\\';
				*(dest++) = 't';
			break;
			case '\n':
				*(dest++) = '\\';
				*(dest++) = 'n';
			break;
			case '\v':
				*(dest++) = '\\';
				*(dest++) = 'v';
			break;
			case '\f':
				*(dest++) = '\\';
				*(dest++) = 'f';
			break;
			case '\r':
				*(dest++) = '\\';
				*(dest++) = 'r';
			break;
			case '\\':
				*(dest++) = '\\';
				*(dest++) = '\\';
			break;
			case '\"':
				*(dest++) = '\\';
				*(dest++) = '\"';
			break;
			default:
				*(dest++) = c;
			}
	}
	*dest = '\0'; /* Ensure nul terminator */
}

void *call_process_file(void *param) {
	FILE *f;
	char *cmd = malloc(strlen(process_file)+strlen((char *)param)+2);
	sprintf(cmd, "%s %s", process_file, (char *)param);
	f=popen(cmd, "r");
	pclose(f);
	free(cmd);
	free(param);
	pthread_exit((void *)NULL);
}

int broadcast(char *protoname, JsonNode *json) {

	int i = 0, broadcasted = 0;
	char *message = json_stringify(json, NULL);

	JsonNode *jret = NULL;

	/* Update the config */
	if(config_update(protoname, json, &jret) == 0) {
		char *conf = json_stringify(jret, NULL);

		for(i=0;i<MAX_CLIENTS;i++) {
			if(handshakes[i] == GUI) {
				socket_write(socket_clients[i], conf);
				broadcasted = 1;
			}
		}

		if(broadcasted == 1) {
			logprintf(LOG_DEBUG, "broadcasted: %s", conf);
		}
		free(conf);
	}
	if(jret) {
		json_delete(jret);
	}
	broadcasted = 0;

	if(json_validate(message) == true && receivers > 0) {

		/* Write the message to all receivers */
		for(i=0;i<MAX_CLIENTS;i++) {
			if(handshakes[i] == RECEIVER || handshakes[i] == NODE) {
				socket_write(socket_clients[i], message);
				broadcasted = 1;
			}
		}

		char *escaped = malloc(2 * strlen(message) + 1);
		escape_characters(escaped, message);

		if(process_file && strlen(process_file) > 0) {
			/* Call the external file in a seperate thread to make
			   it non-blocking. */
			if(strlen(process_file) > 0) {
				if(pth4) {
					pthread_cancel(pth4);
					pthread_join(pth4, NULL);
				}
				char *param = malloc(strlen(escaped)+1);
				strcpy(param, escaped);
				pthread_create(&pth4, NULL, &call_process_file, (void *)param);
				usleep(100);
				broadcasted = 1;
			}
			if(broadcasted) {
				logprintf(LOG_DEBUG, "broadcasted: %s", message);
			}
		}
		free(message);
		free(escaped);
		return 1;
	}
	free(message);
	return 0;
}

void receiver_create_message(protocol_t *protocol, int rep) {
	if(protocol->message) {
		char *valid = json_stringify(protocol->message, NULL);
		json_delete(protocol->message);
		if(valid && json_validate(valid) == true) {
			struct protocol_conflicts_t *tmp_conflicts = NULL;

			tmp_conflicts = protocol->conflicts;
			JsonNode *jmessage = json_mkobject();

			json_append_member(jmessage, "code", json_decode(valid));
			json_append_member(jmessage, "origin", json_mkstring("receiver"));
			json_append_member(jmessage, "protocol", json_mkstring(protocol->id));
			if(rep > -1) {
				json_append_member(jmessage, "repeats", json_mknumber(rep));
			}
			broadcast(protocol->id, jmessage);
			json_delete(jmessage);
			while(tmp_conflicts) {
				jmessage = json_mkobject();
				json_append_member(jmessage, "code", json_decode(valid));
				json_append_member(jmessage, "origin", json_mkstring("receiver"));
				json_append_member(jmessage, "protocol", json_mkstring(tmp_conflicts->id));
				if(rep > -1) {
					json_append_member(jmessage, "repeats", json_mknumber(rep));
				}
				broadcast(tmp_conflicts->id, jmessage);
				tmp_conflicts = tmp_conflicts->next;
				json_delete(jmessage);
			}
		}
		protocol->message = NULL;
		free(valid);
	}
}

void receiver_parse_code(int *rawcode, int rawlen, int plslen) {
	struct protocol_t *protocol = NULL;
	struct protocols_t *pnode = protocols;
	int x = 0;

	while(pnode) {
		protocol = pnode->listener;
		if((((protocol->parseRaw || protocol->parseCode) && protocol->rawlen > 0)
		   || protocol->parseBinary) && protocol->pulse > 0 && protocol->plslen > 0) {
			if(rawlen == protocol->rawlen && (plslen > ((double)protocol->plslen-3) && plslen < ((double)protocol->plslen+3))) {
				if(protocol->parseRaw) {
					logprintf(LOG_DEBUG, "called %s parseRaw()", protocol->id);

					protocol->parseRaw();
					receiver_create_message(protocol, -1);
					//continue;
				}

				/* Convert the raw codes to one's and zero's */
				for(x=0;x<(int)(double)rawlen;x++) {
					protocol->pCode[x] = protocol->code[x];
					memcpy(&protocol->raw[x], &rawcode[x], sizeof(int));
					if(protocol->raw[x] >= ((protocol->pulse*plslen)-plslen)) {
						protocol->code[x]=1;
					} else {
						protocol->code[x]=0;
					}
					/* Check if the current code matches the previous one */
					if(protocol->pCode[x] != protocol->code[x]) {
						repeats=0;
						first = 0;
						second = 0;
					}
				}

				gettimeofday(&tv, NULL);
				if(first > 0) {
					first = second;
				}
				second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;
				if(first == 0) {
					first = second;
				}

				/* Reset # of repeats after a certain delay */
				if(((int)second-(int)first) > 1000000) {
					repeats = 0;
				}

				repeats++;
				/* Continue if we have recognized enough repeated codes */
				if(repeats >= (receive_repeat*protocol->rxrpt)) {
					if(protocol->parseCode) {
						logprintf(LOG_DEBUG, "caught minimum # of repeats %d of %s", repeats, protocol->id);
						logprintf(LOG_DEBUG, "called %s parseCode()", protocol->id);

						protocol->parseCode(repeats);
						receiver_create_message(protocol, repeats);
						//continue;
					}

					if(protocol->parseBinary) {
						/* Convert the one's and zero's into binary */
						for(x=0; x<(int)(double)rawlen; x+=4) {
							if(protocol->code[x+protocol->lsb] == 1) {
								protocol->binary[x/4]=1;
							} else {
								protocol->binary[x/4]=0;
							}
						}

						if((double)protocol->raw[1]/((double)protocol->pulse*(double)protocol->plslen) < 1.5) {
							x -= 4;
						}

						/* Check if the binary matches the binary length */
						if((protocol->binlen > 0 && ((x/4) == protocol->binlen)) || (protocol->binlen == 0 && ((x/4) == protocol->rawlen/4))) {
							logprintf(LOG_DEBUG, "called %s parseBinary()", protocol->id);

							protocol->parseBinary(repeats);
							receiver_create_message(protocol, repeats);
							//continue;
						}
					}
				}
			}
		}
		pnode = pnode->next;
	}
}

/* Send a specific code */
void send_code(JsonNode *json) {
	int i = 0, match = 0, x = 0, logged = 0;

	/* Hold the final protocol struct */
	struct protocol_t *protocol = NULL;
	struct hardware_t *hardware = NULL;

	JsonNode *jcode = NULL;
	JsonNode *jsettings = NULL;
	JsonNode *jprotocols = NULL;
	JsonNode *jprotocol = NULL;
	JsonNode *message = NULL;

	if(!(jcode = json_find_member(json, "code"))) {
		logprintf(LOG_ERR, "sender did not send any codes");
		json_delete(jcode);
	} else if(!(jprotocols = json_find_member(jcode, "protocol"))) {
		logprintf(LOG_ERR, "sender did not provide a protocol name");
		json_delete(jcode);
	} else {
		logged = 0;
		/* If we matched a protocol and are not already sending, continue */
		if(sending == 0 && send_repeat > 0) {
			jprotocol = json_first_child(jprotocols);
			while(jprotocol) {
				match = 0;
				logged = 0;
				if(jprotocol->tag == JSON_STRING) {
					struct protocols_t *pnode = protocols;
					/* Retrieve the used protocol */
					while(pnode) {
						protocol = pnode->listener;
						/* Check if the protocol exists */
						if(protocol_device_exists(protocol, jprotocol->string_) == 0 && match == 0) {
							match = 1;
							break;
						}
						pnode = pnode->next;
					}
				}

				if(match == 1 && protocol->createCode) {
					/* Temporary alter the protocol specific settings */
					if((jsettings = json_find_member(jcode, "settings"))) {
						JsonNode *jchild = NULL;
						if((jchild = json_first_child(jsettings))) {
							while(jchild) {
								if(jchild->tag == JSON_NUMBER) {
									if(protocol_setting_check_number(protocol, jchild->key, (int)jchild->number_) == 0) {
										protocol_setting_update_number(protocol, jchild->key, (int)jchild->number_);
									}
								} else if(jchild->tag == JSON_STRING) {
									if(protocol_setting_check_string(protocol, jchild->key, jchild->string_) == 0) {
										protocol_setting_update_string(protocol, jchild->key, jchild->string_);
									}
								}
								jchild = jchild->next;
							}
							json_delete(jchild);
						}
					}

					/* Let the protocol create his code */
					if(protocol->createCode(jcode) == 0) {
						message = json_mkobject();
						if(protocol->message) {
							char *valid = json_stringify(protocol->message, NULL);
							json_delete(protocol->message);
							if(json_validate(valid) == true) {
								json_append_member(message, "origin", json_mkstring("sender"));
								json_append_member(message, "protocol", json_mkstring(protocol->id));
								json_append_member(message, "code", json_decode(valid));
								json_append_member(message, "repeat", json_mknumber(1));
							}
							protocol->message = NULL;
							free(valid);
						}

						sending = 1;

						struct hardwares_t *htmp = hardwares;
						match = 0;
						while(htmp) {
							if(strcmp(htmp->listener->id, hw_mode) == 0) {
								hardware = htmp->listener;
								match = 1;
								break;
							}
							htmp = htmp->next;
						}

						if(match == 1) {
							/* Create a single code with all repeats included */
							int code_len = (protocol->rawlen*send_repeat*protocol->txrpt)+1;
							size_t send_len = (size_t)(code_len * (int)sizeof(int));
							int longCode[code_len];
							memset(longCode, 0, send_len);

							for(i=0;i<(send_repeat*protocol->txrpt);i++) {
								for(x=0;x<protocol->rawlen;x++) {
									longCode[x+(protocol->rawlen*i)]=protocol->raw[x];
								}
							}

							longCode[code_len] = 0;
							if(hardware->send) {
								if(hardware->send(longCode) == 0) {
									logprintf(LOG_DEBUG, "successfully send %s code", protocol->id);
									if(logged == 0) {
										logged = 1;
										if(strcmp(protocol->id, "raw") == 0) {
											int plslen = protocol->raw[protocol->rawlen-1]/PULSE_DIV;
											receiver_parse_code(protocol->raw, protocol->rawlen, plslen);
										} else {
											/* Write the message to all receivers */
											broadcast(protocol->id, message);
										}
										json_delete(message);
										message = NULL;
									}
								} else {
									logprintf(LOG_ERR, "failed to send code");
								}
							} else if(logged == 0) {
								logged = 1;
								if(strcmp(protocol->id, "raw") == 0) {
									int plslen = protocol->raw[protocol->rawlen-1]/PULSE_DIV;
									receiver_parse_code(protocol->raw, protocol->rawlen, plslen);
								} else {
									/* Write the message to all receivers */
									broadcast(protocol->id, message);
								}
								json_delete(message);
								message = NULL;
							}
						}

						sending = 0;
					}

					/* Restore the protocol specific settings to their default values */
					if((jsettings = json_find_member(jcode, "settings"))) {
						JsonNode *jchild = NULL;
						if((jchild = json_first_child(jsettings))) {
							while(jchild) {
								if(jchild->tag == JSON_NUMBER) {
									if(protocol_setting_check_number(protocol, jchild->key, (int)jchild->number_) == 0) {
										protocol_setting_restore(protocol, jchild->key);
									}
								} else if(jchild->tag == JSON_STRING) {
									if(protocol_setting_check_string(protocol, jchild->key, jchild->string_) == 0) {
										protocol_setting_restore(protocol, jchild->key);
									}
								}
								jchild = jchild->next;
							}
							json_delete(jchild);
						}
					}
				}
				jprotocol = jprotocol->next;
			}
		}
		if(message) {
			json_delete(message);
			message = NULL;
		}
		if(jcode) {
			json_delete(jcode);
		}
	}
}

void client_sender_parse_code(int i, JsonNode *json) {
	int sd = socket_clients[i];

	if(incognito_mode == 0 && handshakes[i] != NODE) {
		/* Don't let the sender wait until we have send the code */
		socket_close(sd);
		handshakes[i] = -1;
	}

	send_code(json);
}

void control_device(struct conf_devices_t *dev, char *state, JsonNode *values) {
	struct conf_settings_t *sett = NULL;
	struct conf_values_t *val = NULL;
	struct options_t *opt = NULL;
	struct protocols_t *tmp_protocols = NULL;
	unsigned short has_settings = 0;

	char *ctmp = NULL;

	JsonNode *code = json_mkobject();
	JsonNode *json = json_mkobject();
	JsonNode *jsettings = json_mkobject();
	JsonNode *jprotocols = json_mkarray();

	/* Check all protocol options */
	tmp_protocols = dev->protocols;
	while(tmp_protocols) {
		json_append_element(jprotocols, json_mkstring(tmp_protocols->name));
		if((opt = tmp_protocols->listener->options)) {
			while(opt) {
				sett = dev->settings;
				while(sett) {
					/* Retrieve the device id's */
					if(strcmp(sett->name, "id") == 0) {
						val = sett->values;
						while(val) {
							if(opt->conftype == config_id && strcmp(val->name, opt->name) == 0 && json_find_string(code, val->name, &ctmp) != 0) {
								json_append_member(code, val->name, json_mkstring(val->value));
							}
							val = val->next;
						}
					}

					/* Retrieve the protocol specific settings */
					if(strcmp(sett->name, "settings") == 0 && !has_settings) {
						has_settings = 1;
						val = sett->values;
						while(val) {
							if(val->type == CONFIG_TYPE_NUMBER) {
								json_append_member(jsettings, val->name, json_mknumber(atoi(val->value)));
							} else if(val->type == CONFIG_TYPE_STRING) {
								json_append_member(jsettings, val->name, json_mkstring(val->value));
							}
							val = val->next;
						}
					}
					sett = sett->next;
				}
				opt = opt->next;
			}
			while(values) {
				opt = tmp_protocols->listener->options;
				while(opt) {
					if(opt->conftype == config_value && strcmp(values->key, opt->name) == 0) {
						if(values->tag == JSON_STRING) {
							json_append_member(code, values->key, json_mkstring(values->string_));
						} else if(values->tag == JSON_NUMBER) {
							ctmp = realloc(ctmp, sizeof(int));
							sprintf(ctmp, "%d", (int)values->number_);
							json_append_member(code, values->key, json_mkstring(ctmp));
							free(ctmp);
						}
					}
					opt = opt->next;
				}
				values = values->next;
			}
		}
		/* Send the new device state */
		if((opt = tmp_protocols->listener->options)) {
			while(opt) {
				if(json_find_member(code, opt->name) == NULL) {
					if(opt->conftype == config_state && opt->argtype == no_value && strcmp(opt->name, state) == 0) {
						json_append_member(code, opt->name, json_mkstring("1"));
						break;
					} else if(opt->conftype == config_state && opt->argtype == has_value) {
						json_append_member(code, opt->name, json_mkstring(state));
						break;
					}
				}
				opt = opt->next;
			}
		}
		tmp_protocols = tmp_protocols->next;
	}

	/* Construct the right json object */
	json_append_member(code, "protocol", jprotocols);
	json_append_member(json, "message", json_mkstring("sender"));
	json_append_member(json, "code", code);
	json_append_member(code, "settings", jsettings);

	send_code(json);

	json_delete(json);
}

void client_node_parse_code(int i, JsonNode *json) {
	int sd = socket_clients[i];
	char *message = NULL;

	if(json_find_string(json, "message", &message) == 0) {
		/* Send the config file to the controller */
		if(strcmp(message, "request config") == 0) {
			JsonNode *jsend = json_mkobject();
			JsonNode *joutput = config2json(1);
			json_append_member(jsend, "config", joutput);
			char *output = json_stringify(jsend, NULL);
			socket_write_big(sd, output);
			free(output);
			json_delete(jsend);
		}
	}
}

void client_controller_parse_code(int i, JsonNode *json) {
	int sd = socket_clients[i];
	char *message = NULL;
	char *location = NULL;
	char *device = NULL;
	char *tmp = NULL;
	struct conf_locations_t *slocation;
	struct conf_devices_t *sdevice;
	JsonNode *code = NULL;
	JsonNode *values = NULL;

	if(json_find_string(json, "message", &message) == 0) {
		/* Send the config file to the controller */
		if(strcmp(message, "request config") == 0) {
			JsonNode *jsend = json_mkobject();
			JsonNode *joutput = config2json(1);
			json_append_member(jsend, "config", joutput);
			char *output = json_stringify(jsend, NULL);
			socket_write_big(sd, output);
			json_delete(jsend);
			free(output);
		/* Control a specific device */
		} else if(strcmp(message, "send") == 0) {
			/* Check if got a code */
			if(!(code = json_find_member(json, "code"))) {
				logprintf(LOG_ERR, "controller did not send any codes");
			} else {
				/* Check if a location and device are given */
				if(json_find_string(code, "location", &location) != 0) {
					logprintf(LOG_ERR, "controller did not send a location");
				} else if(json_find_string(code, "device", &device) != 0) {
					logprintf(LOG_ERR, "controller did not send a device");
				/* Check if the device and location exists in the config file */
				} else if(config_get_location(location, &slocation) == 0) {
					if(config_get_device(location, device, &sdevice) == 0) {
						char *state = malloc(4);
						if(json_find_string(code, "state", &tmp) == 0) {
							state = realloc(state, strlen(tmp)+1);
							strcpy(state, tmp);
						} else {
							state = realloc(state, 4);
							memset(state, '\0', 4);
						}
						/* Send the device code */
						values = json_find_member(code, "values");
						if(values) {
							values = json_first_child(values);
						}
						control_device(sdevice, state, values);
						free(state);
					} else {
						logprintf(LOG_ERR, "the device \"%s\" does not exist", device);
					}
				} else {
					logprintf(LOG_ERR, "the location \"%s\" does not exist", location);
				}
				if(incognito_mode == 0 && handshakes[i] != GUI && handshakes[i] != NODE) {
					socket_close(sd);
					handshakes[i] = -1;
				}
			}
		}
	}
}

void client_webserver_parse_code(int i, char buffer[BUFFER_SIZE]) {
	int sd = socket_clients[i];
	int x = 0;
	FILE *f;
	char *ptr = NULL;;
	char *cache = NULL;
	char *path = NULL;
	struct sockaddr_in sockin;
	socklen_t len = sizeof(sockin);

	if(getsockname(sd, (struct sockaddr *)&sockin, &len) == -1) {
		logprintf(LOG_ERR, "could not determine server ip address");
	} else {
		ptr = malloc(4);
		ptr = strstr(buffer, " HTTP/");
		*ptr = 0;
		ptr = NULL;

		if(strncmp(buffer, "GET ", 4) == 0) {
			ptr = buffer + 4;
		}
		if(strcmp(ptr, "/logo.png") == 0) {
			if(ptr == buffer + 4) {
				socket_write(sd, "HTTP/1.1 200 OK\r");
				socket_write(sd, "Content-Type: image/png\r");
				socket_write(sd, "Server : pilight webserver\r");
				socket_write(sd, "\r");

				path = malloc(strlen(webserver_root)+strlen("logo.png")+1);
				sprintf(path, "%slogo.png", webserver_root);
				f = fopen(path, "rb");
				if(f) {
					x = 0;
					cache = malloc(BUFFER_SIZE);
					while (!feof(f)) {
						x = (int)fread(cache, 1, BUFFER_SIZE, f);
						send(sd, cache, (size_t)x, MSG_NOSIGNAL);
					}
					fclose(f);
					free(cache);
					free(path);
				} else {
					logprintf(LOG_NOTICE, "pilight logo not found");
				}
			}
		} else {
		    /* Catch all webserver page to inform users on which port the webserver runs */
			socket_write(sd, "HTTP/1.1 200 OK\r");
			socket_write(sd, "Server : pilight webserver\r");
			socket_write(sd, "\r");
			socket_write(sd, "<html><head><title>pilight daemon</title></head>\r");
			cache = malloc(BUFFER_SIZE);
			if(webserver_enable == 1) {
				sprintf(cache, "<body><center><img src=\"logo.png\"><br /><p style=\"color: #0099ff; font-weight: 800px; font-family: Verdana; font-size: 20px;\">The pilight webgui is located at <a style=\"text-decoration: none; color: #0099ff; font-weight: 800px; font-family: Verdana; font-size: 20px;\" href=\"http://%s:%d\">http://%s:%d</a></p></center></body></html>\r", inet_ntoa(sockin.sin_addr), webserver_port, inet_ntoa(sockin.sin_addr), webserver_port);
				socket_write(sd, cache);
			} else {
				socket_write(sd, "<body><center><img src=\"logo.png\"></center></body></html>\r");
			}
			free(cache);
		}
	}
}

/* Parse the incoming buffer from the client */
void socket_parse_data(int i, char buffer[BUFFER_SIZE]) {
	int sd = socket_clients[i];
	struct sockaddr_in address;
	int addrlen = sizeof(address);
	char *message;
	char *incognito;
	JsonNode *json = NULL;
	short x = 0;

	getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);

	if(strcmp(buffer, "HEART\n") != 0) {
		logprintf(LOG_DEBUG, "socket recv: %s", buffer);
	}

	if(strcmp(buffer, "HEART\n") == 0) {
		socket_write(sd, "BEAT");
	} else {
		/* Serve static webserver page. This is the only request that's is
		   expected not to be a json object */
		if(strstr(buffer, " HTTP/")) {
			logprintf(LOG_INFO, "client recognized as web");
			handshakes[i] = WEB;
			client_webserver_parse_code(i, buffer);
			socket_close(sd);
		} else if(json_validate(buffer) == true) {
			json = json_decode(buffer);
			/* The incognito mode is used by the daemon to emulate certain clients.
			   Temporary change the client type from the node mode to the emulated
			   client mode. */
			if(json_find_string(json, "incognito", &incognito) == 0) {
				incognito_mode = 1;
				for(x=0;x<(sizeof(clients)/sizeof(clients[0]));x++) {
					if(strcmp(clients[x], incognito) == 0) {
						handshakes[i] = x;
						break;
					}
				}
			} else if(json_find_string(json, "message", &message) == 0) {
				if(handshakes[i] != NODE && handshakes[i] != RECEIVER && handshakes[i] > -1) {
					if(runmode == 2 && sockfd > 0 && strcmp(message, "request config") != 0) {
						socket_write(sockfd, "{\"incognito\":\"%s\"}", clients[handshakes[i]]);
						socket_write(sockfd, buffer);
					}
				}
				if(handshakes[i] == NODE) {
					client_node_parse_code(i, json);
				} else if(handshakes[i] == SENDER) {
					client_sender_parse_code(i, json);
				} else if(handshakes[i] == CONTROLLER || handshakes[i] == GUI) {
					client_controller_parse_code(i, json);
				} else {
					/* Check if we matched a know client type */
					for(x=0;x<(sizeof(clients)/sizeof(clients[0]));x++) {
						char *tmp = malloc(8+strlen(clients[x]));

						sprintf(tmp, "client %s", clients[x]);
						tmp[7+strlen(clients[x])] = '\0';

						if(strcmp(message, tmp) == 0) {
							socket_write(sd, "{\"message\":\"accept client\"}");
							logprintf(LOG_INFO, "client recognized as %s", clients[x]);
							handshakes[i] = x;

							if(handshakes[i] == RECEIVER || handshakes[i] == GUI || handshakes[i] == NODE)
								receivers++;
							free(tmp);
							break;
						}
						free(tmp);
					}
				}
				/* Directly after using the incognito mode, restore the node mode */
				if(incognito_mode == 1) {
					for(x=0;x<(sizeof(clients)/sizeof(clients[0]));x++) {
						if(strcmp(clients[x], "node") == 0) {
							handshakes[i] = x;
							break;
						}
					}
					incognito_mode = 0;
				}
			}
			if(handshakes[i] == -1 && socket_clients[i] > 0) {
				socket_write(sd, "{\"message\":\"reject client\"}");
				socket_close(sd);
			}
		}
	}
	if(json) {
		json_delete(json);
	}
}

void socket_client_disconnected(int i) {
	if(handshakes[i] == RECEIVER || handshakes[i] == GUI || handshakes[i] == NODE)
		receivers--;

	handshakes[i] = 0;
}

void receive_code(void) {
	int plslen = 0, rawlen = 0, match = 0;
	struct hardware_t *hardware = NULL;
	struct hardwares_t *htmp = hardwares;
	int rawcode[255] = {0};

	match = 0;
	while(htmp) {
		if(strcmp(htmp->listener->id, hw_mode) == 0) {
			hardware = htmp->listener;
			match = 1;
			break;
		}
		htmp = htmp->next;
	}

	while(main_loop && match == 1 && hardware->receive) {
		/* Only initialize the hardware receive the data when there are receivers connected */
		if(receivers > 0 && sending == 0) {

			duration = hardware->receive();
			rawcode[rawlen] = duration;
			rawlen++;
			if(rawlen > 255) {
				rawlen = 0;
			}
			if(duration > 4440) {
				if((duration/PULSE_DIV) < 1000) {
					plslen = duration/PULSE_DIV;
				}
				receiver_parse_code(rawcode, rawlen, plslen);
				rawlen = 0;
			}
		} else {
			sleep(1);
		}
	}
}

void *clientize(void *param) {
	char *server = NULL;
	int port = 0;
	steps_t steps = WELCOME;
    char *recvBuff = NULL;
	char *message = NULL;
	char *protocol = NULL;
	JsonNode *json = NULL;
	JsonNode *jconfig = NULL;

	settings_find_string("server-ip", &server);
	settings_find_number("server-port", &port);

	if((sockfd = socket_connect(server, (short unsigned int)port)) == -1) {
		logprintf(LOG_ERR, "could not connect to pilight-daemon");
		exit(EXIT_FAILURE);
	}

	while(main_loop) {
		if(steps > WELCOME) {
			/* Clear the receive buffer again and read the welcome message */
			if(steps == REQUEST) {
				if((recvBuff = socket_read_big(sockfd))) {
					json = json_decode(recvBuff);
					json_find_string(json, "message", &message);
				} else {
					goto close;
				}
			} else {
				if((recvBuff = socket_read(sockfd))) {
					json = json_decode(recvBuff);
					json_find_string(json, "message", &message);
				} else {
					goto close;
				}
			}
		}
		usleep(100);
		switch(steps) {
			case WELCOME:
				socket_write(sockfd, "{\"message\":\"client node\"}");
				steps=IDENTIFY;
			break;
			case IDENTIFY:
				if(strcmp(message, "accept client") == 0) {
					steps=FORWARD;
				}
				if(strcmp(message, "reject client") == 0) {
					steps=REJECT;
				}
			case REQUEST:
				socket_write(sockfd, "{\"message\":\"request config\"}");
				steps=CONFIG;
				json_delete(json);
			break;
			case CONFIG:
				if((jconfig = json_find_member(json, "config"))) {
					config_parse(jconfig);
					json_delete(jconfig);
					steps=FORWARD;
				}
				json_delete(json);
			break;
			case FORWARD:
				if(!json_find_member(json, "config")) {
					if(json_find_string(json, "origin", &message) == 0 &&
					json_find_string(json, "protocol", &protocol) == 0) {
						broadcast(protocol, json);
					}
				}
				json_delete(json);
			break;
			case REJECT:
			default:
				goto close;
			break;
		}
	}
close:
	socket_close(sockfd);
	return NULL;
}

void daemonize(void) {
	int f = 0;
	char buffer[BUFFER_SIZE];

	log_file_enable();
	log_shell_disable();
	/* Get the pid of the fork */
	pid_t npid = fork();
	switch(npid) {
		case 0:
		break;
		case -1:
			logprintf(LOG_ERR, "could not daemonize program");
			exit(1);
		break;
		default:
			if((f = open(pid_file, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) != -1) {
				lseek(f, 0, SEEK_SET);
				sprintf(buffer, "%d", npid);
				int i = write(f, buffer, strlen(buffer));
				if(i != strlen(buffer)) {
					logprintf(LOG_ERR, "could not store pid in %s", pid_file);
				}
			}
			close(f);
			logprintf(LOG_INFO, "daemon started with pid: %d", npid);
			exit(0);
		break;
	}
}

/* Garbage collector of main program */
int main_gc(void) {
	unsigned int match = 0;
	struct hardware_t *hardware = NULL;
	struct hardwares_t *htmp = hardwares;

	main_loop = 0;	
	
	//sleep(1);
	
	match = 0;
	while(htmp) {
		if(strcmp(htmp->listener->id, hw_mode) == 0) {
			hardware = htmp->listener;
			match = 1;
			break;
		}
		htmp = htmp->next;
	}

	if(match == 1 && hardware->deinit) {
		hardware->deinit();
	}

	if(running == 0) {
		/* Remove the stale pid file */
		if(access(pid_file, F_OK) != -1) {
			if(remove(pid_file) != -1) {
				logprintf(LOG_DEBUG, "removed stale pid_file %s", pid_file);
			} else {
				logprintf(LOG_ERR, "could not remove stale pid file %s", pid_file);
			}
		}
	}

	if(webserver_enable == 1) {
		webserver_gc();
	}

	if(valid_config) {
		JsonNode *joutput = config2json(0);
		char *output = json_stringify(joutput, "\t");
		config_write(output);
		json_delete(joutput);
		free(output);
		joutput = NULL;
	}

	if(pth4) {
		pthread_cancel(pth4);
		pthread_join(pth4, NULL);
	}

	if(runmode == 2) {
		pthread_cancel(pth1);
		pthread_join(pth1, NULL);
	}

	pthread_cancel(pth2);
	pthread_join(pth2, NULL);
	if(webserver_enable == 1) {
		pthread_cancel(pth3);
		pthread_join(pth3, NULL);
	}
	
	config_gc();
	protocol_gc();
	hardware_gc();
	settings_gc();
	options_gc();
	socket_gc();	
	
	free(progname);	
	return 0;
}

int main(int argc , char **argv) {

	progname = malloc(16);
	strcpy(progname, "pilight-daemon");

	if(geteuid() != 0) {
		printf("%s requires root priveliges in order to run\n", progname);
		free(progname);
		exit(EXIT_FAILURE);
	}

	/* Run main garbage collector when quiting the daemon */
	gc_attach(main_gc);

	/* Catch all exit signals for gc */
	gc_catch();

	log_file_enable();
	log_shell_disable();

	settingsfile = malloc(strlen(SETTINGS_FILE)+1);
	strcpy(settingsfile, SETTINGS_FILE);

	struct socket_callback_t socket_callback;
	struct options_t *options = NULL;
	struct hardware_t *hardware = NULL;

	char buffer[BUFFER_SIZE];
	int f;
	int itmp;
	unsigned short match = 0;
	char *stmp = NULL;
	char *args = NULL;
	int port = 0;

	int show_help = 0;
	int show_version = 0;
	int show_default = 0;

	memset(buffer, '\0', BUFFER_SIZE);

	options_add(&options, 'H', "help", no_value, 0, NULL);
	options_add(&options, 'V', "version", no_value, 0, NULL);
	options_add(&options, 'D', "nodaemon", no_value, 0, NULL);
	options_add(&options, 'S', "settings", has_value, 0, NULL);

	while(1) {
		int c;
		c = options_parse(&options, argc, argv, 1, &args);
		if(c == -1)
			break;
		if(c == -2) {
			show_help = 1;
			break;
		}
		switch(c) {
			case 'H':
				show_help = 1;
			break;
			case 'V':
				show_version = 1;
			break;
			case 'S':
				if(access(args, F_OK) != -1) {
					settingsfile = realloc(settingsfile, strlen(args)+1);
					strcpy(settingsfile, args);
					settings_set_file(args);
				} else {
					fprintf(stderr, "%s: the settings file %s does not exists\n", progname, args);
					exit(EXIT_FAILURE);
				}
			break;
			case 'D':
				nodaemon=1;
			break;
			default:
				show_default = 1;
			break;
		}
	}
	options_delete(options);

	if(show_help) {
		printf("Usage: %s [options]\n", progname);
		printf("\t -H --help\t\tdisplay usage summary\n");
		printf("\t -V --version\t\tdisplay version\n");
		printf("\t -S --settings\t\tsettings file\n");
		printf("\t -D --nodaemon\t\tdo not daemonize and\n");
		printf("\t\t\t\tshow debug information\n");
		goto clear;
	}
	if(show_version) {
		printf("%s version %.1f, commit %s\n", progname, VERSION, HASH);
		goto clear;
	}
	if(show_default) {
		printf("Usage: %s [options]\n", progname);
		goto clear;
	}

	if(access(settingsfile, F_OK) != -1) {
		if(settings_read() != 0) {
			valid_config = 0;
			goto clear;
		}
	}

	settings_find_number("gpio-sender", &gpio_out);
	settings_find_number("gpio-receiver", &gpio_in);

	if(settings_find_string("hw-mode", &stmp) == 0) {
		hw_mode = stmp;
	}

	settings_find_number("webserver-enable", &webserver_enable);
	settings_find_number("webserver-port", &webserver_port);
	if(settings_find_string("webserver-root", &webserver_root) != 0) {
		webserver_root = realloc(webserver_root, strlen(WEBSERVER_ROOT)+1);
		strcpy(webserver_root, WEBSERVER_ROOT);
	}

	if(settings_find_string("pid-file", &pid_file) != 0) {
		pid_file = realloc(pid_file, strlen(PID_FILE)+1);
		strcpy(pid_file, PID_FILE);
	}

	if((f = open(pid_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) != -1) {
		if(read(f, buffer, BUFFER_SIZE) != -1) {
			//If the file is empty, create a new process
			strcat(buffer, "\0");
			if(!atoi(buffer)) {
				running = 0;
			} else {
				//Check if the process is running
				kill(atoi(buffer), 0);
				//If not, create a new process
				if(errno == ESRCH) {
					running = 0;
				}
			}
		}
	} else {
		logprintf(LOG_ERR, "could not open / create pid_file %s", pid_file);
		goto clear;
	}
	close(f);

	if(settings_find_number("log-level", &itmp) == 0) {
		itmp += 2;
		log_level_set(itmp);
	}

	if(settings_find_string("log-file", &stmp) == 0) {
		log_file_set(stmp);
	}

	if(settings_find_string("mode", &stmp) == 0) {
		if(strcmp(stmp, "client") == 0) {
			runmode = 2;
		} else if(strcmp(stmp, "server") == 0) {
			runmode = 1;
		}
	}

	logprintf(LOG_INFO, "version %.1f, commit %s", VERSION, HASH);

	if(nodaemon == 1 || running == 1) {
		log_file_disable();
		log_shell_enable();
		log_level_set(LOG_DEBUG);
	}

	settings_find_string("process-file", &process_file);

	if(settings_find_number("send-repeats", &send_repeat) != 0) {
		send_repeat = SEND_REPEATS;
	}

	settings_find_number("receive-repeats", &receive_repeat);

	if(running == 1) {
		nodaemon=1;
		logprintf(LOG_NOTICE, "already active (pid %d)", atoi(buffer));
		log_level_set(LOG_NOTICE);
		log_shell_disable();
		goto clear;
	}

	/* Initialize peripheral modules */
	hardware_init();

	struct hardwares_t *htmp = hardwares;
	match = 0;
	while(htmp) {
		if(strcmp(htmp->listener->id, hw_mode) == 0) {
			hardware = htmp->listener;
			match = 1;
			break;
		}
		htmp = htmp->next;
	}
	if(match == 1) {
		if(hardware->init) {
			hardware->init();
		}
	} else {
		logprintf(LOG_NOTICE, "the \"%s\" hw-mode is not supported", hw_mode);
		goto clear;
	}

	if(settings_find_string("config-file", &stmp) == 0) {
		if(config_set_file(stmp) == 0) {
			if(config_read() != 0) {
				valid_config = 0;
				goto clear;
			} else {
				receivers++;
			}

			if(log_level_get() >= LOG_DEBUG) {
				config_print();
			}
		}
	}

	if(settings_find_number("port", &port) != 0) {
		port = PORT;
	}

	if(nodaemon == 0) {
		daemonize();
	}
	socket_start((short unsigned int)port);	

    //initialise all socket_clients and handshakes to 0 so not checked
	memset(socket_clients, 0, sizeof(socket_clients));
	memset(handshakes, -1, sizeof(handshakes));

	/* Export certain daemon function to global usage */
	pilight.broadcast = &broadcast;
	pilight.send = &send_code;
	pilight.receive = &receiver_parse_code;

	/* Run certain daemon functions from the socket library */
    socket_callback.client_disconnected_callback = &socket_client_disconnected;
    socket_callback.client_connected_callback = NULL;
    socket_callback.client_data_callback = &socket_parse_data;

	/* The daemon running in client mode, create a seperate thread that
	   communicates with the server */
	if(runmode == 2) {
		pthread_create(&pth1, NULL, &clientize, (void *)NULL);
	}
	/* Create a seperate thread for the server */
	pthread_create(&pth2, NULL, &socket_wait, (void *)&socket_callback);
	/* Create a seperate thread for the webserver */
	if(webserver_enable == 1) {
		pthread_create(&pth3, NULL, &webserver_start, (void *)NULL);
	}

	/* And our main receiving loop */
	if(match == 1) {
		receive_code();
	} else {
		while(main_loop) {
			sleep(1);
		}
	}

clear:
	if(nodaemon == 0) {
		log_level_set(LOG_NOTICE);
		log_shell_disable();
	}
	main_gc();
	log_gc();
	gc_clear();
	return EXIT_FAILURE;
}