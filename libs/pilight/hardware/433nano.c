/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
#include "../core/eventpool.h"
#include "../core/json.h"
#include "../core/log.h"
#include "../core/dso.h"
#include "../core/firmware.h"
#include "../core/threadpool.h"
#include "../protocols/protocol.h"
#include "433nano.h"

typedef struct data_t {
	char rbuffer[1024];
	int rptr;
	int syncfw;
	int startp;
	int startc;
	int startv;

} data_t;

typedef struct timestamp_t {
	unsigned long first;
	unsigned long second;
} timestamp_t;

timestamp_t timestamp;

static char com[255];
static int fd = 0;
static int doPause = 0;

static void *reason_received_pulsetrain_free(void *param) {
	struct reason_received_pulsetrain_t *data = param;
	FREE(data);
	return NULL;
}

static int serial_write(struct eventpool_fd_t *node) {
#ifdef _WIN32
	DWORD n;
	WriteFile(node->fd, &node->send_iobuf.buf, node->send_iobuf.len, &n, NULL);
#else
	int n = 0;
	n = write(node->fd, node->send_iobuf.buf, node->send_iobuf.len);
#endif
	return (int)n;
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

static int client_callback(struct eventpool_fd_t *node, int event) {
	struct data_t *data = node->userdata;
	int n = 0, c = 0;

	if(doPause == 1) {
		return 0;
	}
	switch(event) {
		case EV_CONNECT_SUCCESS: {
			fd = node->fd;
			eventpool_fd_enable_read(node);
		} break;
		case EV_READ: {
			eventpool_fd_enable_read(node);
			n = read(node->fd, &c, 1);
			if(c == '\n') {
				data->rptr = 0;
				if(data->syncfw != 3) {
					data->syncfw = 1;
					eventpool_fd_enable_write(node);
				}
			} else if(c == 'v') {
				data->startv = data->rptr+1;
			} else if(c == 'c') {
				data->rbuffer[data->rptr++] = c;
				data->startc = data->rptr+1;
			} else if(c == 'p') {
				data->rbuffer[data->rptr-1] = '\0';
				data->rptr++;
				data->startp = data->rptr+1;
			} else if(c == '@') {
				data->rbuffer[data->rptr] = '\0';
				if(data->startv > 0 && data->syncfw < 3) {
					memmove(data->rbuffer, &data->rbuffer[data->startv], data->rptr-2);
					data->syncfw = 3;
					char **array = NULL;
					n = explode(data->rbuffer, ",", &array);
					if(n == 7) {
						if(!(nano433->minrawlen == atoi(array[0]) && nano433->maxrawlen == atoi(array[1]) &&
								nano433->mingaplen == atoi(array[2]) && nano433->maxgaplen == atoi(array[3]))) {
							logprintf(LOG_WARNING, "could not sync FW values");
						} else {
							double version = atof(array[4]);
							double lpf = atof(array[5]);
							double hpf = atof(array[6]);

							if(version > 0 && lpf > 0 && hpf > 0) {
								logprintf(LOG_DEBUG, "filter version: %.0f, lpf: %.0f, hpf: %.0f", version, lpf, hpf);
							}
						}
					}
					array_free(&array, n);
				} else if(data->startc > 0 && data->startp > 0) {
					const char *p = NULL;
					int x = strlen(&data->rbuffer[data->startp]), rawlen = 0;
					int s = data->startp, y = 0, nrpulses = 0, i = 0;
					int pulses[10];
					memset(pulses, 0, 10);

					for(y = data->startp; y < data->startp + (int)x; y++) {
						if(data->rbuffer[y] == ',') {
							data->rbuffer[y] = '\0';
							p = &data->rbuffer[s];
							pulses[nrpulses++] = atoi(p);
							s = y+1;
						}
					}
					p = &data->rbuffer[s];
					pulses[nrpulses++] = atoi(p);

					struct reason_received_pulsetrain_t *data1 = MALLOC(sizeof(struct reason_received_pulsetrain_t));
					if(data1 == NULL) {
						OUT_OF_MEMORY
					}
					x = strlen(&data->rbuffer[data->startc]);
					i = 0;
					for(y = data->startc; y < data->startc + x; y++) {
						rawlen += 2;
						data1->pulses[i++] = pulses[0];
						data1->pulses[i++] = pulses[data->rbuffer[y] - '0'];
					}

					data1->length = rawlen;
					data1->hardware = nano433->id;
					eventpool_trigger(REASON_RECEIVED_PULSETRAIN, reason_received_pulsetrain_free, data1);
					eventpool_fd_enable_read(node);
					return 0;
				}
				data->rptr = 0;
				data->startv = 0;
				data->startc = 0;
				data->startp = 0;
			} else {
				data->rbuffer[data->rptr++] = c;
				if(data->rptr > sizeof(data->rbuffer)) {
					logprintf(LOG_ERR, "433nano read buffer is full");
					data->rptr = 0;
				}
			}
			eventpool_fd_enable_read(node);
		} break;
		case EV_WRITE: {
			if(data->syncfw == 1) {
				struct protocols_t *tmp = protocols;
				while(tmp) {
					if(tmp->listener->hwtype == RF433) {
						if(tmp->listener->maxrawlen > nano433->maxrawlen) {
							nano433->maxrawlen = tmp->listener->maxrawlen;
						}
						if(tmp->listener->minrawlen > 0 && tmp->listener->minrawlen < nano433->minrawlen) {
							nano433->minrawlen = tmp->listener->minrawlen;
						}
						if(tmp->listener->maxgaplen > nano433->maxgaplen) {
							nano433->maxgaplen = tmp->listener->maxgaplen;
						}
						if(tmp->listener->mingaplen > 0 && tmp->listener->mingaplen < nano433->mingaplen) {
							nano433->mingaplen = tmp->listener->mingaplen;
						}
					}
					tmp = tmp->next;
				}
				char send[MAXPULSESTREAMLENGTH+1];
				int len = 0;

				memset(&send, '\0', sizeof(send));

				len = snprintf(send, MAXPULSESTREAMLENGTH, "s:%d,%d,%d,%d@", nano433->minrawlen, nano433->maxrawlen, nano433->mingaplen, nano433->maxgaplen);
				n = write(node->fd, send, len);
				data->syncfw = 2;
			}
			eventpool_fd_enable_read(node);
		} break;
		case EV_DISCONNECTED: {
			fd = 0;
			FREE(node->userdata);
			eventpool_fd_remove(node);
		} break;
	}
	return 0;
}

static unsigned short int nano433HwInit(void *(*callback)(void *)) {
	int fd = 0;
#ifdef _WIN32
	COMMTIMEOUTS timeouts;
	DCB port;
	char tmp[255];
	memset(tmp, '\0', 255);

	snprintf(tmp, 255, "\\\\.\\%s", com);

	if((int)(fd = CreateFile(tmp, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL)) < 0) {
		logprintf(LOG_NOTICE, "cannot open port %s", com);
		return EXIT_FAILURE;
	}
	logprintf(LOG_INFO, "connected to port %s", com);

	memset(&port, '\0', sizeof(port));

	port.DCBlength = sizeof(port);
	if(GetCommState(fd, &port) == FALSE) {
		logprintf(LOG_ERR, "cannot get comm state for port %s", com);
		return EXIT_FAILURE;
	}

	if(BuildCommDCB("baud=57600 parity=n data=8 stop=1", &port) == FALSE) {
		logprintf(LOG_ERR, "cannot build comm DCB for port %s", com);
		return EXIT_FAILURE;
	}

	if(SetCommState(fd, &port) == FALSE) {
		logprintf(LOG_ERR, "cannot set port settings for port %s", com);
		return EXIT_FAILURE;
	}

	timeouts.ReadIntervalTimeout = 1000;
	timeouts.ReadTotalTimeoutMultiplier = 1000;
	timeouts.ReadTotalTimeoutConstant = 1000;
	timeouts.WriteTotalTimeoutMultiplier = 1000;
	timeouts.WriteTotalTimeoutConstant = 1000;

	if(SetCommTimeouts(fd, &timeouts) == FALSE) {
		logprintf(LOG_ERR, "error setting port %s time-outs.", com);
		return EXIT_FAILURE;
	}
#else
	if((fd = open(com, O_RDWR | O_SYNC)) >= 0) {
		serial_interface_attribs(fd, B57600, 0);
	} else {
		logprintf(LOG_NOTICE, "could not open port %s", com);
		return EXIT_FAILURE;
	}
#endif

	struct data_t *data = MALLOC(sizeof(struct data_t));
	if(data == NULL) {
		OUT_OF_MEMORY;
	}
	memset(data->rbuffer, '\0', sizeof(data->rbuffer));
	data->rptr = 0;
	data->syncfw = 0;

	eventpool_fd_add("433nano", fd, client_callback, serial_write, data);

	return EXIT_SUCCESS;
}

static int nano433Send(int *code, int rawlen, int repeats) {
	unsigned int i = 0, x = 0, y = 0, len = 0, nrpulses = 0;
	int pulses[10], match = 0;
	char c[16], send[MAXPULSESTREAMLENGTH+1];

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
			return EXIT_FAILURE;
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

	eventpool_fd_write(fd, send, len);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	timestamp.first = timestamp.second;
	timestamp.second = 1000000 * (unsigned int)tv.tv_sec + (unsigned int)tv.tv_usec;

	if(((int)timestamp.second-(int)timestamp.first) < 1000000) {
		sleep(1);
	}

	return 0;
}

static void *receiveStop(void *param) {
	doPause = 1;
	return NULL;
}

static void *receiveStart(void *param) {
	doPause = 0;
	return NULL;
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

	options_add(&nano433->options, 'p', "comport", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);

	nano433->minrawlen = 1000;
	nano433->maxrawlen = 0;
	nano433->maxgaplen = 5100;
	nano433->mingaplen = 10000;

	nano433->hwtype=RF433;
	nano433->comtype=COMPLSTRAIN;
	nano433->init=&nano433HwInit;
	nano433->sendOOK=&nano433Send;
	nano433->settings=&nano433Settings;

	eventpool_callback(REASON_SEND_BEGIN, receiveStop);
	eventpool_callback(REASON_SEND_END, receiveStart);
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "433nano";
	module->version = "2.0";
	module->reqversion = "8.0";
	module->reqcommit = NULL;
}

void init(void) {
	nano433Init();
}
#endif
