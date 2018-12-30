/*
	Copyright (C) 2013 - 2015 CurlyMo

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

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

#ifdef _WIN32
	#define STRICT
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#else
	#include <termios.h>
#endif

#include "../core/pilight.h"
#include "../core/json.h"
#include "../core/log.h"
#include "../core/dso.h"
#include "../core/eventpool.h"
#include "../core/firmware.h"
#include "../config/registry.h"
#include "../config/hardware.h"
#include "433nano.h"

typedef struct timestamp_t {
	unsigned long first;
	unsigned long second;
} timestamp_t;

timestamp_t timestamp;

typedef struct data_t {
	char buffer[1024];
	int startv;
	int start;
	int startp;
	int bytes;
} data_t;

static struct data_t data;

/* What is the minimum rawlenth to consider a pulse stream valid */
static int minrawlen = 1000;
/* What is the maximum rawlenth to consider a pulse stream valid */
static int maxrawlen = 0;
/* What is the minimum rawlenth to consider a pulse stream valid */
static int maxgaplen = 5100;
/* What is the maximum rawlenth to consider a pulse stream valid */
static int mingaplen = 10000;

#ifdef _WIN32
static HANDLE serial_433_fd;
#else
static int serial_433_fd = 0;
static int nano_433_initialized = 0;
#endif

static char com[255];
static unsigned short loop = 1;
static unsigned short threads = 0;
static unsigned short sendSync = 0;
static pthread_t pth;

static void *reason_send_code_success_free(void *param) {
	struct reason_send_code_success_free *data = param;
	FREE(data);
	return NULL;
}

static void *reason_send_code_fail_free(void *param) {
	struct reason_send_code_fail_free *data = param;
	FREE(data);
	return NULL;
}

static void *reason_received_pulsetrain_free(void *param) {
	struct reason_received_pulsetrain_t *data = param;
	FREE(data);
	return NULL;
}

void *syncFW(void *param) {

	threads++;

#ifdef _WIN32
	DWORD n;
#else
	int n;
#endif
	char send[MAXPULSESTREAMLENGTH+1];
	int len = 0;

	memset(&send, '\0', sizeof(send));

	struct protocols_t *tmp = protocols;
	while(tmp) {
		if(tmp->listener->hwtype == RF433) {
			if(tmp->listener->maxrawlen > maxrawlen) {
				maxrawlen = tmp->listener->maxrawlen;
			}
			if(tmp->listener->minrawlen > 0 && tmp->listener->minrawlen < minrawlen) {
				minrawlen = tmp->listener->minrawlen;
			}
			if(tmp->listener->maxgaplen > maxgaplen) {
				maxgaplen = tmp->listener->maxgaplen;
			}
			if(tmp->listener->mingaplen > 0 && tmp->listener->mingaplen < mingaplen) {
				mingaplen = tmp->listener->mingaplen;
			}
		}
		tmp = tmp->next;
	}

	/* Let's wait for Nano to accept instructions */
	while(sendSync == 0 && loop == 1) {
		usleep(10);
	}

	len = sprintf(send, "s:%d,%d,%d,%d@", minrawlen, maxrawlen, mingaplen, maxgaplen);

#ifdef _WIN32
	WriteFile(serial_433_fd, &send, len, &n, NULL);
#else
	n = write(serial_433_fd, send, len);
#endif

	if(n != len) {
		logprintf(LOG_NOTICE, "could not sync FW values");
	}

	threads--;

	return NULL;
}

#ifndef _WIN32
/* http://stackoverflow.com/a/6947758 */
static int serial_interface_attribs(int fd, speed_t speed, tcflag_t parity) {
	struct termios tty;

	memset(&tty, 0, sizeof tty);
	if(tcgetattr(fd, &tty) != 0) {
		logprintf(LOG_ERR, "error %d from tcgetattr", errno);
		return -1;
	}

	cfsetospeed(&tty, speed);
	cfsetispeed(&tty, speed);

	tty.c_cflag = (tty.c_cflag & (unsigned int)~CSIZE) | CS8;     // 8-bit chars
	tty.c_iflag &= (unsigned int)~IGNBRK;
	tty.c_lflag = 0;
	tty.c_oflag = 0;
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 5;
	tty.c_iflag &= ~((unsigned int)IXON | (unsigned int)IXOFF | (unsigned int)IXANY);
	tty.c_cflag |= ((unsigned int)CLOCAL | (unsigned int)CREAD);
	tty.c_cflag &= ~((unsigned int)PARENB | (unsigned int)PARODD);
	tty.c_cflag |= parity;
	tty.c_cflag &= (unsigned int)~CSTOPB;
	tty.c_cflag &= (unsigned int)~CRTSCTS;

	if(tcsetattr(fd, TCSANOW, &tty) != 0) {
		logprintf(LOG_ERR, "error %d from tcsetattr", errno);
		return -1;
	}
	return 0;
}
#endif

static void *nano433Send(int reason, void *param) {
	struct reason_send_code_t *data1 = param;
	int *code = data1->pulses;
	int rawlen = data1->rawlen;
	int repeats = data1->txrpt;

	unsigned int i = 0, x = 0, y = 0, len = 0, nrpulses = 0;
	int pulses[10], match = 0;
	char c[16], send[MAXPULSESTREAMLENGTH+1];
#ifdef _WIN32
	DWORD n;
#else
	int n = 0;
#endif

	memset(send, 0, MAXPULSESTREAMLENGTH);
	strncpy(&send[0], "c:", 2);
	len += 2;

	for(i=0;i<rawlen;i++) {
		match = -1;
		for(x=0;x<nrpulses;x++) {
			if(pulses[x] == code[i]) {
				match = (int)x;
				break;
			}
		}
		if(match == -1) {
			pulses[nrpulses] = code[i];
			match = (int)nrpulses;
			nrpulses++;
		}
		if(match < 10) {
			send[len++] = (char)(((int)'0')+match);
		} else {
			logprintf(LOG_ERR, "too many distinct pulses for pilight usb nano to send");
			struct reason_code_sent_fail_t *data2 = MALLOC(sizeof(struct reason_code_sent_fail_t));
			strcpy(data2->message, data1->message);
			strcpy(data2->uuid, data1->uuid);
			eventpool_trigger(REASON_CODE_SEND_FAIL, reason_send_code_fail_free, data2);
			return NULL;
		}
	}

	strncpy(&send[len], ";p:", 3);
	len += 3;
	for(i=0;i<nrpulses;i++) {
		y = (unsigned int)snprintf(c, sizeof(c), "%d", pulses[i]);
		strncpy(&send[len], c, y);
		len += y;
		if(i+1 < nrpulses) {
			strncpy(&send[len++], ",", 1);
		}
	}
	strncpy(&send[len], ";r:", 3);
	len += 3;
	y = (unsigned int)snprintf(c, sizeof(c), "%d", repeats);
	strncpy(&send[len], c, y);
	len += y;
	strncpy(&send[len], "@", 3);
	len += 3;

#ifdef _WIN32
	WriteFile(serial_433_fd, &send, len, &n, NULL);
#else
	n = write(serial_433_fd, send, len);
#endif

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	if(((int)timestamp.second-(int)timestamp.first) < 1000000) {
		sleep(1);
	}

	if(n == len) {
		struct reason_code_sent_success_t *data2 = MALLOC(sizeof(struct reason_code_sent_success_t));
		strcpy(data2->message, data1->message);
		strcpy(data2->uuid, data1->uuid);
		eventpool_trigger(REASON_CODE_SEND_SUCCESS, reason_send_code_success_free, data2);
	} else {
		struct reason_code_sent_fail_t *data2 = MALLOC(sizeof(struct reason_code_sent_fail_t));
		strcpy(data2->message, data1->message);
		strcpy(data2->uuid, data1->uuid);
		eventpool_trigger(REASON_CODE_SEND_FAIL, reason_send_code_fail_free, data2);
	}
	return NULL;
}

static void poll_cb(uv_poll_t *req, int status, int events) {
	char c[1];
	int s = 0, nrpulses = 0, y = 0;
	int pulses[10];
	size_t x = 0;
	int error = 0;

	int fd = req->io_watcher.fd;
#ifdef _WIN32
	DWORD n;
#else
	int n = 0;
#endif

	if(events & UV_READABLE) {
#ifdef _WIN32
		ReadFile(fd, c, 1, &n, NULL);
#else
		n = read(fd, c, 1);
#endif
		if(n > 0) {
			if(c[0] == '\n') {
				sendSync = 1;
				return;
			} else {
				if(c[0] == 'v') {
					data.startv = 1;
					data.start = 1;
					data.bytes = 0;
				}
				if(c[0] == 'c') {
					data.start = 1;
					data.bytes = 0;
				}
				if(c[0] == 'p') {
					data.startp = data.bytes+2;
					data.buffer[data.bytes-1] = '\0';
				}
				if(c[0] == '@') {
					data.buffer[data.bytes] = '\0';
					if(data.startv == 1) {
						data.start = 0;
						data.startv = 0;
						char **array = NULL;
						int c = explode(&data.buffer[2], ",", &array);
						if(c == 7) {
							if(!(minrawlen == atoi(array[0]) && maxrawlen == atoi(array[1]) &&
							     mingaplen == atoi(array[2]) && maxgaplen == atoi(array[3]))) {
								logprintf(LOG_WARNING, "could not sync FW values");
							}
							firmware.version = atof(array[4]);
							firmware.lpf = atof(array[5]);
							firmware.hpf = atof(array[6]);

							if(firmware.version > 0 && firmware.lpf > 0 && firmware.hpf > 0) {
								config_registry_set_number("pilight.firmware.version", firmware.version);
								config_registry_set_number("pilight.firmware.lpf", firmware.lpf);
								config_registry_set_number("pilight.firmware.hpf", firmware.hpf);
								logprintf(LOG_INFO, "pilight-usb-nano version: %d, lpf: %d, hpf: %d", (int)firmware.version, (int)firmware.lpf, (int)firmware.hpf);
							}
						}
						array_free(&array, c);
					} else {
						x = strlen(&data.buffer[data.startp]);
						s = data.startp;
						nrpulses = 0;
						for(y = data.startp; y < data.startp + (int)x; y++) {
							if(data.buffer[y] == ',') {
								data.buffer[y] = '\0';
								pulses[nrpulses++] = atoi(&data.buffer[s]);
								s = y+1;
							}
							if(nrpulses > 9 || s > 1023) {
								logprintf(LOG_NOTICE, "433nano: discarded invalid pulse train");
								error = 1;
								break;
							}
						}
						if(error == 0) {
							pulses[nrpulses++] = atoi(&data.buffer[s]);
							x = strlen(&data.buffer[2]);

							struct reason_received_pulsetrain_t *data1 = MALLOC(sizeof(struct reason_received_pulsetrain_t));
							if(data1 == NULL) {
								OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
							}

							data1->length = 0;

							for(y = 2; y < 2 + x; y++) {
								data1->pulses[data1->length++] = pulses[0];
								data1->pulses[data1->length++] = pulses[data.buffer[y] - '0'];
							}

							data1->hardware = nano433->id;

							eventpool_trigger(REASON_RECEIVED_PULSETRAIN, reason_received_pulsetrain_free, data1);
						}
						data.bytes = 0;
					}
				}
				if(data.start == 1) {
					data.buffer[data.bytes++] = c[0];
				}
			}
		}
	}
	if(events == 0) {
		logprintf(LOG_ERR, "connection lost to pilight-usb-nano on port %s", com);
		nano_433_initialized = 0;
	}
}

static unsigned short int nano433HwInit(void) {
#ifdef _WIN32
	COMMTIMEOUTS timeouts;
	DCB port;
	char tmp[255];
	memset(tmp, '\0', 255);

	snprintf(tmp, 255, "\\\\.\\%s", com);

	if((int)(serial_433_fd = CreateFile(tmp, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL)) < 0) {
		logprintf(LOG_NOTICE, "cannot open port %s", com);
		return EXIT_FAILURE;
	}
	logprintf(LOG_INFO, "connected to port %s", com);

	memset(&port, '\0', sizeof(port));

	port.DCBlength = sizeof(port);
	if(GetCommState(serial_433_fd, &port) == FALSE) {
		logprintf(LOG_ERR, "cannot get comm state for port %s", com);
		return EXIT_FAILURE;
	}

	if(BuildCommDCB("baud=57600 parity=n data=8 stop=1", &port) == FALSE) {
		logprintf(LOG_ERR, "cannot build comm DCB for port %s", com);
		return EXIT_FAILURE;
	}

	if(SetCommState(serial_433_fd, &port) == FALSE) {
		logprintf(LOG_ERR, "cannot set port settings for port %s", com);
		return EXIT_FAILURE;
	}

	timeouts.ReadIntervalTimeout = 1000;
	timeouts.ReadTotalTimeoutMultiplier = 1000;
	timeouts.ReadTotalTimeoutConstant = 1000;
	timeouts.WriteTotalTimeoutMultiplier = 1000;
	timeouts.WriteTotalTimeoutConstant = 1000;

	if(SetCommTimeouts(serial_433_fd, &timeouts) == FALSE) {
		logprintf(LOG_ERR, "error setting port %s time-outs.", com);
		return EXIT_FAILURE;
	}
#else
	if((serial_433_fd = open(com, O_RDWR | O_SYNC)) >= 0) {
		serial_interface_attribs(serial_433_fd, B57600, 0);
		nano_433_initialized = 1;

		uv_poll_t *poll_req = NULL;
		if((poll_req = MALLOC(sizeof(uv_poll_t))) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		memset(data.buffer, '\0', sizeof(data.buffer));
		data.bytes = 0;

		uv_poll_init(uv_default_loop(), poll_req, serial_433_fd);
		uv_poll_start(poll_req, UV_READABLE, poll_cb);

	} else {
		logprintf(LOG_NOTICE, "could not open port %s", com);
		return EXIT_FAILURE;
	}
#endif

	pthread_create(&pth, NULL, &syncFW, (void *)NULL);
	pthread_detach(pth);

	eventpool_callback(REASON_SEND_CODE, nano433Send);

	return EXIT_SUCCESS;
}

static unsigned short nano433HwDeinit(void) {
	loop = 0;
#ifdef _WIN32
	CloseHandle(serial_433_fd);
#else
	if(nano_433_initialized == 1) {
		close(serial_433_fd);
		nano_433_initialized = 0;
	}
#endif
	return EXIT_SUCCESS;
}

static unsigned short nano433Settings(JsonNode *json) {
	if(strcmp(json->key, "comport") == 0) {
		if(json->tag == JSON_STRING) {
			strcpy(com, json->string_);
			return EXIT_SUCCESS;
		}
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void nano433Init(void) {
	hardware_register(&nano433);
	hardware_set_id(nano433, "433nano");

	options_add(&nano433->options, "p", "comport", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);

	nano433->hwtype=RF433;
	nano433->comtype=COMPLSTRAIN;
	nano433->init=&nano433HwInit;
	nano433->deinit=&nano433HwDeinit;
	nano433->settings=&nano433Settings;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "433nano";
	module->version = "1.2";
	module->reqversion = "7.0";
	module->reqcommit = "10";
}

void init(void) {
	nano433Init();
}
#endif
