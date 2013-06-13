/*
	Copyright (C) Silver Moon (m00n.silv3r@gmail.com)
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
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <pthread.h>
#include <getopt.h>

#include "lirc.h"
#include "lirc/lircd.h"
#include "lirc/hardware.h"
#include "lirc/transmit.h"
#include "lirc/hw-types.h"
#include "lirc/hw_default.h"

#include "protocol.h"
#include "options.h"
#include "gc.h"
#include "protocols/kaku_switch.h"
#include "protocols/kaku_dimmer.h"
#include "protocols/kaku_old.h"
#include "protocols/elro.h"
#include "protocols/raw.h"
#include "protocols/alecto.h"
#include "config.h"

/* ID's or the clients */
#define RECEIVER		1
#define SENDER			2

int serverSocket;
int clientSocket;
int	clients[MAX_CLIENTS];
/* If we have accepted a client, handshakes will store the type of client */
int handshakes[MAX_CLIENTS];

/* The pidfile and pid of this daemon */
char *pidfile = PID_FILE;
pid_t pid;
/* The default config file location */
char *configfile = CONFIG_FILE;
/* The to call when a new code is send or received */
char logfile[1024];
/* The duration of the received pulse */
int duration = 0;
/* The number of receivers connected */
int receivers = 0;
/* Daemonize or not */
int nodaemon = 0;
/* How many times does to keep need to be resend */
int repeat = SEND_REPEATS;
/* The frequency on which the lirc module needs to send 433.92Mhz*/
unsigned int freq = FREQ433; // Khz
/* Are we currently sending code */
int sending = 0;
/* Is the module initialized */
int initialized = 0;
/* Is the right frequency set */
int setfreq = 0;

/* The CLI options of the specific protocol */
struct options_t *sendOptions;
struct options_t *node;

void logprintf(int prio, char *format_str, ...) { }

void logperror(int prio, const char *s) { }

/* Initialize the hardware module lirc_rpi */
int module_init() {
	if(initialized == 0) {

		if(!hw.init_func()) {
			fprintf(stderr, "%s: could not open %s\n", progname, hw.device);
			fprintf(stderr, "%s: default_init(): Device or resource busy\n", progname);
			return EXIT_FAILURE;
		}

		/* Only set the frequency once */
		if(setfreq == 0) {
			freq = FREQ433;
			/* Set the lirc_rpi frequency to 433.92Mhz */
			if(hw.ioctl_func(LIRC_SET_SEND_CARRIER, &freq) == -1) {
				perror("ioctl");
				return EXIT_FAILURE;
			}
			setfreq = 1;
		}
		initialized = 1;
	}
	return EXIT_SUCCESS;
}

/* Release the hardware module lirc_rpi */
int module_deinit() {
	if(initialized == 1) {
		/* Restore the lirc_rpi frequency to its default value */
		hw.deinit_func();
		initialized	= 0;
	}
	return EXIT_SUCCESS;
}

/* Start the socket server */
int start_server(int port) {
	int opt = 1;
    struct sockaddr_in address;

    //initialise all clients[] to 0 so not checked
	memset(clients,0,sizeof(clients));
	memset(handshakes,0,sizeof(handshakes));

    //create a master socket
    if((serverSocket = socket(AF_INET , SOCK_STREAM , 0)) == 0)  {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    //set master socket to allow multiple connections , this is just a good habit, it will work without this
    if(setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    //bind the socket to localhost
    if (bind(serverSocket, (struct sockaddr *)&address, sizeof(address))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    //try to specify maximum of 3 pending connections for the master socket
    if(listen(serverSocket, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    return 0;
}

/* Send a specific code */
void send_code(struct options_t *options) {
	int i=0, match = 0, x = 0, logged = 1;
	char name[255];
	/* Hold the final protocol struct */
	protocol_t *device = malloc(sizeof(protocol_t));
	/* The code that is send to the hardware wrapper */
	struct ir_ncode code;
	/* Temporary pointer to the protocol options */
	struct option *backup_options;
	struct options_t *node = options;
	extern FILE *popen();
	FILE *f;
	char message[BUFFER_SIZE];
	char cmd[255];
	memset(message,'0',BUFFER_SIZE);

	if(getOption(options,'p') != NULL)
		strcpy(name,getOption(options,'p'));

	for(i=0; i<protocols.nr; ++i) {
		device = protocols.listeners[i];
		/* Check if the protocol exists */
		if(strcmp(device->id,name) == 0 && match == 0) {
			match = 1;
			if(device->options != NULL) {
				/* Copy all CLI options from the specific protocol */
				backup_options=device->options;
				x=0;
				/* Get the full CLI argument name to send to the receivers */
				while(backup_options->name != NULL) {
					node = options;
					while(node->id != 0) {
						if(node->id == backup_options->val) {
							if(backup_options->has_arg == 1) {
								x+=sprintf(message+x,"%s %s ",backup_options->name,getOption(options,node->id));
							} else {
								x+=sprintf(message+x,"%s ",backup_options->name);
							}
						}
						node = node->next;
					}
					backup_options++;
				}
			}
			break;
		}
	}
	logged = 0;

	/* If we matched a protocol and are not already sending, continue */
	if(match == 1 && sending == 0) {
		/* Let the protocol create his code */
		device->createCode(options);

		/* Check if we need to repeat the code other than the default repeat */
		if(atoi(getOption(options,'r')) > 0)
			repeat=atoi(getOption(options,'r'));

		sending = 1;
		/* Create a single code with all repeats included */
		int longCode[(device->rawLength*repeat)+1];
		for(i=0;i<repeat;i++) {
			for(x=0;x<device->rawLength;x++) {
				longCode[x+(device->rawLength*i)]=device->raw[x];
			}
		}
		/* Send this single big code at once */
		code.length = (x+(device->rawLength*(i-1)))+1;
		code.signals = longCode;
		/* If we have no receiver, we need to initialize the hardware first */
		if(receivers == 0)
			module_init();

		/* Send the code, if we succeeded, inform the receiver */
		if(send_ir_ncode(&code) == 1) {
			if(receivers > 0 && logged == 0) {
				logged = 1;
				/* Write the message to all receivers */
				for(x=0;x<MAX_CLIENTS;x++) {
					if(handshakes[x] == RECEIVER) {
						send(clients[x], message, strlen(message), 0);
					}
				}

				/* Call the external file */
				if(strlen(logfile) > 0) {
					memset(cmd,'0',255);
					strcpy(cmd,logfile);
					strcat(cmd," sender ");
					strcat(cmd,message);
					f=popen(cmd, "r");
					pclose(f);
				}
			}
		}
		/* Release the hardware to spare resources */
		if(receivers == 0)
			module_deinit();
		sending = 0;
	}
}

/* Parse the incoming data from the client */
int parse_data(int i, char buffer[BUFFER_SIZE]) {
    char message[BUFFER_SIZE];
	int sd = clients[i];
	char *pch = NULL;
	bzero(message,BUFFER_SIZE);
	
	if(handshakes[i] == SENDER) {
		if(strcmp(buffer,"SEND\n") == 0) {
			/* Don't let the sender wait until we have send the code */
			close(sd);
			clients[i] = 0;
			handshakes[i] = 0;
			send_code(node);
			sendOptions = malloc(25*sizeof(struct options_t));
		} else {
			pch = strtok(buffer," \n");
			while(pch != NULL) {
				/* Rebuild the CLI arguments struct */
				node = malloc(sizeof(struct options_t));
				node->id = atoi(pch);
				pch = strtok(NULL," \n");
				if(pch != NULL) {
					node->value = strdup(pch);
					node->next = sendOptions;
					sendOptions = node;
					pch = strtok(NULL," \n");
				} else {
					break;
				}
			}
		}
	}
	if(strcmp(buffer,"CLIENT RECEIVER\n") == 0) {
		strcpy(message,"ACCEPT CLIENT\n");
		handshakes[i] = RECEIVER;
		receivers++;
	}
	if(strcmp(buffer,"CLIENT SENDER\n") == 0 && sending == 0) {
		strcpy(message,"ACCEPT CLIENT\n");
		handshakes[i] = SENDER;
	}
	if(handshakes[i] == 0) {
		strcpy(message,"REJECT CLIENT\n");
	}
	if(strlen(message) > 0 && send(sd, message, strlen(message), MSG_NOSIGNAL) == -1) {
		close(sd);
		clients[i] = 0;
		handshakes[i] = 0;
	}	
	return 1;
}

void receive() {
	lirc_t data;
	int x, y = 0, i;
	protocol_t *device = malloc(sizeof(protocol_t));
	char message[BUFFER_SIZE];
	extern FILE *popen();
	FILE *f;

	while(1) {
		/* Only initialize the hardware receive the data when there are receivers connected */
		if(receivers > 0) {
			module_init();

			data = hw.readdata(0);
			duration = (data & PULSE_MASK);
			/* A space is normally for 295 long, so filter spaces less then 200 */
			if(sending == 0 && duration > 200) {

				for(i=0; i<protocols.nr; ++i) {
					device = protocols.listeners[i];

					/* If we are recording, keep recording until the footer has been matched */
					if(device->recording == 1) {
						if(device->bit < 255) {
							device->raw[device->bit++] = duration;
						} else {
							device->bit = 0;
							device->recording = 0;
						}
					}

					/* Try to catch the header of the code */
					if(duration > (PULSE_LENGTH-(PULSE_LENGTH*device->multiplier[0]))
					   && duration < (PULSE_LENGTH+(PULSE_LENGTH*device->multiplier[0]))
					   && device->bit == 0) {
						device->raw[device->bit++] = duration;
					} else if(duration > ((device->header*PULSE_LENGTH)-((device->header*PULSE_LENGTH)*device->multiplier[0]))
					   && duration < ((device->header*PULSE_LENGTH)+((device->header*PULSE_LENGTH)*device->multiplier[0]))
					   && device->bit == 1) {
						device->raw[device->bit++] = duration;
						device->recording = 1;
					}

					/* Try to catch the footer of the code */
					if(duration > ((device->footer*PULSE_LENGTH)-((device->footer*PULSE_LENGTH)*device->multiplier[1]))
					   && duration < ((device->footer*PULSE_LENGTH)+((device->footer*PULSE_LENGTH)*device->multiplier[1]))) {

						/* Check if the code matches the raw length */
						if(device->bit == device->rawLength) {
							device->parseRaw();

							/* Convert the raw codes to one's and zero's */
							for(x=0;x<device->bit;x++) {

								/* Check if the current code matches the previous one */
								if(device->pCode[x]!=device->code[x])
									y=0;
								device->pCode[x]=device->code[x];
								if(device->raw[x] > ((device->pulse*PULSE_LENGTH)-PULSE_LENGTH)) {
									device->code[x]=1;
								} else {
									device->code[x]=0;
								}
							}

							y++;

							/* Continue if we have recognized enough repeated codes */
							if(y >= device->repeats) {
								device->parseCode();

								/* Convert the one's and zero's into binary */
								for(x=2; x<device->bit; x+=4) {
									if(device->code[x+1] == 1) {
										device->binary[x/4]=1;
									} else {
										device->binary[x/4]=0;
									}
								}

								/* Check if the binary matches the binary length */
								if((x/4) == device->binaryLength) {
									memset(message,'0',BUFFER_SIZE);
									memcpy(message,device->parseBinary(),BUFFER_SIZE);
									if(message != NULL) {
										/* Write the message to all receivers */
										for(i=0;i<MAX_CLIENTS;i++) {
											if(handshakes[i] == RECEIVER) {
												send(clients[i], message, strlen(message), 0);
											}
										}

										/* Call the external file */
										if(strlen(logfile) > 0) {
											char cmd[255];
											strcpy(cmd,logfile);
											strcat(cmd," receiver ");
											strcat(cmd,message);
											f=popen(cmd, "r");
											pclose(f);
										}

										continue;
									}
								}
							}
						}
						device->recording = 0;
						device->bit = 0;
					}
				}
			}
		} else {
			sleep(1);
		}
	}
}

void *wait_for_data(void *param) {
	int activity;
	int i, n, sd;
    int max_sd;
    struct sockaddr_in address;
	int addrlen = sizeof(address);
    char readBuff[BUFFER_SIZE];
	fd_set readfds;
    char *message = "ACCEPT CONNECTION\n";

	while(1) {
		do {
			//clear the socket set
			FD_ZERO(&readfds);

			//add master socket to set
			FD_SET(serverSocket, &readfds);
			max_sd = serverSocket;

			//add child sockets to set
			for(i=0;i<MAX_CLIENTS;i++) {
				//socket descriptor
				sd = clients[i];

				//if valid socket descriptor then add to read list
				if(sd > 0)
					FD_SET(sd, &readfds);

				//highest file descriptor number, need it for the select function
				if(sd > max_sd)
					max_sd = sd;
			}
			//wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
			activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
		} while(activity == -1 && errno == EINTR);

        //If something happened on the master socket , then its an incoming connection
        if(FD_ISSET(serverSocket, &readfds)) {
            if((clientSocket = accept(serverSocket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            //inform user of socket number - used in send and receive commands
            //printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , clientSocket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

            //send new connection accept message
            if(send(clientSocket, message, strlen(message), 0) != strlen(message)) {
                perror("send");
            }

            //puts("Welcome message sent successfully");

            //add new socket to array of sockets
            for(i=0;i<MAX_CLIENTS;i++) {
                //if position is empty
                if(clients[i] == 0) {
                    clients[i] = clientSocket;
                    //printf("Adding to list of sockets as %d\n" , i);
                    break;
                }
            }
        }
        memset(readBuff,'0',BUFFER_SIZE);
        //else its some IO operation on some other socket :)
        for(i=0;i<MAX_CLIENTS;i++) {
			sd = clients[i];

            if(FD_ISSET(sd , &readfds)) {
                //Check if it was for closing, and also read the incoming message
                if ((n = read(sd ,readBuff, sizeof(readBuff)-1)) == 0) {
                    //Somebody disconnected, get his details and print
                    //getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                    //printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

					if(handshakes[i] == RECEIVER)
						receivers--;
					/* Release the hardware when no receiver are connected */
					if(receivers == 0 && sending == 0)
						module_deinit();

                    //Close the socket and mark as 0 in list for reuse
                    close(sd);
                    clients[i] = 0;
					handshakes[i] = 0;
                } else {
                    //set the string terminating NULL byte on the end of the data read
                    readBuff[n] = '\0';
					
					if(n > -1)
						parse_data(i, readBuff);
                }
            }
        }
    }
	return 0;
}

void deamonize() {
	int f;
	char buffer[BUFFER_SIZE];

	//Get the pid of the fork
	pid_t npid = fork();
	switch(npid) {
		case 0:
		break;
		case -1:
			perror("fork");
			exit(1);
		break;
		default:
			if((f = open(pidfile, O_RDWR | O_CREAT | O_TRUNC)) != -1) {
				lseek(f, 0, SEEK_SET);
				sprintf(buffer,"%d",npid);
				write(f,buffer,strlen(buffer));
			}
			close(f);
			exit(0);
		break;
	}
}

/* Garbage collector of main program */
int main_gc() {

	module_init();
	/* Only restore the frequency when we stop the daemon */
	freq = FREQ38;
	if(hw.ioctl_func(LIRC_SET_SEND_CARRIER, &freq) == -1) {
		perror("ioctl");
		return EXIT_FAILURE;
	}
	module_deinit();
	/* Remove the stale pid file */
	if(access(pidfile, F_OK) != -1) {
		remove(pidfile);
	}
	return 0;
}

int main(int argc , char **argv) {
	/* Run main garbage collector when quiting the daemon */
	gc_attach(main_gc);

	/* Catch all exit signals for gc */
	gc_catch();

	progname = malloc((10*sizeof(char))+1);
	progname = "433-daemon";

	pthread_t pth;
	char *lsocket = "/dev/lirc0";
	char buffer[BUFFER_SIZE];
	int have_device = 0;
	int have_config = 0;
	int f;
	int running = 1;
	sendOptions = malloc(25*sizeof(struct options_t));

	hw_choose_driver(NULL);
	while (1) {
		int c;
		static struct option long_options[] = {
			{"help", no_argument, NULL, 'h'},
			{"version", no_argument, NULL, 'v'},
			{"socket", required_argument, NULL, 's'},
			{"log", required_argument, NULL, 'l'},
			{"nodaemon", required_argument, NULL, 'f'},
			{"config", required_argument, NULL, 'c'},
			{0, 0, 0, 0}
		};
		c = getopt_long(argc, argv, "hvs:l:dc:", long_options, NULL);
		if (c == -1)
			break;
		switch (c) {
			case 'h':
				printf("Usage: %s [options]\n", progname);
				printf("\t -h --help\t\tdisplay usage summary\n");
				printf("\t -v --version\t\tdisplay version\n");
				printf("\t -s --socket=socket\tread from given socket\n");
				printf("\t -l --log=file\tfile to call after received / send code\n");
				printf("\t -c --config=file\tconfig file\n");
				printf("\t -d --nodaemon\tdo not daemonize\n");
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
					strcpy(configfile, optarg);
					have_config = 1;
				} else {
					fprintf(stderr, "%s: the config file %s does not exists\n", progname, optarg);
					return EXIT_FAILURE;
				}
			break;
			case 'd':
				nodaemon=1;
			break;
			case 'l':
				if(access(optarg, F_OK) != -1) {
					strcpy(logfile, optarg);
					receivers++;
				} else {
					fprintf(stderr, "%s: the log file %s does not exists\n", progname, optarg);
					return EXIT_FAILURE;
				}
			break;
			default:
				printf("Usage: %s [options]\n", progname);
				return (EXIT_FAILURE);
			break;
		}
	}
	if(optind < argc) {
		fprintf(stderr, "%s: too many arguments\n", progname);
		return EXIT_FAILURE;
	}

	if(strcmp(lsocket, "/var/lirc/lircd") == 0) {
		fprintf(stderr, "%s: refusing to connect to lircd socket\n", progname);
		return EXIT_FAILURE;
	}

	if(have_device)
		hw.device = lsocket;

	if((f = open(pidfile, O_RDWR | O_CREAT)) != -1) {
		read(f, buffer, 255);
		//If the file is empty, create a new process
		if(!atoi(buffer)) {
			running = 0;
		} else {
			//Check if the process is running
			kill(atoi(buffer), 0);
			if(errno == ESRCH) {
				running = 0;
			}
		}
	}
	close(f);
	if(running == 1) {
		fprintf(stderr, "%s: already active (pid %d)\n", progname, atoi(buffer));
		return EXIT_FAILURE;
	}

	/* Initialize peripheral modules */
	kakuSwInit();
	kakuDimInit();
	kakuOldInit();
	elroInit();
	rawInit();
	alectoInit();

	if(have_config == 1 || access(configfile, F_OK) != -1) {
		if(read_config(configfile) != 0) {	
			return EXIT_FAILURE;
		}
	}
	
	// while(locations != NULL) {
		// printf("%s\t\t%s\n", locations->id, locations->name);
		// while(locations->devices != NULL) {
			// printf("- %s\t\t%s\t%s\n",locations->devices->id,locations->devices->name,locations->devices->protocol->id);
			// while(locations->devices->settings != NULL) {
				// printf("  *%s: %s\n",locations->devices->settings->name, locations->devices->settings->value);
				// locations->devices->settings = locations->devices->settings->next;
			// }
			// locations->devices = locations->devices->next;			
		// }
		// locations = locations->next;
	// }

	start_server(PORT);
	if(nodaemon == 0)
		deamonize();

	/* Make sure the server part is non-blocking by creating a new thread */
	pthread_create(&pth, NULL, wait_for_data, NULL);
	receive();
	pthread_join(pth,NULL);

	return EXIT_FAILURE;
}
