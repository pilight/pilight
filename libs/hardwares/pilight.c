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
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "../../pilight.h"
#include "common.h"
#include "log.h"
#include "hardware.h"
#include "wiringPi.h"
#include "pilight.h"

#define IOCTL_GPIO_IN				10
#define IOCTL_LONGEST_V_P			11
#define IOCTL_SHORTEST_V_P			12
#define IOCTL_START_RECEIVER		13
#define IOCTL_STOP_RECEIVER			14
#define IOCTL_FILTER_ON				15
#define IOCTL_GPIO_OUT				16
#define IOCTL_START_TRANSMITTER		17
#define IOCTL_STOP_TRANSMITTER		18


int pilight_mod_rec_initialized = 0;
int pilight_mod_trans_initialized = 0;
const char *pilight_mod_socket = DEFAULT_PILIGHT_SOCKET;
int pilight_mod_fd_rec = 0;
int pilight_mod_fd_trans = 0;
int pilight_mod_out = 0;
int pilight_mod_in = 0;
int pilight_mod_prefilter = 0;
int pilight_mod_svp = 0;
int pilight_mod_lvp = 0;

static int *pinToGpio ;

static int pinToGpioR1 [64] =
{
  17, 18, 21, 22, 23, 24, 25, 4,        // From the Original Wiki - GPIO 0 through 7:        wpi  0 -  7
   0,  1,                                // I2C  - SDA0, SCL0                                wpi  8 -  9
   8,  7,                                // SPI  - CE1, CE0                                wpi 10 - 11
  10,  9, 11,                                 // SPI  - MOSI, MISO, SCLK                        wpi 12 - 14
  14, 15,                                // UART - Tx, Rx                                wpi 15 - 16

// Padding:

      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,        // ... 31
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,        // ... 47
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,        // ... 63
} ;

static int pinToGpioR2 [64] =
{
  17, 18, 27, 22, 23, 24, 25, 4,        // From the Original Wiki - GPIO 0 through 7:        wpi  0 -  7
   2,  3,                                // I2C  - SDA0, SCL0                                wpi  8 -  9
   8,  7,                                // SPI  - CE1, CE0                                wpi 10 - 11
  10,  9, 11,                                 // SPI  - MOSI, MISO, SCLK                        wpi 12 - 14
  14, 15,                                // UART - Tx, Rx                                wpi 15 - 16
  28, 29, 30, 31,                        // New GPIOs 8 though 11                        wpi 17 - 20

// Padding:

                      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,        // ... 31
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,        // ... 47
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,        // ... 63
} ;

int getWiringPiPin(int pin)
{	
	int boardRev;
	
	boardRev = piBoardRev () ;
	if (boardRev == 1){
		pinToGpio =  pinToGpioR1 ;
	}
	else{
		pinToGpio =  pinToGpioR2 ;
	}
	
	return pinToGpio[pin & 63];
}


unsigned short pilightModHwInit(void) {
	char *socket = NULL;
	int filter_on = 1;

	if(settings_find_string("hw-socket", &socket) == 0) {
		pilight_mod_socket = socket;
	}

	settings_find_number("gpio-sender", &pilight_mod_out);
	settings_find_number("gpio-receiver", &pilight_mod_in);
	settings_find_number("attiny-prefilter", &pilight_mod_prefilter);
	settings_find_number("pilight-prefilter-shortest-pulse", &pilight_mod_svp);
	settings_find_number("pilight-prefilter-longest-pulse", &pilight_mod_lvp);

	if(pilight_mod_prefilter == 1){
		filter_on = 0;
	}
	
	if(pilight_mod_trans_initialized == 0) {
		if((pilight_mod_fd_trans = open(pilight_mod_socket, O_WRONLY)) < 0) {
			logprintf(LOG_ERR, "could not open %s", pilight_mod_socket);
			return EXIT_FAILURE;
		} else {
			
			ioctl(pilight_mod_fd_trans, IOCTL_GPIO_OUT, getWiringPiPin(pilight_mod_out));
			ioctl(pilight_mod_fd_trans, IOCTL_START_TRANSMITTER, 0);
			
			logprintf(LOG_DEBUG, "initialized pilight transmitter module");
			pilight_mod_trans_initialized = 1;
		}
	}
	
	if(pilight_mod_rec_initialized == 0) {
		if((pilight_mod_fd_rec = open(pilight_mod_socket, O_RDONLY)) < 0) {
			logprintf(LOG_ERR, "could not open %s", pilight_mod_socket);
			return EXIT_FAILURE;
		} else {
			
			ioctl(pilight_mod_fd_rec, IOCTL_GPIO_IN, getWiringPiPin(pilight_mod_in));
			ioctl(pilight_mod_fd_rec, IOCTL_LONGEST_V_P, pilight_mod_lvp);
			ioctl(pilight_mod_fd_rec, IOCTL_SHORTEST_V_P, pilight_mod_svp);
			ioctl(pilight_mod_fd_rec, IOCTL_FILTER_ON, filter_on);
			ioctl(pilight_mod_fd_rec, IOCTL_START_RECEIVER, 0);
			
			logprintf(LOG_DEBUG, "initialized pilight receiver module");
			pilight_mod_rec_initialized = 1;
		}
	}
	
	return EXIT_SUCCESS;
}

unsigned short pilightModHwDeinit(void) {
	if(pilight_mod_rec_initialized == 1) {
		ioctl(pilight_mod_fd_rec, IOCTL_STOP_RECEIVER, 0);
		if(pilight_mod_fd_rec != 0) {
			close(pilight_mod_fd_rec);
			pilight_mod_fd_rec = 0;
		}
		logprintf(LOG_DEBUG, "deinitialized pilight receiver module");
		pilight_mod_rec_initialized	= 0;
	}
	
	if(pilight_mod_trans_initialized == 1) {
		ioctl(pilight_mod_fd_trans, IOCTL_STOP_TRANSMITTER, 0);
		if(pilight_mod_fd_trans != 0) {
			close(pilight_mod_fd_trans);
			pilight_mod_fd_trans = 0;
		}

		logprintf(LOG_DEBUG, "deinitialized pilight transmitter module");
		pilight_mod_trans_initialized	= 0;
	}

	return EXIT_SUCCESS;
}

int pilightModSend(int *code) {
	int ret = 0;
	unsigned int code_len = 0;
	
	while(code[code_len]) {
		code_len++;
	}
	code_len++;
	code_len*=sizeof(int);
	
	ret = write(pilight_mod_fd_trans, code, code_len);
	
	if(ret == code_len){
		return 0;
	}
	return -1;
}

int pilightModReceive(void) {
	char buff[255] = {0};
	if((read(pilight_mod_fd_rec, buff, sizeof(buff))) < 0) {
		return -1;
	}

	return (atoi(buff) & 0x00FFFFFF);
}

void pilightModInit(void) {

	hardware_register(&pilight_mod);
	hardware_set_id(pilight_mod, "pilight");

	pilight_mod->init=&pilightModHwInit;
	pilight_mod->deinit=&pilightModHwDeinit;
	pilight_mod->send=&pilightModSend;
	pilight_mod->receive=&pilightModReceive;
}