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
#include <sys/ioctl.h>
#include <limits.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>
#include <math.h>
#include <string.h>

#include "pilight.h"
#include "common.h"
#include "settings.h"
#include "hardware.h"
#include "log.h"
#include "options.h"
#include "threads.h"
#include "wiringX.h"
#include "irq.h"
#include "gc.h"
#include "dso.h"

static int pulse_length = 0;
static unsigned short main_loop = 1;
static pthread_t pth;

typedef enum {
	WAIT,
	CAPTURE,
	ON,
	OFF,
	ALL,
	UNIT1,
	UNIT2,
	PROCESSUNIT,
	STOP
} steps_t;

static void rmDup(int *a, int *b) {
	int x=0, y=0, i=0;
	int temp[75];
	int match = 0;

	/* Remove all ALL bits that are also stores as the ON/OFF bits */
	memset(temp,-1,75);
	for(i=0;i<75;i++) {
		match=0;
		if(a[i] == -1)
			break;
		for(y=0;y<75;y++) {
			if(b[y] == -1)
				break;
			if(a[i] == b[y])
				match=1;
		}
		if(match == 0)
			temp[x++]=a[i];
	}
	memset(a,-1,75);
	for(i=0;i<x;i++) {
		a[i]=temp[i];
	}
}

static int normalize(int i) {
	double x;
	x=(double)i/pulse_length;

	return (int)(round(x));
}

int main_gc(void) {
	printf("\n");
	log_shell_disable();

	main_loop = 0;

	struct conf_hardware_t *tmp_confhw = conf_hardware;
	while(tmp_confhw) {
		if(tmp_confhw->hardware->deinit) {
			tmp_confhw->hardware->deinit();
		}
		tmp_confhw = tmp_confhw->next;
	}

	threads_gc();
	pthread_join(pth, NULL);

	options_gc();
	settings_gc();
	hardware_gc();
	dso_gc();
	wiringXGC();
	log_gc();

	sfree((void *)&progname);

	return EXIT_SUCCESS;
}

void *receive_code(void *param) {
	int duration = 0;
	int i = 0;
	int y = 0;
	int z = 0;

	steps_t state = CAPTURE;
	steps_t pState = WAIT;

	int recording = 1;
	int bit = 0;
	int raw[255] = {0};
	int pRaw[255] = {0};
	int code[255] = {0};
	int onCode[255] = {0};
	int offCode[255] = {0};
	int allCode[255] = {0};
	int unit1Code[255] = {0};
	int unit2Code[255] = {0};
	int unit3Code[255] = {0};
	int binary[255] = {0};
	int pBinary[255] = {0};
	int onBinary[255] = {0};
	int offBinary[255] = {0};
	int allBinary[255] = {0};
	int unit1Binary[255] = {0};
	int unit2Binary[255] = {0};
	int unit3Binary[255] = {0};

	int temp[75] = {-1};
	int footer = 0;
	int pulse = 0;
	int onoff[75] = {-1};
	int all[75] = {-1};
	int unit[75] = {-1};
	int rawLength = 0;
	int binaryLength = 0;

	memset(onoff, -1, 75);
	memset(unit, -1, 75);
	memset(all, -1, 75);
	memset(temp, -1, 75);

	struct hardware_t *hw = (hardware_t *)param;
	while(main_loop && hw->receive) {
		switch(state) {
			case CAPTURE:
				printf("1. Please send and hold one of the OFF buttons.");
				break;
			case ON:
				/* Store the previous OFF button code */
				for(i=0;i<binaryLength;i++) {
					offBinary[i] = binary[i];
				}
				printf("2. Please send and hold the ON button for the same device\n");
				printf("   as for which you send the previous OFF button.");
				break;
			case OFF:
				/* Store the previous ON button code */
				for(i=0;i<binaryLength;i++) {
					onBinary[i] = binary[i];
				}
				for(i=0;i<rawLength;i++) {
					onCode[i] = code[i];
				}

				z=0;
				/* Compare the ON and OFF codes and save bit that are different */
				for(i=0;i<binaryLength;i++) {
					if(offBinary[i] != onBinary[i]) {
						onoff[z++]=i;
					}
				}
				for(i=0;i<rawLength;i++) {
					offCode[i] = code[i];
				}
				printf("3. Please send and hold (one of the) ALL buttons.\n");
				printf("   If you're remote doesn't support turning ON or OFF\n");
				printf("   all devices at once, press the same OFF button as in\n");
				printf("   the beginning.");
			break;
			case ALL:
				z=0;
				memset(temp,-1,75);
				/* Store the ALL code */
				for(i=0;i<binaryLength;i++) {
					allBinary[i] = binary[i];
					if(allBinary[i] != onBinary[i]) {
						temp[z++] = i;
					}
				}
				for(i=0;i<rawLength;i++) {
					allCode[i] = code[i];
				}
				/* Compare the ALL code to the ON and OFF code and store the differences */
				y=0;
				for(i=0;i<binaryLength;i++) {
					if(allBinary[i] != offBinary[i]) {
						all[y++] = i;
					}
				}
				if((unsigned int)z < y) {
					for(i=0;i<z;i++) {
						all[z]=temp[z];
					}
				}
				printf("4. Please send and hold the ON button with the lowest ID.");
			break;
			case UNIT1:
				/* Store the lowest unit code */
				for(i=0;i<binaryLength;i++) {
					unit1Binary[i] = binary[i];
				}
				for(i=0;i<rawLength;i++) {
					unit1Code[i] = code[i];
				}
				printf("5. Please send and hold the ON button with the second to lowest ID.");
			break;
			case UNIT2:
				/* Store the second to lowest unit code */
				for(i=0;i<binaryLength;i++) {
					unit2Binary[i] = binary[i];
				}
				for(i=0;i<rawLength;i++) {
					unit2Code[i] = code[i];
				}
				printf("6. Please send and hold the ON button with the highest ID.");
			break;
			case PROCESSUNIT:
				z=0;
				/* Store the highest unit code and compare the three codes. Store all
				   bit that are different */
				for(i=0;i<binaryLength;i++) {
					unit3Binary[i] = binary[i];
					if((unit2Binary[i] != unit1Binary[i]) || (unit1Binary[i] != unit3Binary[i]) || (unit2Binary[i] != unit3Binary[i])) {
						unit[z++]=i;
					}
				}
				for(i=0;i<rawLength;i++) {
					unit3Code[i] = code[i];
				}
				state=STOP;
			break;
			case WAIT:
			case STOP:
			default:;
		}
		fflush(stdout);
		if(state!=WAIT)
			pState=state;
		if(state==STOP)
			main_loop = 0;
		else
			state=WAIT;

		duration = hw->receive();

		/* If we are recording, keep recording until the next footer has been matched */
		if(recording == 1) {
			if(bit < 255) {
				raw[bit++] = duration;
			} else {
				bit = 0;
				recording = 0;
			}
		}

		/* First try to catch code that seems to be a footer.
		   If a real footer has been recognized, start using that as the new footer */
		if((duration > 5100 && footer == 0) || ((footer-(footer*0.1)<duration) && (footer+(footer*0.1)>duration))) {
			recording = 1;
			pulse_length = duration/PULSE_DIV;

			/* Check if we are recording similar codes */
			for(i=0;i<(bit-1);i++) {
				if(!(((pRaw[i]-(pRaw[i]*0.3)) < raw[i]) && ((pRaw[i]+(pRaw[i]*0.3)) > raw[i]))) {
					y=0;
					z=0;
					recording=0;
				}
				pRaw[i]=raw[i];
			}
			y++;
			/* Continue if we have 2 matches */
			if(y>2) {
				/* If we are certain we are recording similar codes.
				   Save the raw code length */
				if(footer>0) {
					if(rawLength == 0)
						rawLength=bit;
				}
				if(rawLength == 0 || rawLength == bit) {

					/* Try to catch the footer, and the low and high values */
					for(i=0;i<bit;i++) {
						if((i+1)<bit && i > 2 && footer > 0) {
							if((raw[i]/pulse_length) >= 2) {
								pulse=raw[i];
							}
						}
						if(duration > 5000 && duration < 100000)
							footer=raw[i];
					}
					/* If we have gathered all data, stop with the loop */
					if(footer > 0 && pulse > 0 && rawLength > 0) {
						/* Convert the raw code into binary code */
						for(i=0;i<rawLength;i++) {
							if((unsigned int)raw[i] > (pulse-pulse_length)) {
								code[i]=1;
							} else {
								code[i]=0;
							}
						}
						for(i=2;i<rawLength; i+=4) {
							if(code[i+1] == 1) {
								binary[i/4]=1;
							} else {
								binary[i/4]=0;
							}
						}
						if(binaryLength == 0)
							binaryLength = (int)((float)i/4);

						/* Check if the subsequent binary code matches
						   to check if the same button was still held */
						if(binaryLength == (i/4)) {
							for(i=0;i<binaryLength;i++) {
								if(pBinary[i] != binary[i]) {
									z=1;
								}
							}

							/* If we are capturing a different button
							   continue to the next step */
							if(z==1 || state == CAPTURE) {
								switch(pState) {
									case CAPTURE:
										state=ON;
									break;
									case ON:
										state=OFF;
									break;
									case OFF:
										state=ALL;
									break;
									case ALL:
										state=UNIT1;
									break;
									case UNIT1:
										state=UNIT2;
									break;
									case UNIT2:
										state=PROCESSUNIT;
									break;
									case PROCESSUNIT:
										state=STOP;
									break;
									case WAIT:
									case STOP:
									default:;
								}
							printf(" Done.\n\n");
							pState=WAIT;
							}
						}
					}
				}
			}
			bit=0;
		}

		/* Reset the button repeat counter */
		if(z==1) {
			z=0;
			for(i=0;i<binaryLength;i++) {
				pBinary[i]=binary[i];
			}
		}
	}

	rmDup(all, onoff);
	rmDup(unit, onoff);
	rmDup(all, unit);

	/* Print everything */
	printf("--[RESULTS]--\n");
	printf("\n");
	printf("hardware:\t%s\n", hw->id);
	printf("pulse:\t\t%d\n", normalize(pulse));
	printf("rawlen:\t\t%d\n", rawLength);
	printf("binlen:\t\t%d\n", binaryLength);
	printf("plslen:\t\t%d\n", pulse_length);
	printf("\n");
	printf("on-off bit(s):\t");
	z=0;
	while(onoff[z] > -1) {
		printf("%d ",onoff[z++]);
	}
	printf("\n");
	printf("all bit(s):\t");
	z=0;
	while(all[z] > -1) {
		printf("%d ",all[z++]);
	}
	printf("\n");
	printf("unit bit(s):\t");
	z=0;
	while(unit[z] > -1) {
		printf("%d ",unit[z++]);
	}
	printf("\n\n");
	printf("Raw code:\n");
	for(i=0;i<rawLength;i++) {
		printf("%d ",normalize(raw[i])*pulse_length);
	}
	printf("\n");
	printf("Raw simplified:\n");
	printf("On:\t");
	for(i=0;i<rawLength;i++) {
		printf("%d",onCode[i]);
	}
	printf("\n");
	printf("Off:\t");
	for(i=0;i<rawLength;i++) {
		printf("%d",offCode[i]);
	}
	printf("\n");
	printf("All:\t");
	for(i=0;i<rawLength;i++) {
		printf("%d",allCode[i]);
	}
	printf("\n");
	printf("Unit 1:\t");
	for(i=0;i<rawLength;i++) {
		printf("%d",unit1Code[i]);
	}
	printf("\n");
	printf("Unit 2:\t");
	for(i=0;i<rawLength;i++) {
		printf("%d",unit2Code[i]);
	}
	printf("\n");
	printf("Unit 3:\t");
	for(i=0;i<rawLength;i++) {
		printf("%d",unit3Code[i]);
	}
	printf("\n");
	printf("Binary code:\n");
	printf("On:\t");
	for(i=0;i<binaryLength;i++) {
		printf("%d",onBinary[i]);
	}
	printf("\n");
	printf("Off:\t");
	for(i=0;i<binaryLength;i++) {
		printf("%d",offBinary[i]);
	}
	printf("\n");
	printf("All:\t");
	for(i=0;i<binaryLength;i++) {
		printf("%d",allBinary[i]);
	}
	printf("\n");
	printf("Unit 1:\t");
	for(i=0;i<binaryLength;i++) {
		printf("%d",unit1Binary[i]);
	}
	printf("\n");
	printf("Unit 2:\t");
	for(i=0;i<binaryLength;i++) {
		printf("%d",unit2Binary[i]);
	}
	printf("\n");
	printf("Unit 3:\t");
	for(i=0;i<binaryLength;i++) {
		printf("%d",unit3Binary[i]);
	}
	main_loop = 0;
	return NULL;
}

int main(int argc, char **argv) {

	gc_attach(main_gc);

	/* Catch all exit signals for gc */
	gc_catch();

	log_shell_enable();
	log_level_set(LOG_NOTICE);

	struct options_t *options = NULL;

	char *args = NULL;
	char *hwfile = NULL;
	pid_t pid = 0;
	int x = 0;

	char settingstmp[] = SETTINGS_FILE;
	settings_set_file(settingstmp);

	progname = malloc(16);
	if(!progname) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(progname, "pilight-learn");

	options_add(&options, 'H', "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'V', "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, 'F', "settings", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);

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
				printf("\t -F --settings\t\tsettings file\n");
				goto clear;
			break;
			case 'V':
				printf("%s %s\n", progname, VERSION);
				goto clear;
			break;
			case 'F':
				if(settings_set_file(args) == EXIT_FAILURE) {
					goto clear;
				}
			break;
			default:
				printf("Usage: %s [options]\n", progname);
				goto clear;
			break;
		}
	}
	options_delete(options);

	char pilight_daemon[] = "pilight-daemon";
	char pilight_debug[] = "pilight-debug";
	char pilight_raw[] = "pilight-raw";
	if((pid = findproc(pilight_daemon, NULL, 1)) > 0) {
		logprintf(LOG_ERR, "pilight-daemon instance found (%d)", (int)pid);
		goto clear;
	}

	if((pid = findproc(pilight_raw, NULL, 1)) > 0) {
		logprintf(LOG_ERR, "pilight-raw instance found (%d)", (int)pid);
		goto clear;
	}

	if((pid = findproc(pilight_debug, NULL, 1)) > 0) {
		logprintf(LOG_ERR, "pilight-debug instance found (%d)", (int)pid);
		goto clear;
	}

	if(settings_read() != 0) {
		goto clear;
	}

	hardware_init();

	if(settings_find_string("hardware-file", &hwfile) == 0) {
		hardware_set_file(hwfile);
		if(hardware_read() == EXIT_FAILURE) {
			goto clear;
		}
	}

	/* Start threads library that keeps track of all threads used */
	threads_create(&pth, NULL, &threads_start, (void *)NULL);

	struct conf_hardware_t *tmp_confhw = conf_hardware;
	while(tmp_confhw) {
		if(tmp_confhw->hardware->init() == EXIT_FAILURE) {
			logprintf(LOG_ERR, "could not initialize %s hardware mode", tmp_confhw->hardware->id);
			goto clear;
		}
		if(x == 0) {
			threads_register(tmp_confhw->hardware->id, &receive_code, (void *)tmp_confhw->hardware, 0);
		}
		tmp_confhw = tmp_confhw->next;
		x++;
	}
	if(x > 1) {
		printf("pilight-learn can only use one hardware module at one time\n\n");
	}

	while(main_loop) {
		sleep(1);
	}

clear:
	if(main_loop) {
		main_gc();
	}
	return (EXIT_FAILURE);
}
