/*
	Copyright (C) 2013 CurlyMo

	This file is part of the Raspberry Pi 433.92Mhz transceiver.

    Raspberry Pi 433.92Mhz transceiver is free software: you can redistribute
	it and/or modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation, either version 3 of the License,
	or (at your option) any later version.

    Raspberry Pi 433.92Mhz transceiver is distributed in the hope that it will
	be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Raspberry Pi 433.92Mhz transceiver. If not, see
	<http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <ctype.h>

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
	NODE
} client_type_t;

typedef struct clients_t clients_t;

char clients[4][11] = {
	"receiver\0",
	"sender\0",
	"controller\0",
	"node\0"
};

/* The pidfile and pid of this daemon */
char *pidfile;
pid_t pid;
/* The to call when a new code is send or received */
char processfile[1024];
/* The duration of the received pulse */
int duration = 0;
/* The number of receivers connected */
int receivers = 0;
/* Daemonize or not */
int nodaemon = 0;
/* Are we already running */
int running = 1;
/* How many times does to keep need to be resend */
int repeat = SEND_REPEATS;
/* Are we currently sending code */
int sending = 0;
/* If we have accepted a client, handshakes will store the type of client */
short handshakes[MAX_CLIENTS];

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
	int i;
	char *message = json_stringify(json, NULL);
	char *escaped = malloc(2 * strlen(message) + 1);

	/* Update the config file */
	config_update(protoname, json);
	if(json_validate(message) == true && receivers > 0) {

		/* Write the message to all receivers */
		for(i=0;i<MAX_CLIENTS;i++) {
			if(handshakes[i] == RECEIVER) {
				socket_write(socket_clients[i], message);
			}
		}

		escape_characters(escaped, message);

		/* Call the external file */
		if(strlen(processfile) > 0) {
			char cmd[255];
			strcpy(cmd, processfile);
			strcat(cmd," ");
			strcat(cmd, escaped);
			f=popen(cmd, "r");
			pclose(f);
		}
		free(escaped);
		logprintf(LOG_DEBUG, "broadcasted: %s", message);
		return 1;
	}
	free(escaped);
	free(message);
	return 0;
}

/* Send a specific code */
void send_code(JsonNode *json) {
	int i=0, match = 0, x = 0, logged = 1;
	char *name;
	char *temp;

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
		if(match == 1 && sending == 0 && protocol->createCode != NULL && repeat > 0) {
			/* Let the protocol create his code */
			if(protocol->createCode(jcode) == 0) {

				if(protocol->message != NULL && json_validate(json_stringify(message, NULL)) == true && json_validate(json_stringify(message, NULL)) == true) {
					json_append_member(message, "origin", json_mkstring("sender"));
					json_append_member(message, "code", protocol->message);
				}

				/* Check if we need to repeat the code other than the default repeat */
				if(json_find_string(jcode, "repeat", &temp) == 0) {
					repeat=atoi(temp);
					logprintf(LOG_DEBUG, "set send repeat to %d time(s)", repeat);
				}

				sending = 1;
				/* Create a single code with all repeats included */
				int longCode[(protocol->rawLength*repeat)+1];
				for(i=0;i<repeat;i++) {
					for(x=0;x<protocol->rawLength;x++) {
						longCode[x+(protocol->rawLength*i)]=protocol->raw[x];
					}
				}
				/* Send this single big code at once */
				code.length = (x+(protocol->rawLength*(i-1)))+1;
				code.signals = longCode;
				/* If we have no receiver, we need to initialize the hardware first */
				if(receivers == 0)
					module_init();

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
				/* Release the hardware to spare resources */
				if(receivers == 0)
					module_deinit();
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

void control_device(struct conf_devices_t *dev) {
	struct conf_settings_t *sett = NULL;
	struct conf_values_t *val = NULL;
	struct options_t *opt = NULL;
	char *fval = NULL;
	char *cstate = NULL;
	char *nstate = NULL;
	JsonNode *code = json_mkobject();
	JsonNode *json = json_mkobject();

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
				if(strcmp(sett->name, "state") == 0) {
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
						/* Send the device code */
						control_device(sdevice);
					} else {
						logprintf(LOG_ERR, "the device \"%s\" does not exist", device);
					}
				} else {
					logprintf(LOG_ERR, "the location \"%s\" does not exist", location);
				}
				socket_close(sd);
				handshakes[i] = -1;
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

	logprintf(LOG_DEBUG, "socket recv: %s", buffer);

	json = json_decode(buffer);
	if(json_find_string(json, "message", &message) == 0) {
		if(handshakes[i] == SENDER) {
			client_sender_parse_code(i, json);
		} else if(handshakes[i] == CONTROLLER) {
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

					if(handshakes[i] == RECEIVER)
						receivers++;
					break;
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
	if(handshakes[i] == RECEIVER)
		receivers--;
	/* Release the hardware when no receiver are connected */
	if(receivers == 0 && sending == 0)
		module_deinit();

	handshakes[i] = 0;
}

void receive_code(void) {
	lirc_t data;
	unsigned int x, y = 0, i;
	protocol_t *protocol = malloc(sizeof(protocol_t));

	while(1) {
		/* Only initialize the hardware receive the data when there are receivers connected */
		if(receivers > 0) {
			module_init();
			JsonNode *message = json_mkobject();
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
						&& protocol->pulse > 0 && protocol->multiplier != NULL) {
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
						if(duration > (PULSE_LENGTH-(PULSE_LENGTH*protocol->multiplier[0]))
						   && duration < (PULSE_LENGTH+(PULSE_LENGTH*protocol->multiplier[0]))
						   && protocol->bit == 0) {
							protocol->raw[protocol->bit++] = duration;
						} else if(duration > ((protocol->header*PULSE_LENGTH)-((protocol->header*PULSE_LENGTH)*protocol->multiplier[0]))
						   && duration < ((protocol->header*PULSE_LENGTH)+((protocol->header*PULSE_LENGTH)*protocol->multiplier[0]))
						   && protocol->bit == 1) {
							protocol->raw[protocol->bit++] = duration;
							protocol->recording = 1;
						}

						/* Try to catch the footer of the code */
						if(duration > ((protocol->footer*PULSE_LENGTH)-((protocol->footer*PULSE_LENGTH)*protocol->multiplier[1]))
						   && duration < ((protocol->footer*PULSE_LENGTH)+((protocol->footer*PULSE_LENGTH)*protocol->multiplier[1]))) {
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
										if(protocol->pCode[x]!=protocol->code[x])
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
									if(protocol->repeats > 0 && y >= protocol->repeats) {
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
			json_delete(message);
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
			if((f = open(pidfile, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) != -1) {
				lseek(f, 0, SEEK_SET);
				sprintf(buffer, "%d", npid);
				if(write(f, buffer, strlen(buffer)) != -1) {
					logprintf(LOG_ERR, "could not store pid in %s", pidfile);
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

	module_init();
	module_deinit();
	if(running == 0) {
		/* Remove the stale pid file */
		if(access(pidfile, F_OK) != -1) {
			if(remove(pidfile) != -1) {
				logprintf(LOG_DEBUG, "removed stale pidfile %s", pidfile);
			} else {
				logprintf(LOG_ERR, "could not remove stale pid file %s", pidfile);
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

	progname = malloc((10*sizeof(char))+1);
	progname = strdup("433-daemon");

	pidfile = strdup(PID_FILE);
	configfile = strdup(CONFIG_FILE);

	struct socket_callback_t socket_callback;
	pthread_t pth;
	char *lsocket = strdup("/dev/lirc0");
	char buffer[BUFFER_SIZE];
	int have_device = 0;
	int have_config = 0;
	int f;

	addOption(&options, 'h', "help", no_value, 0, NULL);
	addOption(&options, 'v', "version", no_value, 0, NULL);
	addOption(&options, 's', "socket", has_value, 0, "^/dev/([A-Za-z]+)([0-9]+)");
	addOption(&options, 'p', "process", has_value, 0, NULL);
	addOption(&options, 'd', "nodaemon", no_value, 0, NULL);
	addOption(&options, 'L', "loglevel", has_value, 0, "[0-5]");
	addOption(&options, 'l', "logfile", has_value, 0, NULL);
	addOption(&options, 'c', "config", has_value, 0, NULL);

	hw_choose_driver(NULL);
	while (1) {
		int c;
		c = getOptions(&options, argc, argv, 1);
		if (c == -1)
			break;
		switch(c) {
			case 'h':
				printf("Usage: %s [options]\n", progname);
				printf("\t -h --help\t\tdisplay usage summary\n");
				printf("\t -v --version\t\tdisplay version\n");
				printf("\t -s --socket=socket\tread from given socket\n");
				printf("\t -p --process=file\tfile to call after received / send code\n");
				printf("\t -l --logfile=file\tlogfile\n");
				printf("\t -L --loglevel=[1-5]\t1 only errors\n");
				printf("\t\t\t\t2 warnings + previous\n");
				printf("\t\t\t\t3 notices  + previous \n");
				printf("\t\t\t\t4 info     + previous (default)\n");
				printf("\t\t\t\t5 debug    + previous\n");
				printf("\t\t\t\t  only works when not daemonized\n");
				printf("\t -c --config=file\tconfig file\n");
				printf("\t -d --nodaemon\t\tdo not daemonize\n");
				return (EXIT_SUCCESS);
			break;
			case 'v':
				printf("%s %s\n", progname, "1.0");
				return (EXIT_SUCCESS);
			break;
			case 's':
				lsocket = optarg;
				have_device = 1;
			break;
			case 'c':
				if(access(optarg, F_OK) != -1) {
					have_config = 1;
					config_set_file(optarg);
				} else {
					fprintf(stderr, "%s: the config file %s does not exists\n", progname, optarg);
					return EXIT_FAILURE;
				}
			break;
			case 'd':
				nodaemon=1;
			break;
			case 'p':
				if(access(optarg, F_OK) != -1) {
					strcpy(processfile, optarg);
					receivers++;
				} else {
					fprintf(stderr, "%s: the process file %s does not exists\n", progname, optarg);
					return EXIT_FAILURE;
				}
			break;
			case 'l':
				set_logfile(optarg);
			break;
			case 'L':
				if(atoi(optarg) <=5 && atoi(optarg) >= 1) {
					set_loglevel(atoi(optarg)+2);
				} else {
					fprintf(stderr, "%s: invalid loglevel specified\n", progname);
					return EXIT_FAILURE;
				}
			break;
			default:
				printf("Usage: %s [options]\n", progname);
				return (EXIT_FAILURE);
			break;
		}
	}

	if(strcmp(lsocket, "/var/lirc/lircd") == 0) {
		logprintf(LOG_ERR, "refusing to connect to lircd socket");
		return EXIT_FAILURE;
	}

	if(have_device)
		hw.device = lsocket;

	if((f = open(pidfile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) != -1) {
		if(read(f, buffer, BUFFER_SIZE) != -1) {
			//If the file is empty, create a new process
			if(!atoi(buffer)) {
				running = 0;
			} else {
				//Check if the process is running
				if(errno == ESRCH) {
					running = 0;
				}
			}
		}
	} else {
		logprintf(LOG_ERR, "could not open / create pidfile %s", pidfile);
		return EXIT_FAILURE;
	}
	close(f);

	if(running == 1) {
		nodaemon=1;
		logprintf(LOG_NOTICE, "already active (pid %d)", atoi(buffer));
		return EXIT_FAILURE;
	}

	module_init();

	/* Initialize peripheral modules */
	hw_init();

	if(have_config == 1 && config_read() != 0) {
		logprintf(LOG_ERR, "could not open config file %s", configfile);
		return EXIT_FAILURE;
	}

	if(get_loglevel() >= LOG_DEBUG) {
		config_print();
	}

	start_server(PORT);
	if(nodaemon == 0)
		deamonize();

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
