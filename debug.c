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
#include "libs/pilight/core/options.h"
#include "libs/pilight/core/datetime.h"
#include "libs/pilight/core/ssdp.h"
#include "libs/pilight/core/socket.h"
#include "libs/pilight/core/gc.h"
#include "libs/pilight/core/dso.h"

#include "libs/pilight/events/events.h"

#include "libs/pilight/hardware/hardware.h"

#ifndef _WIN32
	#include "libs/wiringx/wiringX.h"
#endif

static unsigned short main_loop = 1;
static unsigned short inner_loop = 1;
static int doSkip = 0;

static int normalize(int i, int pulselen) {
	double x;
	x=(double)i/pulselen;

	return (int)(round(x));
}

int main_gc(void) {
	log_shell_disable();
	main_loop = 0;
	inner_loop = 0;

	datetime_gc();
#ifdef EVENTS
	events_gc();
#endif
	options_gc();
	socket_gc();

	storage_gc();
	hardware_gc();
	whitelist_free();

#ifndef _WIN32
	wiringXGC();
#endif
	dso_gc();
	log_gc();
	gc_clear();

	FREE(progname);

	return EXIT_SUCCESS;
}

void *receivePulseTrain(void *param) {
	doSkip ^= 1;
	if(doSkip == 1) {
		return NULL;
	}

	struct threadpool_tasks_t *task = param;
	struct reason_received_pulsetrain_t *data = task->userdata;

	int pulselen = 0;
	int pulse = 0;

	struct tm tm;
	time_t now = 0;

	struct hardware_t *hw = NULL;
	int i = 0;

	if(data->hardware != NULL && data->pulses != NULL && data->length > 0) {
		if(hardware_select_struct(ORIGIN_MASTER, data->hardware, &hw) == 0) {
			memset(&tm, '\0', sizeof(struct tm));
			pulse = 0;
			inner_loop = 1;

			i = 0;
			time(&now);

			if(data->length > 0) {
				pulselen = data->pulses[data->length-1]/PULSE_DIV;

				if(pulselen > 25) {
					for(i=3;i<data->length;i++) {
						if((data->pulses[i]/pulselen) >= 2) {
							pulse=data->pulses[i];
							break;
						}
					}

					if(normalize(pulse, pulselen) > 0 && data->length > 25) {
						/* Print everything */
						printf("--[RESULTS]--\n");
						printf("\n");
#ifdef _WIN32
						localtime(&now);
#else
						localtime_r(&now, &tm);
#endif

#ifdef _WIN32
						printf("time:\t\t%s\n", asctime(&tm));
#else
						char buf[128];
						char *p = buf;
						memset(&buf, '\0', sizeof(buf));
						asctime_r(&tm, p);
						printf("time:\t\t%s", buf);
#endif
						printf("hardware:\t%s\n", hw->id);
						printf("pulse:\t\t%d\n", normalize(pulse, pulselen));
						printf("rawlen:\t\t%d\n", data->length);
						printf("pulselen:\t%d\n", pulselen);
						printf("\n");
						printf("Raw code:\n");
						for(i=0;i<data->length;i++) {
							printf("%d ", data->pulses[i]);
						}
						printf("\n");
					}
				}
			}
		}
	}
	return (void *)NULL;
}

// void *receiveOOK(void *param) {
	// int duration = 0;
	// int i = 0;
	// unsigned int y = 0;

	// int recording = 1;
	// int pulselen = 0;
	// int bit = 0;
	// int raw[MAXPULSESTREAMLENGTH] = {0};
	// int pRaw[MAXPULSESTREAMLENGTH] = {0};
	// int footer = 0;
	// int pulse = 0;
	// int rawLength = 0;
	// int plsdec = 1;

	// struct tm tm;
	// time_t now = 0, later = 0;

	// struct hardware_t *hw = (hardware_t *)param;

	// while(main_loop) {
		// memset(&raw, '\0', MAXPULSESTREAMLENGTH);
		// memset(&pRaw, '\0', MAXPULSESTREAMLENGTH);
		// memset(&tm, '\0', sizeof(struct tm));
		// recording = 1;
		// bit = 0;
		// footer = 0;
		// pulse = 0;
		// rawLength = 0;
		// inner_loop = 1;
		// pulselen = 0;

		// duration = 0;
		// i = 0;
		// y = 0;
		// time(&now);

		// while(inner_loop == 1 && hw->receiveOOK != NULL) {
			// duration = hw->receiveOOK();
			// time(&later);
			// if(difftime(later, now) > 1) {
				// inner_loop = 0;
			// }
			// /* If we are recording, keep recording until the next footer has been matched */
			// if(recording == 1) {
				// if(bit < MAXPULSESTREAMLENGTH) {
					// raw[bit++] = duration;
				// } else {
					// bit = 0;
					// recording = 0;
				// }
			// }

			// /* First try to catch code that seems to be a footer.
				 // If a real footer has been recognized, start using that as the new footer */
			// if((duration > 5100 && footer == 0) || ((footer-(footer*0.3)<duration) && (footer+(footer*0.3)>duration))) {
				// recording = 1;
				// pulselen = (int)duration/PULSE_DIV;

				// /* Check if we are recording similar codes */
				// for(i=0;i<(bit-1);i++) {
					// if(!(((pRaw[i]-(pRaw[i]*0.3)) < raw[i]) && ((pRaw[i]+(pRaw[i]*0.3)) > raw[i]))) {
						// y=0;
						// recording=0;
					// }
					// pRaw[i]=raw[i];
				// }
				// y++;

				// /* Continue if we have 2 matches */
				// if(y>=1) {
					// /* If we are certain we are recording similar codes. Save the raw code length. */
					// if(footer>0) {
						// if(rawLength == 0)
							// rawLength=bit;
					// }

					// if(pulselen > 1000) {
						// plsdec = 10;
					// }
					// /* Try to catch the footer, and the low and high values */
					// for(i=0;i<bit;i++) {
						// if((i+1)<bit && i > 2 && footer > 0) {
							// if((raw[i]/(pulselen/plsdec)) >= 2) {
								// pulse=raw[i];
							// }
						// }
						// if(duration > 5100) {
							// footer=raw[i];
						// }
					// }

					// /* If we have gathered all data, stop with the loop */
					// if(footer > 0 && pulse > 0 && rawLength > 0) {
						// inner_loop = 0;
					// }
				// }
				// bit=0;
			// }

			// fflush(stdout);
		// }

		// if(normalize(pulse, (pulselen/plsdec)) > 0 && rawLength > 25) {
			// /* Print everything */
			// printf("--[RESULTS]--\n");
			// printf("\n");
// #ifdef _WIN32
			// localtime(&now);
// #else
			// localtime_r(&now, &tm);
// #endif

// #ifdef _WIN32
			// printf("time:\t\t%s\n", asctime(&tm));
// #else
			// char buf[128];
			// char *p = buf;
			// memset(&buf, '\0', sizeof(buf));
			// asctime_r(&tm, p);
			// printf("time:\t\t%s", buf);
// #endif
			// printf("hardware:\t%s\n", hw->id);
			// printf("pulse:\t\t%d\n", normalize(pulse, (pulselen/plsdec)));
			// printf("rawlen:\t\t%d\n", rawLength);
			// printf("pulselen:\t%d\n", pulselen);
			// printf("\n");
			// printf("Raw code:\n");
			// for(i=0;i<rawLength;i++) {
				// printf("%d ",normalize(raw[i], (pulselen/plsdec))*(pulselen/plsdec));
			// }
			// printf("\n");
		// }
	// }

	// main_loop = 0;

	// return NULL;
// }

int main(int argc, char **argv) {
	atomicinit();

	gc_attach(main_gc);

	/* Catch all exit signals for gc */
	gc_catch();

	pilight.process = PROCESS_CLIENT;

	if((progname = MALLOC(15)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(progname, "pilight-debug");

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

	struct options_t *options = NULL;

	char *args = NULL;
	pid_t pid = 0;

	char *fconfig = NULL;
	if((fconfig = MALLOC(strlen(CONFIG_FILE)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(fconfig, CONFIG_FILE);

	options_add(&options, 'H', "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'V', "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'C', "config", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);

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
				printf("\t -C --config\t\tconfig file\n");
				goto clear;
			break;
			case 'V':
				printf("%s v%s\n", progname, PILIGHT_VERSION);
				goto clear;
			break;
			case 'C':
				if((fconfig = REALLOC(fconfig, strlen(args)+1)) == NULL) {
					OUT_OF_MEMORY
				}
				strcpy(fconfig, args);
			break;
			default:
				printf("Usage: %s [options]\n", progname);
				goto clear;
			break;
		}
	}
	options_delete(options);

#ifdef _WIN32
	if((pid = check_instances(L"pilight-debug")) != -1) {
		logprintf(LOG_NOTICE, "pilight-debug is already running");
		goto clear;
	}
#endif

	if((pid = isrunning("pilight-daemon")) != -1) {
		logprintf(LOG_NOTICE, "pilight-daemon instance found (%d)", (int)pid);
		goto clear;
	}

	if((pid = isrunning("pilight-raw")) != -1) {
		logprintf(LOG_NOTICE, "pilight-raw instance found (%d)", (int)pid);
		goto clear;
	}

	eventpool_init(EVENTPOOL_NO_THREADS);
	hardware_init();
	storage_init();
	if(storage_read(fconfig, CONFIG_HARDWARE | CONFIG_SETTINGS) != 0) {
		goto clear;
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
						// if(strcmp(hardware->id, "IRgpio") == 0) {
							// hardware->maxrawlen = MAXPULSESTREAMLENGTH;
							// hardware->minrawlen = 0;
							// hardware->maxgaplen = 10000;
							// hardware->mingaplen = 5100;
						// } else if(strcmp(hardware->id, "433gpio") == 0) {
							hardware->maxrawlen = MAXPULSESTREAMLENGTH;
							hardware->minrawlen = 25;
							hardware->maxgaplen = 34000;
							hardware->mingaplen = 5100;
						// }
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
							goto clear;
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

	printf("Press and hold one of the buttons on your remote or wait until\n");
	printf("another device such as a weather station has sent new codes\n");
	printf("The debugger will automatically reset itself after one second of\n");
	printf("failed leads. It will keep running until you explicitly stop it.\n");
	printf("This is done by pressing both the [CTRL] and C buttons on your keyboard.\n");

	eventpool_process(NULL);

clear:
	FREE(fconfig);
	if(main_loop == 1) {
		main_gc();
	}
	return (EXIT_FAILURE);
}
