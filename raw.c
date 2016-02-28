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

#include "libs/pilight/core/threads.h"
#include "libs/pilight/core/pilight.h"
#include "libs/pilight/core/network.h"
#include "libs/pilight/core/config.h"
#include "libs/pilight/core/log.h"
#include "libs/pilight/core/datetime.h"
#include "libs/pilight/core/ssdp.h"
#include "libs/pilight/core/socket.h"
#include "libs/pilight/core/threads.h"
#include "libs/pilight/core/irq.h"
#include "libs/pilight/core/dso.h"
#include "libs/pilight/core/gc.h"

#include "libs/pilight/protocols/protocol.h"

#include "libs/pilight/events/events.h"

#include "libs/pilight/config/hardware.h"

#ifndef _WIN32
	#include "libs/wiringx/wiringX.h"
#endif

static unsigned short main_loop = 1;
static unsigned short linefeed = 0;
static int min_pulses = 0;
static int pulses_per_line = 0;

static const char pulse_fmt[] = " %5d";
static const char line_fmt[] = "\n%*d:";

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
	int duration = 0, iLoop = 0, len = 0;
	size_t lines = 0;

	int pulses[min_pulses < 1 ? 1 : min_pulses];

	struct hardware_t *hw = (hardware_t *)param;
	while(main_loop && hw->receiveOOK) {
		duration = hw->receiveOOK();
		if(duration > 0) {
			iLoop++;
			if(min_pulses > 0) {
				if(iLoop < min_pulses) {
					pulses[iLoop] = duration;
				} else if(iLoop == min_pulses) {
					len = printf("%6u %s:", ++lines, hw->id) - 1; // don't count the ':' - is added by line_fmt.
					for(iLoop = 1; iLoop < min_pulses; iLoop++) {
						if(iLoop > 1 && (iLoop-1)%pulses_per_line == 0) {
							printf(line_fmt, len, (iLoop-1));
						}
						printf(pulse_fmt, pulses[iLoop]);
					}
					if(iLoop > 1 && (iLoop-1)%pulses_per_line == 0) {
						printf(line_fmt, len, (iLoop-1));
					}
					printf(pulse_fmt, duration);
				} else {
					if((iLoop-1)%pulses_per_line == 0) {
						printf(line_fmt, len, (iLoop-1));
					}
					printf(pulse_fmt, duration);
				}

				if(duration > 5100) {
					if(iLoop >= min_pulses) {
						printf(line_fmt, len, iLoop);
						printf(" -#: %d\n", iLoop);
						if(iLoop >= pulses_per_line) {
							printf("\n");	// space line after large reports.
						}
					}
					iLoop = 0;
				}
			}
			else if(linefeed == 1) {
				if(duration > 5100) {
					printf(" %d -#: %d\n%s: ",duration, iLoop, hw->id);
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

void *receivePulseTrain(void *param) {
	struct rawcode_t r;
	size_t lines = 0;
	int i = 0, len = 0;

	struct hardware_t *hw = (hardware_t *)param;
	while(main_loop && hw->receivePulseTrain) {
		hw->receivePulseTrain(&r);
		if(r.length == -1) {
			main_gc();
			break;
		} else if(r.length > 0) {
			if(min_pulses > 0) {
				int have_nl = 0;
				if(r.length >= min_pulses) {
					len = printf("%6u %s:", ++lines, hw->id) - 1; // don't count the ':' - is added by line_fmt.
					for(i = 0; i < r.length; i++) {
						if((i > 0 && i%pulses_per_line == 0) || have_nl) {
							printf(line_fmt, len, i);
						}
						printf(pulse_fmt, r.pulses[i]);
						have_nl = 0;
						if(r.pulses[i] > 5100) {
							printf(" -#: %d\n", i+1);
							have_nl = 1;
						}
					}
				}
				if(r.length >= pulses_per_line || !have_nl) {
					printf("\n");
				}
				continue;
			}
			for(i=0;i<r.length;i++) {
				if(linefeed == 1) {
					printf(" %d", r.pulses[i]);
					if(r.pulses[i] > 5100) {
						printf(" -# %d\n %s:", i+1, hw->id);
					}
				} else {
					printf("%s: %d\n", hw->id, r.pulses[i]);
				}
			}
		}
	};
	return NULL;
}

int main(int argc, char **argv) {
	// memtrack();

	atomicinit();
	struct options_t *options = NULL;
	char *args = NULL;
	char *configtmp = MALLOC(strlen(CONFIG_FILE)+1);
	pid_t pid = 0;

	strcpy(configtmp, CONFIG_FILE);

	gc_attach(main_gc);

	/* Catch all exit signals for gc */
	gc_catch();

	if((progname = MALLOC(12)) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
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

#ifndef _WIN32
	wiringXLog = logprintf;
#endif

	options_add(&options, 'H', "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'V', "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'C', "config", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'L', "linefeed", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'm', "minpulses", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'p', "pulsesperpline", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);

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
				printf("\t -m --minpulses count\t  Print nothing if not at least count pulses. Implies --linefeed.\n");
				printf("\t -p --pulsesperline count Pulses to print per line (10 by default). Implies --linefeed.\n");
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
				configtmp = REALLOC(configtmp, strlen(args)+1);
				strcpy(configtmp, args);
			break;
			case 'm':
				min_pulses = atoi(args);
				if(min_pulses < 1) {
					printf("%s: --minpulses must be 1 or more.\n", progname);
					goto close;
				}
				break;
			case 'p':
				pulses_per_line = atoi(args);
				if(pulses_per_line < 1) {
					printf("%s: --pulsesperline must be 1 or more.\n", progname);
					goto close;
				}
				break;
			default:
				printf("Usage: %s [options]\n", progname);
				goto close;
			break;
		}
	}
	options_delete(options);

	if(pulses_per_line > 0 || min_pulses > 0) {
		linefeed = 1;
	}
	if(linefeed == 1) {
		if(pulses_per_line > 0) {
			if(min_pulses < 1) {
				min_pulses = 1;
			}
		} else if(min_pulses > 0) {
			pulses_per_line = 10;
		}
	}

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

	if(config_set_file(configtmp) == EXIT_FAILURE) {
		FREE(configtmp);
		return EXIT_FAILURE;
	}

	protocol_init();
	config_init();
	if(config_read() != EXIT_SUCCESS) {
		FREE(configtmp);
		goto close;
	}
	FREE(configtmp);

	/* Start threads library that keeps track of all threads used */
	threads_start();

	struct conf_hardware_t *tmp_confhw = conf_hardware;
	while(tmp_confhw) {
		if(tmp_confhw->hardware->init) {
			if(tmp_confhw->hardware->init() == EXIT_FAILURE) {
				logprintf(LOG_ERR, "could not initialize %s hardware mode", tmp_confhw->hardware->id);
				goto close;
			}
			if(tmp_confhw->hardware->comtype == COMOOK) {
				threads_register(tmp_confhw->hardware->id, &receiveOOK, (void *)tmp_confhw->hardware, 0);
			} else if(tmp_confhw->hardware->comtype == COMPLSTRAIN) {
				threads_register(tmp_confhw->hardware->id, &receivePulseTrain, (void *)tmp_confhw->hardware, 0);
			}
		}
		tmp_confhw = tmp_confhw->next;
	}

	while(main_loop) {
		sleep(1);
	}

close:
	if(args != NULL) {
		FREE(args);
	}
	if(main_loop == 1) {
		main_gc();
	}
	return (EXIT_FAILURE);
}
