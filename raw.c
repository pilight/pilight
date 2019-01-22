/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "libs/libuv/uv.h"
#include "libs/pilight/core/pilight.h"
#include "libs/pilight/core/network.h"
#include "libs/pilight/core/log.h"
#include "libs/pilight/core/datetime.h"
#include "libs/pilight/core/ssdp.h"
#include "libs/pilight/core/socket.h"
#include "libs/pilight/core/dso.h"

#include "libs/pilight/protocols/protocol.h"

#include "libs/pilight/events/events.h"

#ifndef _WIN32
	#include <wiringx.h>
#endif

static uv_signal_t *signal_req = NULL;
static unsigned short loop = 1;
static unsigned short linefeed = 0;

static int main_gc(void) {
	loop = 0;

	protocol_gc();
	hardware_gc();
	storage_gc();

#ifndef _WIN32
	wiringXGC();
#endif
	log_shell_disable();
	eventpool_gc();
	log_gc();

	FREE(progname);

	return EXIT_SUCCESS;
}

// void *receiveOOK(void *param) {
	// int duration = 0, iLoop = 0;

	// struct hardware_t *hw = (hardware_t *)param;
	// while(loop && hw->receiveOOK) {
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

static void *receivePulseTrain(int reason, void *param, void *userdata) {
	struct reason_received_pulsetrain_t *data = param;
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
	// while(loop && hw->receivePulseTrain) {
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

static void signal_cb(uv_signal_t *handle, int signum) {
	uv_stop(uv_default_loop());
	main_gc();
}

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void main_loop(int onclose) {
	if(onclose == 1) {
		signal_cb(NULL, SIGINT);
	}
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);	
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	if(onclose == 1) {
		while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
			usleep(10);
		}
	}
}

int main(int argc, char **argv) {
	const uv_thread_t pth_cur_id = uv_thread_self();
	memcpy((void *)&pth_main_id, &pth_cur_id, sizeof(uv_thread_t));

	struct options_t *options = NULL;
	char *args = NULL;
	char *fconfig = NULL;

	pilight.process = PROCESS_CLIENT;

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);	
	
	if((fconfig = MALLOC(strlen(CONFIG_FILE)+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(fconfig, CONFIG_FILE);

	if((progname = MALLOC(12)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(progname, "pilight-raw");

	if((signal_req = MALLOC(sizeof(uv_signal_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_signal_init(uv_default_loop(), signal_req);
	uv_signal_start(signal_req, signal_cb, SIGINT);	

#ifndef _WIN32
	if(geteuid() != 0) {
		printf("%s requires root privileges in order to run\n", progname);
		FREE(progname);
		exit(EXIT_FAILURE);
	}
#endif

	log_init();
	log_shell_enable();
	log_file_disable();
	log_level_set(LOG_NOTICE);

	options_add(&options, "H", "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "V", "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "C", "config", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "L", "linefeed", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);

	while (1) {
		int c;
		c = options_parse1(&options, argc, argv, 1, &args, NULL);
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
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
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
	options_gc();
	options = NULL;

	int *ret = NULL, n = 0;
	if((n = isrunning("pilight-raw", &ret)) > 1) {
		int i = 0;
		for(i=0;i<n;i++) {
			if(ret[i] != getpid()) {
				logprintf(LOG_NOTICE, "pilight-raw is already running (%d)", ret[i]);
				break;
			}
		}
		FREE(ret);
		goto close;
	}

	if((n = isrunning("pilight-daemon", &ret)) > 0) {
		logprintf(LOG_NOTICE, "pilight-daemon instance found (%d)", ret[0]);
		FREE(ret);
		goto close;
	}

	if((n = isrunning("pilight-debug", &ret)) > 0) {
		logprintf(LOG_NOTICE, "pilight-debug instance found (%d)", ret[0]);
		FREE(ret);
		goto close;
	}

	eventpool_init(EVENTPOOL_NO_THREADS);
	protocol_init();
	hardware_init();
	storage_init();
	if(storage_read(fconfig, CONFIG_HARDWARE | CONFIG_SETTINGS) != 0) {		
		FREE(fconfig);
		goto close;
	}
	FREE(fconfig);	

	eventpool_callback(REASON_RECEIVED_PULSETRAIN, receivePulseTrain, NULL);

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
		while(jchilds && loop == 1) {
			if(hardware_select_struct(ORIGIN_MASTER, jchilds->key, &hardware) == 0) {
				if(hardware->init != NULL) {
					if(hardware->init() == EXIT_FAILURE) {
						if(loop == 1) {
							logprintf(LOG_ERR, "could not initialize %s hardware mode", hardware->id);
							goto close;
						} else {
							break;
						}
					}
				}
			}
			if(loop == 0) {
				break;
			}

			jchilds = jchilds->next;
		}
	}

	main_loop(0);

close:
	if(options != NULL) {
		options_delete(options);
		options_gc();
		options = NULL;
	}

	main_loop(1);

	main_gc();

	return (EXIT_FAILURE);
}
