/*
  Copyright (C) CurlyMo

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
#include <assert.h>

#ifndef _WIN32
#include <wiringx.h>
#endif

#include "libs/pilight/core/threads.h"
#include "libs/pilight/core/pilight.h"
#include "libs/pilight/core/network.h"
#include "libs/pilight/core/log.h"
#include "libs/pilight/core/options.h"
#include "libs/pilight/core/threads.h"
#include "libs/pilight/core/datetime.h"
#include "libs/pilight/core/ssdp.h"
#include "libs/pilight/core/socket.h"
#include "libs/pilight/core/gc.h"
#include "libs/pilight/core/dso.h"
#include "libs/pilight/config/config.h"
#include "libs/pilight/config/hardware.h"
#include "libs/pilight/lua_c/lua.h"
#include "libs/pilight/lua_c/table.h"

#include "libs/pilight/events/events.h"

static unsigned short main_loop = 1;
static unsigned short inner_loop = 1;

static uv_signal_t *signal_req = NULL;

static char *lua_root = LUA_ROOT;
static int raw[MAXPULSESTREAMLENGTH] = {0};
static int pRaw[MAXPULSESTREAMLENGTH] = {0};
static int rawLength = 0;
static int buffer[WIRINGX_BUFFER];
static int footer = 0;
static int pulselen = 0;
static int bit = 0;
static int iter = 0;

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
	ssdp_gc();
#ifdef EVENTS
	events_gc();
#endif
	options_gc();
	socket_gc();

	eventpool_gc();
	config_write(1, "all");
	config_gc();
	protocol_gc();
	whitelist_free();
	threads_gc();

	plua_gc();
#ifndef _WIN32
	wiringXGC();
#endif
	dso_gc();
	log_gc();
	gc_clear();

	FREE(progname);
	xfree();

	return EXIT_SUCCESS;
}

static void *received_pulsetrain(int reason, void *param, void *userdata) {
	int pulse = 0;
	int buffer[WIRINGX_BUFFER];

	struct tm tm;
	time_t now = 0;

	char *hardware = NULL;
	double length = 0.0;

	{
		struct plua_metatable_t *table = param;
		char nr[255], *p = nr;
		double pulse = 0.0;

		memset(&nr, 0, 255);

		int i = 0;
		plua_metatable_get_number(table, "length", &length);
		plua_metatable_get_string(table, "hardware", &hardware);

		printf("\n");
		for(i=0;i<length;i++) {
			snprintf(p, 254, "pulses.%d", i+1);
			plua_metatable_get_number(table, nr, &pulse);
			buffer[i] = (int)pulse;
			printf("%d ", (int)pulse);
		}
		printf("\n");
	}

	int i = 0;
	if((int)length > 0) {
		pulselen = buffer[(int)length-1]/PULSE_DIV;

		if(pulselen > 25) {
			for(i=3;i<(int)length;i++) {
				if((buffer[i]/pulselen) >= 2) {
					pulse = buffer[i];
					break;
				}
			}

			if(normalize(pulse, pulselen) > 0 && (int)length > 25) {
				/* Print everything */
				printf("--[RESULTS]--\n");
				printf("\n");
#ifdef _WIN32
				memcpy(&tm, localtime(&now), sizeof(struct tm));
#else
				localtime_r(&now, &tm);
#endif

#ifdef _WIN32
				printf("time:\t\t%s\n", asctime(&tm));
#else
				time_t rawtime;
				struct tm * timeinfo;

				time(&rawtime);
				timeinfo = localtime(&rawtime);
				printf("time:\t\t%s", asctime(timeinfo));
#endif
				printf("hardware:\t%s\n", hardware);
				printf("pulse:\t\t%d\n", normalize(pulse, pulselen));
				printf("rawlen:\t\t%d\n", (int)length);
				printf("pulselen:\t%d\n", pulselen);
				printf("\n");
				printf("Raw code:\n");
				for(i=0;i<length;i++) {
					printf("%d ", buffer[i]);
				}
				printf("\n");
			}
		}
		iter = 0;
	}
	return NULL;
}

static void *received_ook(int reason, void *param, void *userdata) {
	int recording = 0;
	int pulselen = 0;
	int pulse = 0;
	int plsdec = 1;
	int hasnul = 0;

	struct tm tm;
	time_t now = 0;

	char *hardware = NULL;
	double length = 0.0;

	{
		struct plua_metatable_t *table = param;
		char nr[255], *p = nr;
		double pulse = 0.0;

		memset(&nr, 0, 255);

		int i = 0;
		plua_metatable_get_number(table, "length", &length);
		plua_metatable_get_string(table, "hardware", &hardware);

		for(i=0;i<length;i++) {
			snprintf(p, 254, "pulses.%d", i+1);
			plua_metatable_get_number(table, nr, &pulse);
			if(iter > 1024) {
				iter = 0;
			}
			buffer[iter++] = (int)pulse;
		}
	}

	int z = 0, duration = 0, i = 0, y = 0;
	for(z=0;z<iter;z++) {
		duration = buffer[z];

		/* If we are recording, keep recording until the next footer has been matched */
		if(recording == 1 && bit < MAXPULSESTREAMLENGTH) {
			raw[bit++] = duration;
		} else {
				bit = 0;
				recording = 0;
				footer = 0;
				rawLength = 0;
				plsdec = 1;
		}

		/* First try to catch code that seems to be a footer.
			 If a real footer has been recognized, start using that as the new footer */
		if((duration > 5100 && footer == 0) || ((footer-(footer*0.3)<duration) && (footer+(footer*0.3)>duration))) {
			recording = 1;
			pulselen = (int)duration/PULSE_DIV;

			/* Check if we are recording similar codes */
			for(i=0;i<(bit-1);i++) {
				if(!(((pRaw[i]-(pRaw[i]*0.3)) < raw[i]) && ((pRaw[i]+(pRaw[i]*0.3)) > raw[i]))) {
					y=0;
					recording=0;
				}
				pRaw[i]=raw[i];
			}
			y++;

			/* Continue if we have 2 matches */
			if(y>=1) {
				/* If we are certain we are recording similar codes. Save the raw code length. */
				if(footer>0) {
					if(rawLength == 0)
						rawLength=bit;
				}

				if(pulselen > 1000) {
					plsdec = 10;
				}
				/* Try to catch the footer, and the low and high values */
				for(i=0;i<bit;i++) {
					if((i+1)<bit && i > 2 && footer > 0) {
						if((raw[i]/(pulselen/plsdec)) >= 2) {
							pulse=raw[i];
						}
					}
					if(duration > 5100) {
						footer=raw[i];
					}
				}

				/* If we have gathered all data, stop with the loop */
				if(footer > 0 && pulse > 0 && rawLength > 0) {
					inner_loop = 0;
				}
			}
			bit = 0;
		}
	}

	for(i=0;i<rawLength;i++) {
		if((normalize(raw[i], (pulselen/plsdec))*(pulselen/plsdec)) == 0) {
			hasnul = 1;
			break;
		}
	}

	if(normalize(pulse, (pulselen/plsdec)) > 0 && rawLength > 25) {
		iter = 0;
		if(hasnul == 0) {
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
			time_t rawtime;
			struct tm * timeinfo;

			time(&rawtime);
			timeinfo = localtime(&rawtime);
			printf("time:\t\t%s", asctime(timeinfo));
#endif
			printf("hardware:\t%s\n", hardware);
			printf("pulse:\t\t%d\n", normalize(pulse, (pulselen/plsdec)));
			printf("rawlen:\t\t%d\n", rawLength);
			printf("pulselen:\t%d\n", pulselen);
			printf("\n");
			printf("Raw code:\n");
			for(i=0;i<rawLength;i++) {
				printf("%d ",normalize(raw[i], (pulselen/plsdec))*(pulselen/plsdec));
			}
			printf("\n");
		}
	}
	memcpy(&buffer[0], &buffer[iter], (iter)*sizeof(int));
	iter = 0;

	return NULL;
}

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

static void main_loop1(int onclose) {
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

	memtrack();

	atomicinit();

	gc_attach(main_gc);

	/* Catch all exit signals for gc */
	gc_catch();

	if((progname = MALLOC(15)) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	strcpy(progname, "pilight-debug");

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

	log_shell_enable();
	log_file_disable();
	log_level_set(LOG_NOTICE);

	struct options_t *options = NULL;

	char *configtmp = CONFIG_FILE;
	int help = 0;

	options_add(&options, "H", "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "V", "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "C", "config", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "Ls", "storage-root", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "Ll", "lua-root", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);

	// if(argc == 1) {
		// printf("Usage: %s [options]\n", progname);
		// goto clear;
	// }

	if(options_parse(options, argc, argv, 1) == -1) {
		help = 1;
	}

	if(options_exists(options, "H") == 0 || help == 1) {
		printf("Usage: %s [options]\n", progname);
		printf("\t -H  --help\t\t\tdisplay usage summary\n");
		printf("\t -V  --version\t\t\tdisplay version\n");
		printf("\t -C  --config\t\t\tconfig file\n");
		printf("\t -Ls --storage-root=xxxx\tlocation of the storage lua modules\n");
		printf("\t -Ll --lua-root=xxxx\t\tlocation of the plain lua modules\n");
		goto clear;
	}

	if(options_exists(options, "V") == 0) {
		printf("%s v%s\n", progname, PILIGHT_VERSION);
		goto clear;
	}

	if(options_exists(options, "C") == 0) {
		options_get_string(options, "C", &configtmp);
	}

	if(options_exists(options, "Ls") == 0) {
		char *arg = NULL;
		options_get_string(options, "Ls", &arg);
		if(config_root(arg) == -1) {
			logprintf(LOG_ERR, "%s is not valid storage lua modules path", arg);
			goto clear;
		}
	}

	if(options_exists(options, "Ll") == 0) {
		options_get_string(options, "Ll", &lua_root);
	}

	{
		int len = strlen(lua_root)+strlen("lua/?/?.lua")+1;
		char *lua_path = MALLOC(len);

		if(lua_path == NULL) {
			OUT_OF_MEMORY
		}

		plua_init();

		memset(lua_path, '\0', len);
		snprintf(lua_path, len, "%s/?/?.lua", lua_root);
		plua_package_path(lua_path);

		memset(lua_path, '\0', len);
		snprintf(lua_path, len, "%s/?.lua", lua_root);
		plua_package_path(lua_path);

		FREE(lua_path);
	}

	int *ret = NULL, n = 0;
	if((n = isrunning("pilight-debug", &ret)) > 1) {
		int i = 0;
		for(i=0;i<n;i++) {
			if(ret[i] != getpid()) {
				logprintf(LOG_NOTICE, "pilight-debug is already running (%d)", ret[i]);
				break;
			}
		}
		FREE(ret);
		goto clear;
	}
	if(ret != NULL) {
		FREE(ret);
	}

	if((n = isrunning("pilight-daemon", &ret)) > 0) {
		logprintf(LOG_NOTICE, "pilight-daemon instance found (%d)", ret[0]);
		FREE(ret);
		goto clear;
	}
	if(ret != NULL) {
		FREE(ret);
	}

	if((n = isrunning("pilight-raw", &ret)) > 0) {
		logprintf(LOG_NOTICE, "pilight-raw instance found (%d)", ret[0]);
		FREE(ret);
		goto clear;
	}
	if(ret != NULL) {
		FREE(ret);
	}

	if(config_set_file(configtmp) == EXIT_FAILURE) {
		goto clear;
	}

	eventpool_init(EVENTPOOL_NO_THREADS);
	eventpool_callback(REASON_RECEIVED_PULSETRAIN+10000, received_pulsetrain, NULL);
	eventpool_callback(REASON_RECEIVED_OOK+10000, received_ook, NULL);

	protocol_init();
	hardware_init();
	config_init();

	struct lua_state_t *state = plua_get_free_state();
	if(config_read(state->L, CONFIG_SETTINGS | CONFIG_HARDWARE) != EXIT_SUCCESS) {
		assert(plua_check_stack(state->L, 0) == 0);
		plua_clear_state(state);
		goto clear;
	}
	assert(plua_check_stack(state->L, 0) == 0);
	plua_clear_state(state);

	struct plua_metatable_t *table = config_get_metatable();
	plua_metatable_set_number(table, "registry.hardware.RF433.mingaplen", 5100);
	plua_metatable_set_number(table, "registry.hardware.RF433.maxgaplen", 34000);
	plua_metatable_set_number(table, "registry.hardware.RF433.minrawlen", 25);
	plua_metatable_set_number(table, "registry.hardware.RF433.maxrawlen", MAXPULSESTREAMLENGTH);

	if(config_hardware_run() == -1) {
		logprintf(LOG_NOTICE, "there are no hardware modules configured");
		uv_stop(uv_default_loop());
		goto clear;
	}

	printf("Press and hold one of the buttons on your remote or wait until\n");
	printf("another device such as a weather station has sent new codes\n");
	printf("The debugger will automatically reset itself after one second of\n");
	printf("failed leads. It will keep running until you explicitly stop it.\n");
	printf("This is done by pressing both the [CTRL] and C buttons on your keyboard.\n");

	options_delete(options);

	main_loop1(0);
	main_gc();

	return (EXIT_SUCCESS);
clear:
	options_delete(options);
	main_loop1(1);
	main_gc();

	return (EXIT_FAILURE);
}
