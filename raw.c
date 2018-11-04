/*
	Copyright (C) 2013 - 2014 CurlyMo

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

#include "libs/pilight/protocols/protocol.h"

#include "libs/pilight/events/events.h"


#ifndef PILIGHT_DEVELOPMENT
static uv_signal_t *signal_req = NULL;
#endif

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

#ifndef PILIGHT_DEVELOPMENT
	eventpool_gc();
#endif
	config_gc();
	protocol_gc();
	whitelist_free();
	threads_gc();

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

void *receiveOOK(void *param) {
	int duration = 0, iLoop = 0;
	long stream_duration = 0;

	struct hardware_t *hw = (hardware_t *)param;
	while(main_loop && hw->receiveOOK) {
		duration = hw->receiveOOK();
		stream_duration += duration;
		iLoop++;
		if(duration > 0) {
			if(linefeed == 1) {
				if(duration > 5100) {
					printf(" %d -#: %d -d: %ld\n%s: ",duration, iLoop, stream_duration, hw->id);
					iLoop = 0;
				} else {
					printf(" %d", duration);
				}
			} else {
				printf("%s: %d\n", hw->id, duration);
			}
		}
	};
	return NULL;
}

static void *receivePulseTrain(int reason, void *param) {
	struct reason_received_pulsetrain_t *data = param;
	int i = 0;

	if(data->hardware != NULL && data->pulses != NULL && data->length > 0) {
#ifndef PILIGHT_REWRITE
		char *id = NULL;
		struct conf_hardware_t *tmp_confhw = conf_hardware;
		while(tmp_confhw) {
			if(strcmp(tmp_confhw->hardware->id, data->hardware) == 0) {
				id = tmp_confhw->hardware->id;
			}
			tmp_confhw = tmp_confhw->next;
		}
#endif
#ifdef PILIGHT_REWRITE
		struct hardware_t *hw = NULL;
		if(hardware_select_struct(ORIGIN_MASTER, data->hardware, &hw) == 0) {
#endif
			if(data->length > 0) {
				for(i=0;i<data->length;i++) {
					if(linefeed == 1) {
						printf(" %d", data->pulses[i]);
						if(data->pulses[i] > 5100) {
							printf(" -# %d\n %s:", i, id);
						}
					} else {
						printf("%s: %d\n", id, data->pulses[i]);
					}
				}
			}
#ifdef PILIGHT_REWRITE
		}
#endif
	}

	return (void *)NULL;
}

#ifndef PILIGHT_DEVELOPMENT
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
#endif

int main(int argc, char **argv) {
#ifndef PILIGHT_DEVELOPMENT
	const uv_thread_t pth_cur_id = uv_thread_self();
	memcpy((void *)&pth_main_id, &pth_cur_id, sizeof(uv_thread_t));
#endif

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

#ifndef PILIGHT_DEVELOPMENT
	if((signal_req = MALLOC(sizeof(uv_signal_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_signal_init(uv_default_loop(), signal_req);
	uv_signal_start(signal_req, signal_cb, SIGINT);
#endif

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

	if(options_parse(options, argc, argv) == -1) {
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

	if(config_set_file(configtmp) == EXIT_FAILURE) {
		goto close;
	}

#ifndef PILIGHT_DEVELOPMENT
	eventpool_init(EVENTPOOL_THREADED);
#endif
	protocol_init();
	config_init();
	if(config_read(CONFIG_SETTINGS | CONFIG_HARDWARE) != EXIT_SUCCESS) {
		goto close;
	}

#ifndef PILIGHT_DEVELOPMENT
	eventpool_callback(REASON_RECEIVED_PULSETRAIN, receivePulseTrain);
#endif

	/* Start threads library that keeps track of all threads used */
	threads_start();

	int has_hardware = 0;
	struct conf_hardware_t *tmp_confhw = conf_hardware;
	while(tmp_confhw) {
		if(tmp_confhw->hardware->init) {
			if(tmp_confhw->hardware->comtype == COMOOK) {
				tmp_confhw->hardware->maxrawlen = 1024;
				tmp_confhw->hardware->minrawlen = 0;
				tmp_confhw->hardware->maxgaplen = 99999;
				tmp_confhw->hardware->mingaplen = 0;
			}
			if(tmp_confhw->hardware->init() == EXIT_FAILURE) {
				logprintf(LOG_ERR, "could not initialize %s hardware mode", tmp_confhw->hardware->id);
				goto close;
			} else {
				has_hardware = 1;
			}
		}
		tmp_confhw = tmp_confhw->next;
	}

	if(has_hardware == 0) {
		logprintf(LOG_NOTICE, "there are no hardware modules configured");
		uv_stop(uv_default_loop());
	}

#ifdef PILIGHT_DEVELOPMENT
	while(main_loop) {
		sleep(1);
	}
#else
	main_loop1(0);
#endif

close:
	options_delete(options);
	if(args != NULL) {
		FREE(args);
	}
#ifdef PILIGHT_DEVELOPMENT
	if(main_loop == 1) {
		main_gc();
	}
#else
	main_loop1(1);
	main_gc();
#endif
	return (EXIT_FAILURE);
}
