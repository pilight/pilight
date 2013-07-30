/*
	Copyright (C) 2013 CurlyMo

	This file is part of the QPido.

    QPido is free software: you can redistribute it and/or modify it 
	under the terms of the GNU General Public License as published by 
	the Free Software Foundation, either version 3 of the License, or 
	(at your option) any later version.

    QPido is distributed in the hope that it will be useful, but 
	WITHOUT ANY WARRANTY; without even the implied warranty of 
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
	General Public License for more details.

    You should have received a copy of the GNU General Public License 
	along with QPido. If not, see <http://www.gnu.org/licenses/>
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
#include <pthread.h>
#include <ctype.h>

#include "settings.h"
#include "config.h"
#include "gc.h"
#include "log.h"
#include "options.h"
#include "socket.h"
#include "json.h"

#include "lirc.h"
#include "lirc/lircd.h"
#include "lirc/hardware.h"
#include "lirc/transmit.h"
#include "lirc/hw-types.h"
#include "lirc/hw_default.h"

#include "hardware.h"

typedef enum {
	RECEIVER,
	SENDER,
	CONTROLLER,
	NODE,
	GUI
} client_type_t;

typedef struct clients_t clients_t;

char clients[5][11] = {
	"receiver\0",
	"sender\0",
	"controller\0",
	"node\0",
	"gui\0"
};

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
int receive_repeat;
/* Are we currently sending code */
int sending = 0;
/* If we have accepted a client, handshakes will store the type of client */
short handshakes[MAX_CLIENTS];
/* Server port */
int port = 0;

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

int broadcast(char *protoname, JsonNode *json) {
	FILE *f;
	int i = 0, broadcasted = 0;
	char *message = json_stringify(json, NULL);

	char *escaped = malloc(2 * strlen(message) + 1);
	JsonNode *jret = json_mkobject();

	/* Update the config file */
	jret = config_update(protoname, json);

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

	broadcasted = 0;

	if(json_validate(message) == true && receivers > 0) {

		/* Write the message to all receivers */
		for(i=0;i<MAX_CLIENTS;i++) {
			if(handshakes[i] == RECEIVER) {
				socket_write(socket_clients[i], message);
				broadcasted = 1;
			}
		}

		escape_characters(escaped, message);

		if(process_file != NULL && strlen(process_file) > 0) {
			/* Call the external file */
			if(strlen(process_file) > 0) {
				char cmd[255];
				strcpy(cmd, process_file);
				strcat(cmd," ");
				strcat(cmd, escaped);
				f=popen(cmd, "r");
				pclose(f);
				broadcasted = 1;
			}
			free(escaped);
			if(broadcasted) {
				logprintf(LOG_DEBUG, "broadcasted: %s", message);
			}
		}
		return 1;
	}
	json_delete(jret);
	free(message);
	return 0;
}

/* Send a specific code */
void send_code(JsonNode *json) {
	int i=0, match = 0, x = 0, logged = 1;
	char *name;

	/* Hold the final protocol struct */
	protocol_t *protocol = malloc(sizeof(protocol_t));
	/* The code that is send to the hardware wrapper */
	struct ir_ncode code;

	JsonNode *jcode = json_mkobject();
	JsonNode *message = json_mkobject();

	if((jcode = json_find_member(json, "code")) == NULL) {
		logprintf(LOG_ERR, "sender did not send any codes");
	} else if(json_find_string(jcode, "protocol", &name) != 0) {
		logprintf(LOG_ERR, "sender did not provide a protocol name");
	} else {
		for(i=0; i<protocols.nr; ++i) {
			protocol = protocols.listeners[i];
			/* Check if the protocol exists */
			if(providesDevice(&protocol, name) == 0 && match == 0) {
				match = 1;
				break;
			}
		}
		logged = 0;
		/* If we matched a protocol and are not already sending, continue */
		if(match == 1 && sending == 0 && protocol->createCode != NULL && send_repeat > 0) {
			/* Let the protocol create his code */
			if(protocol->createCode(jcode) == 0) {

				if(protocol->message != NULL && json_validate(json_stringify(message, NULL)) == true && json_validate(json_stringify(message, NULL)) == true) {
					json_append_member(message, "origin", json_mkstring("sender"));
					json_append_member(message, "code", protocol->message);
				}

				sending = 1;
				/* Create a single code with all repeats included */
				int longCode[(protocol->rawLength*send_repeat)+1];
				for(i=0;i<send_repeat;i++) {
					for(x=0;x<protocol->rawLength;x++) {
						longCode[x+(protocol->rawLength*i)]=protocol->raw[x];
					}
				}
				/* Send this single big code at once */
				code.length = (x+(protocol->rawLength*(i-1)))+1;
				code.signals = longCode;

				/* Send the code, if we succeeded, inform the receiver */
				if(send_ir_ncode(&code) == 1) {
					logprintf(LOG_DEBUG, "successfully send %s code", protocol->id);
					if(logged == 0) {
						logged = 1;
						/* Write the message to all receivers */
						broadcast(protocol->id, message);
					}
				} else {
					logprintf(LOG_ERR, "failed to send code");
				}

				sending = 0;
			}
		}
	}
}

void client_sender_parse_code(int i, JsonNode *json) {
	int sd = socket_clients[i];

	/* Don't let the sender wait until we have send the code */
	socket_close(sd);
	handshakes[i] = -1;

	send_code(json);
}

void control_device(struct conf_devices_t *dev, char *state) {
	struct conf_settings_t *sett = NULL;
	struct conf_values_t *val = NULL;
	struct options_t *opt = NULL;
	char *fval = NULL;
	char *cstate = NULL;
	char *nstate = NULL;
	JsonNode *code = json_mkobject();
	JsonNode *json = json_mkobject();

	if(strlen(state) > 0) {
		nstate = strdup(state);
	}

	/* Check all protocol options */
	if(dev->protopt->options != NULL) {
		opt = dev->protopt->options;
		while(opt) {
			sett = dev->settings;
			while(sett) {
				/* Retrieve the device id's */
				if(opt->conftype == config_id && strcmp(sett->name, opt->name) == 0) {
					json_append_member(code, sett->name, json_mkstring(sett->values->value));
				}
				/* Retrieve the current state */
				if(strcmp(sett->name, "state") == 0 && strlen(state) == 0) {
					cstate = strdup(sett->values->value);
				}
				/* Retrieve the possible device values */
				if(strcmp(sett->name, "values") == 0) {
					val = sett->values;
				}
				sett = sett->next;
			}
			opt = opt->next;
		}
	}

	if(strlen(state) == 0) {
		/* Get the next state value */
		while(val) {
			if(fval == NULL) {
				fval = strdup(val->value);
			}
			if(strcmp(val->value, cstate) == 0) {
				if(val->next) {
					nstate = val->next->value;
				} else {
					nstate = strdup(fval);
				}
				break;
			}
			val = val->next;
		}
	}

	/* Send the new device state */
	if(dev->protopt->options != NULL) {
		opt = dev->protopt->options;
		while(opt) {
			if(opt->conftype == config_state && opt->argtype == no_value && strcmp(opt->name, nstate) == 0) {
				json_append_member(code, opt->name, json_mkstring("1"));
				break;
			} else if(opt->conftype == config_state && opt->argtype == has_value) {
				json_append_member(code, opt->name, json_mkstring(nstate));
				break;
			}
			opt = opt->next;
		}
	}
	/* Construct the right json object */
	json_append_member(code, "protocol", json_mkstring(dev->protoname));
	json_append_member(json, "message", json_mkstring("sender"));
	json_append_member(json, "code", code);

	send_code(json);
	json_delete(code);
	json_delete(json);
}

void client_controller_parse_code(int i, JsonNode *json) {
	int sd = socket_clients[i];
	char *message = NULL;
	char *location = NULL;
	char *device = NULL;
	char *state = NULL;
	char *tmp = NULL;
	struct conf_locations_t *slocation;
	struct conf_devices_t *sdevice;
	JsonNode *code = NULL;

	if(json_find_string(json, "message", &message) == 0) {
		/* Send the config file to the controller */
		if(strcmp(message, "request config") == 0) {
			json = json_mkobject();
			json_append_member(json, "config", config2json());
			socket_write_big(sd, json_stringify(json, NULL));
		/* Control a specific device */
		} else if(strcmp(message, "send") == 0) {
			/* Check if got a code */
			if((code = json_find_member(json, "code")) == NULL) {
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
						if(json_find_string(code, "state", &tmp) == 0) {
							state = strdup(tmp);
						} else {
							state = strdup("\0");
						}
						/* Send the device code */
						control_device(sdevice, state);
					} else {
						logprintf(LOG_ERR, "the device \"%s\" does not exist", device);
					}
				} else {
					logprintf(LOG_ERR, "the location \"%s\" does not exist", location);
				}
				if(handshakes[i] == CONTROLLER) {
					socket_close(sd);
					handshakes[i] = -1;
				}
			}
		}
	}
}

/* Parse the incoming buffer from the client */
void socket_parse_data(int i, char buffer[BUFFER_SIZE]) {
	int sd = socket_clients[i];
	struct sockaddr_in address;
	int addrlen = sizeof(address);
	char tmp[25];
	char *message;
	JsonNode *json = json_mkobject();
	short x = 0;

	getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);

	if(strcmp(buffer, "HEART\n") != 0) {
		logprintf(LOG_DEBUG, "socket recv: %s", buffer);
	}

	json = json_decode(buffer);

	if(strcmp(buffer, "HEART\n") == 0) {
		socket_write(sd, "BEAT");
	} else {
		if(json_find_string(json, "message", &message) == 0) {
			if(handshakes[i] == SENDER) {
				client_sender_parse_code(i, json);
			} else if(handshakes[i] == CONTROLLER || handshakes[i] == GUI) {
				client_controller_parse_code(i, json);
			} else {

				/* Check if we matched a know client type */
				for(x=0;x<(sizeof(clients)/sizeof(clients[0]));x++) {
					memset(tmp, '\0', 25);

					strcat(tmp, "client ");
					strcat(tmp, clients[x]);

					if(strcmp(message, tmp) == 0) {
						socket_write(sd, "{\"message\":\"accept client\"}");
						logprintf(LOG_INFO, "client recognized as %s", clients[x]);
						handshakes[i] = x;

						if(handshakes[i] == RECEIVER || handshakes[i] == GUI)
							receivers++;
						break;
					}
				}
			}
		}
		if(handshakes[i] == -1 && socket_clients[i] > 0) {
			socket_write(sd, "{\"message\":\"reject client\"}");
		}
	}
	json_delete(json);
}

void socket_client_disconnected(int i) {
	if(handshakes[i] == RECEIVER || handshakes[i] == GUI)
		receivers--;

	handshakes[i] = 0;
}

void receive_code(void) {
	lirc_t data;
	unsigned int x, y = 0, i;
	protocol_t *protocol = malloc(sizeof(protocol_t));
	JsonNode *message = NULL;

	while(1) {
		/* Only initialize the hardware receive the data when there are receivers connected */
		if(receivers > 0) {
			message = json_mkobject();
			data = hw.readdata(0);
			duration = (data & PULSE_MASK);
			/* A space is normally for 295 long, so filter spaces less then 200 */
			if(sending == 0 && duration > 200) {
				for(i=0; i<protocols.nr; ++i) {
					protocol = protocols.listeners[i];
					/* Lots of checks if the protocol can actually receive anything */
					if((((protocol->parseRaw != NULL || protocol->parseCode != NULL) && protocol->rawLength > 0)
					    || (protocol->parseBinary != NULL && protocol->binaryLength > 0))
						&& protocol->header > 0 && protocol->footer > 0
						&& protocol->pulse > 0 && protocol->multiplier > 0) {
						/* If we are recording, keep recording until the footer has been matched */
						if(protocol->recording == 1) {
							if(protocol->bit < 255) {
								protocol->raw[protocol->bit++] = duration;
							} else {
								protocol->bit = 0;
								protocol->recording = 0;
							}
						}

						/* Try to catch the header of the code */
						if(duration > (PULSE_LENGTH-(PULSE_LENGTH*protocol->multiplier))
						   && duration < (PULSE_LENGTH+(PULSE_LENGTH*protocol->multiplier))
						   && protocol->bit == 0) {
							protocol->raw[protocol->bit++] = duration;
						} else if(duration > ((protocol->header*PULSE_LENGTH)-((protocol->header*PULSE_LENGTH)*protocol->multiplier))
						   && duration < ((protocol->header*PULSE_LENGTH)+((protocol->header*PULSE_LENGTH)*protocol->multiplier))
						   && protocol->bit == 1) {
							protocol->raw[protocol->bit++] = duration;
							protocol->recording = 1;
						}

						/* Try to catch the footer of the code */
						if(duration > ((protocol->footer*PULSE_LENGTH)-((protocol->footer*PULSE_LENGTH)*protocol->multiplier))
						   && duration < ((protocol->footer*PULSE_LENGTH)+((protocol->footer*PULSE_LENGTH)*protocol->multiplier))) {
							//logprintf(LOG_DEBUG, "catched %s header and footer", protocol->id);
							/* Check if the code matches the raw length */
							if((protocol->bit == protocol->rawLength)) {
								if(protocol->parseRaw != NULL) {
									logprintf(LOG_DEBUG, "called %s parseRaw()", protocol->id);

									protocol->parseRaw();
									if(protocol->message != NULL && json_validate(json_stringify(message, NULL)) == true) {
										json_append_member(message, "origin", json_mkstring("receiver"));
										json_append_member(message, "code", protocol->message);
										broadcast(protocol->id, message);
										continue;
									} else {
										continue;
									}
								}

								if((protocol->parseCode != NULL || protocol->parseBinary != NULL) && protocol->pulse > 0) {
									/* Convert the raw codes to one's and zero's */
									for(x=0;x<protocol->bit;x++) {

										/* Check if the current code matches the previous one */
										if(protocol->pCode[x] != protocol->code[x])
											y=0;
										protocol->pCode[x]=protocol->code[x];
										if(protocol->raw[x] > ((protocol->pulse*PULSE_LENGTH)-PULSE_LENGTH)) {
											protocol->code[x]=1;
										} else {
											protocol->code[x]=0;
										}
									}

									y++;

									/* Continue if we have recognized enough repeated codes */
									if(y >= receive_repeat) {
										if(protocol->parseCode != NULL) {
											logprintf(LOG_DEBUG, "catched minimum # of repeats %s of %s", y, protocol->id);
											logprintf(LOG_DEBUG, "called %s parseCode()", protocol->id);

											protocol->parseCode();
											if(protocol->message != NULL && json_validate(json_stringify(message, NULL)) == true) {
												json_append_member(message, "origin", json_mkstring("receiver"));
												json_append_member(message, "code", protocol->message);
												broadcast(protocol->id, message);
												continue;
											} else {
												continue;
											}
										}

										if(protocol->parseBinary != NULL) {
											/* Convert the one's and zero's into binary */
											for(x=2; x<protocol->bit; x+=4) {
												if(protocol->code[x+1] == 1) {
													protocol->binary[x/4]=1;
												} else {
													protocol->binary[x/4]=0;
												}
											}

											/* Check if the binary matches the binary length */
											if((protocol->binaryLength > 0) && ((x/4) == protocol->binaryLength)) {
												logprintf(LOG_DEBUG, "called %s parseBinary()", protocol->id);

												protocol->parseBinary();
												if(protocol->message != NULL && json_validate(json_stringify(message, NULL)) == true) {
													json_append_member(message, "origin", json_mkstring("receiver"));
													json_append_member(message, "code", protocol->message);
													broadcast(protocol->id, message);
													continue;
												} else {
													continue;
												}
											}
										}
									}
								}
							}
							protocol->recording = 0;
							protocol->bit = 0;
						}
					}
				}
			}
			//json_delete(message);
		} else {
			sleep(1);
		}
	}
}

void deamonize(void) {
	int f;
	char buffer[BUFFER_SIZE];

	enable_file_log();
	disable_shell_log();
	//Get the pid of the fork
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
				if(write(f, buffer, strlen(buffer)) != -1) {
					logprintf(LOG_ERR, "could not store pid in %s", pid_file);
				}
			}
			close(f);
			logprintf(LOG_INFO, "started daemon with pid %d", npid);
			exit(0);
		break;
	}
}

/* Garbage collector of main program */
int main_gc(void) {

	module_deinit();
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
	return 0;
}

int main(int argc , char **argv) {
	/* Run main garbage collector when quiting the daemon */
	gc_attach(main_gc);

	/* Catch all exit signals for gc */
	gc_catch();

	disable_file_log();
	enable_shell_log();

	progname = malloc((12*sizeof(char))+1);
	progname = strdup("qpido-daemon");

	settingsfile = strdup(SETTINGS_FILE);
	
	struct socket_callback_t socket_callback;
	pthread_t pth;
	char buffer[BUFFER_SIZE];
	int f;
	int itmp;
	char *stmp = NULL;

	addOption(&options, 'H', "help", no_value, 0, NULL);
	addOption(&options, 'V', "version", no_value, 0, NULL);
	addOption(&options, 'D', "nodaemon", no_value, 0, NULL);
	addOption(&options, 'S', "settings", has_value, 0, NULL);

	hw_choose_driver(NULL);
	while (1) {
		int c;
		c = getOptions(&options, argc, argv, 1);
		if (c == -1)
			break;
		switch(c) {
			case 'H':
				printf("Usage: %s [options]\n", progname);
				printf("\t -H --help\t\tdisplay usage summary\n");
				printf("\t -V --version\t\tdisplay version\n");
				printf("\t -S --settings\t\tsettings file\n");
				printf("\t -D --nodaemon\t\tdo not daemonize and\n");
				printf("\t\t\t\tshow debug information\n");
				return (EXIT_SUCCESS);
			break;
			case 'V':
				printf("%s %s\n", progname, "1.0");
				return (EXIT_SUCCESS);
			break;
			case 'S': 
				if(access(optarg, F_OK) != -1) {
					settingsfile = strdup(optarg);
					settings_set_file(optarg);
				} else {
					fprintf(stderr, "%s: the settings file %s does not exists\n", progname, optarg);
					return EXIT_FAILURE;
				}
			break;
			case 'D':
				nodaemon=1;
			break;
			default:
				printf("Usage: %s [options]\n", progname);
				return (EXIT_FAILURE);
			break;
		}
	}
	
	if(access(settingsfile, F_OK) != -1) {
		if(settings_read() != 0) {
			return EXIT_FAILURE;
		}
	}
	
	if(settings_find_string("pid-file", &pid_file) != 0) {
		pid_file = strdup(PID_FILE);
	}

	if((f = open(pid_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) != -1) {
		if(read(f, buffer, BUFFER_SIZE) != -1) {
			//If the file is empty, create a new process
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
		return EXIT_FAILURE;
	}
	close(f);	

	if(settings_find_string("socket", &hw.device) != 0) {
		hw.device = strdup(DEFAULT_LIRC_SOCKET);
	}

	if(settings_find_number("log-level", &itmp) == 0) {
		itmp += 2;
		set_loglevel(itmp);
	}
	
	if(settings_find_string("log-file", &stmp) == 0) {
		set_logfile(stmp);
	}
	
	if(nodaemon == 1 || running == 1) {
		set_loglevel(LOG_DEBUG);
	}

	settings_find_string("process-file", &process_file);

	if(strcmp(hw.device, "/var/lirc/lircd") == 0) {
		logprintf(LOG_ERR, "refusing to connect to lircd socket");
		return EXIT_FAILURE;
	}

	if(settings_find_number("send-repeats", &send_repeat) != 0) {
		send_repeat = SEND_REPEATS;
	}

	if(settings_find_number("receive-repeats", &receive_repeat) != 0) {
		receive_repeat = RECEIVE_REPEATS;
	}	
	
	if(running == 1) {
		nodaemon=1;
		logprintf(LOG_NOTICE, "already active (pid %d)", atoi(buffer));
		return EXIT_FAILURE;
	}

	module_init();

	/* Initialize peripheral modules */
	hw_init();

	if(settings_find_string("config-file", &configfile) == 0) {
		if(config_set_file(configfile) == 0) {
			if(config_read() != 0) {
				return EXIT_FAILURE;
			} else {
				receivers++;
			}

			if(get_loglevel() >= LOG_DEBUG) {
				config_print();
			}
		}
	}
	
	if(settings_find_number("port", &port) != 0) {
		port = PORT;
	}

	start_server((short unsigned int)port);
	if(nodaemon == 0) {
		deamonize();
	}

    //initialise all socket_clients and handshakes to 0 so not checked
	memset(socket_clients, 0, sizeof(socket_clients));
	memset(handshakes, -1, sizeof(handshakes));

    socket_callback.client_disconnected_callback = &socket_client_disconnected;
    socket_callback.client_connected_callback = NULL;
    socket_callback.client_data_callback = &socket_parse_data;

	/* Make sure the server part is non-blocking by creating a new thread */
	pthread_create(&pth, NULL, &wait_for_data, (void *)&socket_callback);
	receive_code();
	pthread_join(pth, NULL);

	return EXIT_FAILURE;
}
