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
#ifndef _WIN32
#include <wiringx.h>
#endif
#include <assert.h>

#include "libs/pilight/core/threads.h"
#include "libs/pilight/core/pilight.h"
#include "libs/pilight/core/network.h"
#include "libs/pilight/core/options.h"
#include "libs/pilight/core/log.h"
#include "libs/pilight/core/datetime.h"
#include "libs/pilight/core/ssdp.h"
#include "libs/pilight/core/socket.h"
#include "libs/pilight/core/threads.h"
#include "libs/pilight/core/dso.h"
#include "libs/pilight/core/gc.h"
#include "libs/pilight/config/config.h"
#include "libs/pilight/config/hardware.h"
#include "libs/pilight/lua_c/lua.h"
#include "libs/pilight/lua_c/table.h"

#include "libs/pilight/protocols/protocol.h"

#include "libs/pilight/events/events.h"

static uv_signal_t *signal_req = NULL;

static unsigned short main_loop = 1;
static unsigned short linefeed = 0;

static char *lua_root = LUA_ROOT;

int main_gc(void) {
	log_shell_disable();
	main_loop = 0;

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

#ifdef _WIN32
	WSACleanup();
#endif

	return EXIT_SUCCESS;
}

static int iter = 0;

static void *listener(int reason, void *param, void *userdata) {
	struct plua_metatable_t *table = param;
	char nr[255], *p = nr;
	char *hardware = NULL;
	double length = 0.0;
	double pulse = 0.0;
	int buffer[WIRINGX_BUFFER];
	memset(&buffer, 0, WIRINGX_BUFFER*sizeof(int));

	memset(&nr, 0, 255);

	int i = 0;
	plua_metatable_get_number(table, "length", &length);
	plua_metatable_get_string(table, "hardware", &hardware);

	for(i=0;i<length;i++) {
		snprintf(p, 254, "pulses.%d", i+1);
		plua_metatable_get_number(table, nr, &pulse);
		buffer[i] = (int)pulse;
	}

	if((int)length > 0) {
		for(i=0;i<(int)length;i++) {
			if(linefeed == 1) {
				printf(" %d", buffer[i]);
				iter++;
				if(buffer[i] > 5100) {
					printf(" -# %d\n %s:", iter, hardware);
					iter = 0;
				}
			} else {
				printf("%s: %d\n", hardware, buffer[i]);
			}
		}
	}
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

	// memtrack();

	atomicinit();
	struct options_t *options = NULL;
	char *args = NULL;
	char *configtmp = CONFIG_FILE;
	int help = 0;

	gc_attach(main_gc);

	/* Catch all exit signals for gc */
	gc_catch();

	if((progname = MALLOC(12)) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
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

	log_shell_enable();
	log_file_disable();
	log_level_set(LOG_NOTICE);

	options_add(&options, "H", "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "V", "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "C", "config", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "L", "linefeed", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "Ls", "storage-root", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "Ll", "lua-root", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);

	if(options_parse(options, argc, argv, 1) == -1) {
		help = 1;
	}

	if(options_exists(options, "H") == 0 || help == 1) {
		printf("Usage: %s [options]\n", progname);
		printf("\t -H  --help\t\t\tdisplay usage summary\n");
		printf("\t -V  --version\t\t\tdisplay version\n");
		printf("\t -L  --linefeed\t\t\tstructure raw printout\n");
		printf("\t -C  --config\t\t\tconfig file\n");
		printf("\t -Ls --storage-root=xxxx\tlocation of the storage lua modules\n");
		printf("\t -Ll --lua-root=xxxx\t\tlocation of the plain lua modules\n");
		goto close;
	}

	if(options_exists(options, "L") == 0) {
		linefeed = 1;
	}

	if(options_exists(options, "V") == 0) {
		printf("%s v%s\n", progname, PILIGHT_VERSION);
		goto close;
	}

	if(options_exists(options, "C") == 0) {
		options_get_string(options, "C", &configtmp);
	}

	if(options_exists(options, "Ls") == 0) {
		char *arg = NULL;
		options_get_string(options, "Ls", &arg);
		if(config_root(arg) == -1) {
			logprintf(LOG_ERR, "%s is not valid storage lua modules path", arg);
			goto close;
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
	if(ret != NULL) {
		FREE(ret);
	}

	if((n = isrunning("pilight-daemon", &ret)) > 0) {
		logprintf(LOG_NOTICE, "pilight-daemon instance found (%d)", ret[0]);
		FREE(ret);
		goto close;
	}
	if(ret != NULL) {
		FREE(ret);
	}

	if((n = isrunning("pilight-debug", &ret)) > 0) {
		logprintf(LOG_NOTICE, "pilight-debug instance found (%d)", ret[0]);
		FREE(ret);
		goto close;
	}
	if(ret != NULL) {
		FREE(ret);
	}

	if(config_set_file(configtmp) == EXIT_FAILURE) {
		goto close;
	}

	eventpool_init(EVENTPOOL_THREADED);
	eventpool_callback(REASON_RECEIVED_PULSETRAIN+10000, listener, NULL);
	eventpool_callback(REASON_RECEIVED_OOK+10000, listener, NULL);

	plua_init();
	protocol_init();
	hardware_init();
	config_init();

	struct lua_state_t *state = plua_get_free_state();
	if(config_read(state->L, CONFIG_SETTINGS | CONFIG_HARDWARE) != EXIT_SUCCESS) {
		assert(plua_check_stack(state->L, 0) == 0);
		plua_clear_state(state);
		goto close;
	}
	assert(plua_check_stack(state->L, 0) == 0);
	plua_clear_state(state);

	struct plua_metatable_t *table = config_get_metatable();
	plua_metatable_set_number(table, "registry.hardware.RF433.mingaplen", 0);
	plua_metatable_set_number(table, "registry.hardware.RF433.maxgaplen", 99999);
	plua_metatable_set_number(table, "registry.hardware.RF433.minrawlen", 0);
	plua_metatable_set_number(table, "registry.hardware.RF433.maxrawlen", WIRINGX_BUFFER);

	if(config_hardware_run() == -1) {
		logprintf(LOG_NOTICE, "there are no hardware modules configured");
		uv_stop(uv_default_loop());
	}

	main_loop1(0);

	options_delete(options);
	if(args != NULL) {
		FREE(args);
	}
	main_gc();
	return EXIT_SUCCESS;

close:
	options_delete(options);
	if(args != NULL) {
		FREE(args);
	}

	plua_gc();
	protocol_gc();
	main_loop1(1);
	main_gc();

	return (EXIT_FAILURE);
}
