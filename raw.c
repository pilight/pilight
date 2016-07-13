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

#include "libs/pilight/core/threadpool.h"
#include "libs/pilight/core/pilight.h"
#include "libs/pilight/core/network.h"
#include "libs/pilight/core/log.h"
#include "libs/pilight/core/datetime.h"
#include "libs/pilight/core/ssdp.h"
#include "libs/pilight/core/socket.h"
#include "libs/pilight/core/dso.h"
#include "libs/pilight/core/gc.h"

#include "libs/pilight/protocols/protocol.h"

#include "libs/pilight/events/events.h"

#ifndef _WIN32
	#include "libs/wiringx/wiringX.h"
#endif

static unsigned short main_loop = 1;
static unsigned short linefeed = 0;

int main_gc(void) {
	log_shell_disable();
	main_loop = 0;

	datetime_gc();
#ifdef EVENTS
	events_gc();
#endif
	options_gc();
	socket_gc();

	storage_gc();
	whitelist_free();

#ifndef _WIN32
	wiringXGC();
#endif
	dso_gc();
	log_gc();
	gc_clear();

	FREE(progname);

#ifdef _WIN32
	WSACleanup();
#endif

	return EXIT_SUCCESS;
}

// void *receiveOOK(void *param) {
	// int duration = 0, iLoop = 0;

	// struct hardware_t *hw = (hardware_t *)param;
	// while(main_loop && hw->receiveOOK) {
		// duration = hw->receiveOOK();
		// iLoop++;
		// if(duration > 0) {
			// if(linefeed == 1) {
				// if(duration > 5100) {
					// printf(" %d -#: %d\n%s: ", duration, iLoop, hw->id);
					// iLoop = 0;
				// } else {
					// printf(" %d", duration);
				// }
			// } else {
				// printf("%s: %d\n", hw->id, duration);
			// }
		// }
	// };
	// return NULL;
// }

void *receivePulseTrain(void *param) {
	struct threadpool_tasks_t *task = param;
	struct reason_received_pulsetrain_t *data = task->userdata;
	struct hardware_t *hw = NULL;
	int i = 0;

	if(data->hardware != NULL && data->pulses != NULL && data->length > 0) {
		if(hardware_select_struct(ORIGIN_MASTER, data->hardware, &hw) == 0) {
			if(data->length > 0) {
				for(i=0;i<data->length;i++) {
					if(linefeed == 1) {
						printf(" %d", data->pulses[i]);
						if(data->pulses[i] > 5100) {
							printf(" -# %d\n %s:", i, hw->id);
						}
					} else {
						printf("%s: %d\n", hw->id, data->pulses[i]);
					}
				}
			}
		}
	}
	return (void *)NULL;
}

// void *receivePulseTrain(void *param) {
	// struct rawcode_t r;
	// int i = 0;

	// struct hardware_t *hw = (hardware_t *)param;
	// while(main_loop && hw->receivePulseTrain) {
		// hw->receivePulseTrain(&r);
		// if(r.length == -1) {
			// main_gc();
			// break;
		// } else if(r.length > 0) {
			// for(i=0;i<r.length;i++) {
				// if(linefeed == 1) {
					// printf(" %d", r.pulses[i]);
					// if(r.pulses[i] > 5100) {
						// printf(" -# %d\n %s:", i, hw->id);
					// }
				// } else {
					// printf("%s: %d\n", hw->id, r.pulses[i]);
				// }
			// }
		// }
	// };
	// return NULL;
// }

int main(int argc, char **argv) {
	atomicinit();
	struct options_t *options = NULL;
	char *args = NULL;
	char *fconfig = NULL;
	pid_t pid = 0;

	pilight.process = PROCESS_CLIENT;

	if((fconfig = MALLOC(strlen(CONFIG_FILE)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(fconfig, CONFIG_FILE);

	gc_attach(main_gc);

	/* Catch all exit signals for gc */
	gc_catch();

	if((progname = MALLOC(12)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(progname, "pilight-raw");

#ifndef _WIN32
	if(geteuid() != 0) {
		printf("%s requires root privileges in order to run\n", progname);
		FREE(progname);
		exit(EXIT_FAILURE);
	}
#endif

	log_shell_enable();
	log_file_disable();
	log_level_set(LOG_NOTICE);

	options_add(&options, 'H', "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'V', "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'C', "config", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'L', "linefeed", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);

	while (1) {
		int c;
		c = options_parse(&options, argc, argv, 1, &args);
		if(c == -1)
			break;
		if(c == -2)
			c = 'H';
		switch (c) {
			case 'H':
				printf("Usage: %s [options]\n", progname);
				printf("\t -H --help\t\tdisplay usage summary\n");
				printf("\t -V --version\t\tdisplay version\n");
 				printf("\t -L --linefeed\t\tstructure raw printout\n");
 				printf("\t -C --config\t\tconfig file\n");
				goto close;
			break;
			case 'L':
				linefeed = 1;
			break;
			case 'V':
				printf("%s v%s\n", progname, PILIGHT_VERSION);
				goto close;
			break;
			case 'C':
				if((fconfig = REALLOC(fconfig, strlen(args)+1)) == NULL) {
					OUT_OF_MEMORY
				}
				strcpy(fconfig, args);
			break;
			default:
				printf("Usage: %s [options]\n", progname);
				goto close;
			break;
		}
	}
	options_delete(options);

#ifdef _WIN32
	if((pid = check_instances(L"pilight-raw")) != -1) {
		logprintf(LOG_NOTICE, "pilight-raw is already running");
		goto close;
	}
#endif

	if((pid = isrunning("pilight-daemon")) != -1) {
		logprintf(LOG_NOTICE, "pilight-daemon instance found (%d)", (int)pid);
		goto close;
	}

	if((pid = isrunning("pilight-debug")) != -1) {
		logprintf(LOG_NOTICE, "pilight-debug instance found (%d)", (int)pid);
		goto close;
	}

	eventpool_init(EVENTPOOL_NO_THREADS);
	hardware_init();
	storage_init();
	if(storage_read(fconfig, CONFIG_HARDWARE | CONFIG_SETTINGS) != 0) {
		goto close;
	}

	eventpool_callback(REASON_RECEIVED_PULSETRAIN, receivePulseTrain);

	struct JsonNode *jrespond = NULL;
	struct JsonNode *jchilds = NULL;
	struct hardware_t *hardware = NULL;
	if(hardware_select(ORIGIN_MASTER, NULL, &jrespond) == 0) {
		jchilds = json_first_child(jrespond);
		while(jchilds) {
			if(hardware_select_struct(ORIGIN_MASTER, jchilds->key, &hardware) == 0) {
				if(hardware->init != NULL) {
					if(hardware->comtype == COMOOK) {
						hardware->maxrawlen = 1024;
						hardware->minrawlen = 0;
						hardware->maxgaplen = 99999;
						hardware->mingaplen = 0;
					}
				}
			}

			jchilds = jchilds->next;
		}
	}

	if(hardware_select(ORIGIN_MASTER, NULL, &jrespond) == 0) {
		jchilds = json_first_child(jrespond);
		while(jchilds && main_loop == 1) {
			if(hardware_select_struct(ORIGIN_MASTER, jchilds->key, &hardware) == 0) {
				if(hardware->init != NULL) {
					if(hardware->init(receivePulseTrain) == EXIT_FAILURE) {
						if(main_loop == 1) {
							logprintf(LOG_ERR, "could not initialize %s hardware mode", hardware->id);
							goto close;
						} else {
							break;
						}
					}
				}
			}
			if(main_loop == 0) {
				break;
			}

			jchilds = jchilds->next;
		}
	}

	eventpool_process(NULL);

close:
	if(args != NULL) {
		FREE(args);
	}
	FREE(fconfig);
	if(main_loop == 1) {
		main_gc();
	}
	return (EXIT_FAILURE);
}
