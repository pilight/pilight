/*
	Copyright (C) 2013 CurlyMo

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

#include "settings.h"
#include "log.h"
#include "options.h"
#include "wiringPi.h"
#include "libs/lirc/lirc.h"
#include "libs/lirc/hardware.h"
#include "irq.h"

struct hardware hw_default;

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

void rmDup(int *a, int *b) {
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

int normalize(int i) {
	double x;
	x=(double)i/PULSE_LENGTH;

	return (int)(round(x));
}

int main(int argc, char **argv) {

	log_shell_enable();
	log_shell_disable();
	log_level_set(LOG_NOTICE);

	progname = malloc(16);
	strcpy(progname, "pilight-learn");
	struct options_t *options = NULL;
	
	lirc_t data;
	char *socket = malloc(11);
	strcpy(socket, "/dev/lirc0");
	char *args;
	int have_device = 0;
	char *hw_mode = malloc(strlen(HW_MODE)+1);
	int gpio_in = GPIO_IN_PIN;
	
	strcpy(hw_mode, HW_MODE);
	
	int duration = 0;
	int i = 0;
	int y = 0;
	int z = 0;

	steps_t state = CAPTURE;
	steps_t pState = WAIT;

	int recording = 1;
	int bit = 0;
	int raw[255];
	int pRaw[255];
	int code[255];
	int onCode[255];
	int offCode[255];
	int allCode[255];
	int unit1Code[255];
	int unit2Code[255];
	int unit3Code[255];
	int binary[255];
	int pBinary[255];
	int onBinary[255];
	int offBinary[255];
	int allBinary[255];
	int unit1Binary[255];
	int unit2Binary[255];
	int unit3Binary[255];
	int loop = 1;
	
	int temp[75];
	int footer = 0;
	int header = 0;
	int pulse = 0;
	int onoff[75];
	int all[75];
	int unit[75];
	int rawLength = 0;
	int binaryLength = 0;

	memset(onoff,-1,75);
	memset(all,-1,75);
	memset(unit,-1,75);

	options_add(&options, 'H', "help", no_value, 0, NULL);
	options_add(&options, 'V', "version", no_value, 0, NULL);
	options_add(&options, 'S', "socket", has_value, 0, "^/dev/([A-Za-z]+)([0-9]+)");
	options_add(&options, 'L', "lirc", no_value, 0, NULL);
	options_add(&options, 'G', "gpio", has_value, 0, "^[0-7]$");
	while (1) {
		int c;
		c = options_parse(&options, argc, argv, 1, &args);
		if (c == -1)
			break;
		switch (c) {
			case 'H':
				printf("Usage: %s [options]\n", progname);
				printf("\t -H --help\t\tdisplay usage summary\n");
				printf("\t -V --version\t\tdisplay version\n");		
				printf("\t -S --socket=socket\tread from given socket\n");
				printf("\t -M --module\t\tuse the lirc_rpi kernel module\n");
				printf("\t -G --gpio=#\t\tGPIO pin we're directly reading from\n");
				return (EXIT_SUCCESS);
			break;
			case 'V':
				printf("%s %s\n", progname, "1.0");
				return (EXIT_SUCCESS);
			break;
			case 'M':
				strcpy(hw_mode, "module");
			break;
			case 'G':
				gpio_in = atoi(optarg);
				strcpy(hw_mode, "gpio");
			break;
			case 'S':
				socket = realloc(socket, strlen(optarg)+1);
				socket = optarg;
				have_device = 1;
			break;
			default:
				printf("Usage: %s [options]\n", progname);
				return (EXIT_FAILURE);
			break;
		}
	}
	options_delete(options);
	
	if(strcmp(hw_mode, "module") == 0) {
		hw = hw_default;
		
		if(strcmp(socket, "/var/lirc/lircd") == 0) {
			logprintf(LOG_ERR, "refusing to connect to lircd socket");
			return EXIT_FAILURE;
		}
	
		if(have_device)
			hw.device = socket;
	
		if(!hw.init_func()) {
			return EXIT_FAILURE;
		}
	} else {
		if(wiringPiSetup() == -1)
			return EXIT_FAILURE;

		if(wiringPiISR(gpio_in, INT_EDGE_BOTH) < 0) {
			logprintf(LOG_ERR, "unable to register interrupt for pin %d", gpio_in) ;
			return 1 ;
		}

		(void)piHiPri(55);
	}
	
	printf("Please make sure the daemon is not running when using this debugger.\n\n");	

	while(loop) {
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
			loop = 0;
		else
			state=WAIT;		
		if(strcmp(hw_mode, "module") == 0) {
			data = hw.readdata(0);
			duration = (data & PULSE_MASK);
		} else {
			duration = irq_read(gpio_in);
		}

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
		if((duration > 5000
		   && duration < 100000 && footer == 0) || ((footer-(footer*0.1)<duration) && (footer+(footer*0.1)>duration))) {
			recording = 1;

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
				   Save the header values and the raw code length */
				if(footer>0) {
					if(header == 0) {
						header=raw[1];
					}
					if(rawLength == 0)
						rawLength=bit;
				}
				if(rawLength == 0 || rawLength == bit) {
				   /*|| ((((raw[0]-(raw[0]*0.3)) < header[0]) || ((raw[0]+(raw[0]*0.3)) > header[0]))
 				       && (((raw[1]-(raw[1]*0.3)) < header[1]) || ((raw[1]+(raw[1]*0.3)) > header[1]))
					   && (((raw[bit-1]-(raw[bit-1]*0.1)) < footer) || ((raw[bit-1]+(raw[bit-1]*0.1)) > footer)))) {*/

					/* Try to catch the footer, and the low and high values */
					for(i=0;i<bit;i++) {
						if((i+1)<bit && i > 2 && footer > 0) {
							if((raw[i]/PULSE_LENGTH) >= 2) {
								pulse=raw[i];
							}
						}
						if(duration > 5000 && duration < 100000)
							footer=raw[i];
					}
					/* If we have gathered all data, stop with the loop */
					if(header > 0 && footer > 0 && pulse > 0 && rawLength > 0) {
						/* Convert the raw code into binary code */
						for(i=0;i<rawLength;i++) {
							if((unsigned int)raw[i] > (pulse-PULSE_LENGTH)) {
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
	if(normalize(header) == normalize(pulse)) {
		printf("header:\t\t0\n");
	} else {
		printf("header:\t\t%d\n",normalize(header));
	}
	printf("pulse:\t\t%d\n",normalize(pulse));
	printf("footer:\t\t%d\n",normalize(footer));
	printf("rawLength:\t%d\n",rawLength);
	printf("binaryLength:\t%d\n",binaryLength);
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
		printf("%d ",normalize(raw[i])*PULSE_LENGTH);
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
	printf("\n");
	
	free(hw_mode);
	free(socket);
	free(progname);
	return (EXIT_SUCCESS);
}
