/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#ifdef _WIN32
	#define STRICT
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#else
	#include <sys/time.h>
	#include <unistd.h>
	#include <termios.h>
#endif

#include "../../libuv/uv.h"
#include "../core/pilight.h"
#include "../core/eventpool.h"
#include "../core/json.h"
#include "../core/log.h"
#include "../core/dso.h"
#include "../core/firmware.h"
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

typedef struct data1_t {
	int fd;
	unsigned int ptr;
	char rbuf[BUFFER_SIZE];
	char wbuf[BUFFER_SIZE];
	char buf[BUFFER_SIZE];
} data1_t;

static struct data1_t data1;

typedef struct data2_t {
	char message[1025];
	char uuid[UUID_LENGTH+1];
	int fd;
} data2_t;

typedef struct timestamp_t {
	struct timespec first;
	struct timespec second;
} timestamp_t;

static struct timestamp_t timestamp;

static uv_timer_t *timer_req = NULL;
static char com[255];
static int dowrite = 0;
static int dostop = 0;

static void *reason_received_pulsetrain_free(void *param) {
	struct reason_received_pulsetrain_t *data = param;
	FREE(data);
	return NULL;
}

static void *reason_send_code_fail_free(void *param) {
	struct reason_send_code_fail_t *data = param;
	FREE(data);
	return NULL;
}

static void *reason_send_code_success_free(void *param) {
	struct reason_send_code_success_free *data = param;
	FREE(data);
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
	tty.c_cc[VTIME] = 1;
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

static void on_write_init(uv_fs_t *req) {
	if(req->result < 0) {
		logprintf(LOG_ERR, "write error: %s", uv_strerror((int)req->result));
	}
	uv_fs_req_cleanup(req);
	FREE(req);
}

static void on_write_code(uv_fs_t *req) {
	struct data2_t *data = req->data;
	if(req->result < 0) {
		struct reason_code_sent_fail_t *data1 = MALLOC(sizeof(struct reason_code_sent_fail_t));
		strcpy(data1->message, data->message);
		strcpy(data1->uuid, data->uuid);
		eventpool_trigger(REASON_CODE_SEND_FAIL, reason_send_code_fail_free, data1);
	} else {
		struct reason_code_sent_success_t *data1 = MALLOC(sizeof(struct reason_code_sent_success_t));
		strcpy(data1->message, data->message);
		strcpy(data1->uuid, data->uuid);
		eventpool_trigger(REASON_CODE_SEND_SUCCESS, reason_send_code_success_free, data1);
	}
	uv_fs_req_cleanup(req);
	FREE(data);
	FREE(req);
}

static void parse_code(char *buffer, unsigned int len) {
	char c = 0;
	unsigned int i = 0, startv = 0, startc = 0, startp = 0;

	for(i=0;i<len;i++) {
		c = buffer[i];
		if(c == '\n') {
			return;
		} else if(c == 'v') {
			startv = i+2;
		} else if(c == 'c') {
			startc = i+2;
		} else if(c == 'p') {
			buffer[i-1] = '\0';
			i++;
			startp = i+1;
		} else if(c == '@') {
			if(startv > 0 && dowrite < 3) {
				memmove(buffer, &buffer[startv], i-2);
				buffer[i-2] = '\0';

				dowrite = 3;
				char **array = NULL;
				int n = explode(buffer, ",", &array);

				if(n == 7) {
					if(!(nano433->minrawlen == atoi(array[0]) && nano433->maxrawlen == atoi(array[1]) &&
							nano433->mingaplen == atoi(array[2]) && nano433->maxgaplen == atoi(array[3]))) {
						logprintf(LOG_WARNING, "could not sync FW values");
					} else {
						int version = atoi(array[4]);
						int lpf = atoi(array[5]);
						int hpf = atoi(array[6]);

						logprintf(LOG_NOTICE, "initialized pilight usb nano");
						if(version > 0 && lpf > 0 && hpf > 0) {
							logprintf(LOG_DEBUG, "filter version: %d, lpf: %d, hpf: %d", version, lpf, hpf);
						}
					}
				}
				array_free(&array, n);
			} else if(startc > 0 && startp > 0) {
				const char *p = NULL;
				int x = strlen(&buffer[startp]), rawlen = 0;
				int s = startp, y = 0, nrpulses = 0, i = 0;
				int pulses[10];
				memset(pulses, 0, 10);

				for(y = startp; y < startp + (int)x; y++) {
					if(buffer[y] == ',') {
						buffer[y] = '\0';
						p = &buffer[s];
						pulses[nrpulses++] = atoi(p);
						s = y+1;
					}
				}
				p = &buffer[s];
				pulses[nrpulses++] = atoi(p);

				struct reason_received_pulsetrain_t *data2 = MALLOC(sizeof(struct reason_received_pulsetrain_t));
				if(data2 == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				x = strlen(&buffer[startc]);
				i = 0;
				for(y = startc; y < startc + x; y++) {
					rawlen += 2;
					data2->pulses[i++] = pulses[0];
					data2->pulses[i++] = pulses[buffer[y] - '0'];
				}

				data2->length = rawlen;
				data2->hardware = nano433->id;
				eventpool_trigger(REASON_RECEIVED_PULSETRAIN, reason_received_pulsetrain_free, data2);
			}
		}
	}
}

static void reconnect(uv_timer_t *timer_req) {
	nano433->init();
}

#ifdef _WIN32
/* http://stackoverflow.com/a/38212960 */
#define CLOCK_MONOTONIC 0
#define BILLION (1E9)
static BOOL g_first_time = 1;
static LARGE_INTEGER g_counts_per_sec;

int clock_gettime(int dummy, struct timespec *ct) {
	LARGE_INTEGER count;

	if(g_first_time) {
		g_first_time = 0;

		if(0 == QueryPerformanceFrequency(&g_counts_per_sec)) {
			g_counts_per_sec.QuadPart = 0;
		}
	}

	if((NULL == ct) || (g_counts_per_sec.QuadPart <= 0) || (0 == QueryPerformanceCounter(&count))) {
		return -1;
	}

	ct->tv_sec = count.QuadPart / g_counts_per_sec.QuadPart;
	ct->tv_nsec = ((count.QuadPart % g_counts_per_sec.QuadPart) * BILLION) / g_counts_per_sec.QuadPart;

	return 0;
}
#endif

static void on_read(uv_fs_t *req) {
	unsigned int len = req->result, pos = 0;
	char *p = NULL;
	if((int)len < 0) {
		logprintf(LOG_ERR, "read error: %s", uv_strerror(req->result));
	} else if((int)len >= 0) {
		memcpy(&timestamp.first, &timestamp.second, sizeof(struct timespec));
		clock_gettime(CLOCK_MONOTONIC, &timestamp.second);
		if((((double)timestamp.second.tv_sec + 1.0e-9*timestamp.second.tv_nsec) -
			((double)timestamp.first.tv_sec + 1.0e-9*timestamp.first.tv_nsec)) < 0.0001) {
			dostop = 1;

			uv_timer_start(timer_req, reconnect, 1000, 1000);
		}
		if(len > 0) {
			if(dowrite == 0) {
				if(data1.rbuf[0] == 10) {
					dowrite = 1;
				}
			}
			if(data1.rbuf[0] != 10) {
				while((pos++ < len) && (data1.rbuf[pos] == '\n'));
				pos--;
				memcpy(&data1.buf[data1.ptr], &data1.rbuf[pos], len-pos);
				if((p = strstr(data1.rbuf, "@")) != NULL) {
					data1.ptr += (p-data1.rbuf);
					data1.buf[++data1.ptr] = '\0';

					parse_code(data1.buf, data1.ptr);

					data1.ptr = 0;
					memset(data1.buf, 0, BUFFER_SIZE);
				} else {
					data1.ptr += len;
				}
			}

			if(dowrite == 1) {
				dowrite = 2;

				memset(&data1.wbuf, 0, BUFFER_SIZE);
				uv_fs_t *write_req = NULL;
				if((write_req = MALLOC(sizeof(uv_fs_t))) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}

				uv_buf_t wbuf = uv_buf_init(data1.wbuf, BUFFER_SIZE);

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

				wbuf.len = snprintf(wbuf.base, MAXPULSESTREAMLENGTH, "s:%d,%d,%d,%d@", nano433->minrawlen, nano433->maxrawlen, nano433->mingaplen, nano433->maxgaplen);
				uv_fs_write(uv_default_loop(), write_req, data1.fd, &wbuf, 1, -1, on_write_init);
			}
		}

		memset(&data1.rbuf, 0, BUFFER_SIZE);
		uv_buf_t rbuf = uv_buf_init(data1.rbuf, BUFFER_SIZE);
		if(dostop == 0) {
			uv_fs_read(uv_default_loop(), req, data1.fd, &rbuf, 1, -1, on_read);
		} else {
			uv_fs_t *close_req = NULL;
			if((close_req = MALLOC(sizeof(uv_fs_t))) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			uv_fs_close(uv_default_loop(), close_req, data1.fd, NULL);
			uv_fs_req_cleanup(req);
			uv_fs_req_cleanup(close_req);
			FREE(req);
			FREE(close_req);
		}
	}
}

static void open_cb(uv_fs_t *req) {
	int fd = req->result;

	if(fd >= 0) {
		uv_timer_stop(timer_req);

#ifdef _WIN32
		COMMTIMEOUTS timeouts;
		DCB port;

		memset(&port, '\0', sizeof(port));

		/*
		 * FIXME: Raise
		 */
		port.DCBlength = sizeof(port);
		if(GetCommState((HANDLE)_get_osfhandle(fd), &port) == FALSE) {
			logprintf(LOG_ERR, "cannot get comm state for port");
			raise(SIGINT);
			uv_fs_req_cleanup(req);
			FREE(req);
			return;
		}

		if(BuildCommDCB("baud=57600 parity=n data=8 stop=1", &port) == FALSE) {
			logprintf(LOG_ERR, "cannot build comm DCB for port\n");
			raise(SIGINT);
			uv_fs_req_cleanup(req);
			FREE(req);
			return;
		}

		if(SetCommState((HANDLE)_get_osfhandle(fd), &port) == FALSE) {
			logprintf(LOG_ERR, "cannot build comm DCB for port");
			raise(SIGINT);
			uv_fs_req_cleanup(req);
			FREE(req);
			return;
		}

		timeouts.ReadIntervalTimeout = MAXDWORD;
		timeouts.ReadTotalTimeoutMultiplier = 0;
		timeouts.ReadTotalTimeoutConstant = 1;
		timeouts.WriteTotalTimeoutMultiplier = 0;
		timeouts.WriteTotalTimeoutConstant = 0;

		if(SetCommTimeouts((HANDLE)_get_osfhandle(fd), &timeouts) == FALSE) {
			logprintf(LOG_ERR, "error setting port %s time-outs");
			raise(SIGINT);
			uv_fs_req_cleanup(req);
			FREE(req);
			return;
		}
#else
		serial_interface_attribs(fd, B57600, 0);
#endif

		uv_fs_t *read_req = NULL;
		if((read_req = MALLOC(sizeof(uv_fs_t))) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}

		memset(&data1, 0, sizeof(struct data1_t));
		data1.fd = fd;
		data1.ptr = 0;

		uv_buf_t buf = uv_buf_init(data1.rbuf, BUFFER_SIZE);
		uv_fs_read(uv_default_loop(), read_req, fd, &buf, 1, -1, on_read);
		clock_gettime(CLOCK_MONOTONIC, &timestamp.second);
	} else {
		logprintf(LOG_ERR, "error opening serial device: %s", uv_strerror((int)req->result));
	}
	uv_fs_req_cleanup(req);
	FREE(req);
}

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static unsigned short int nano433HwDeinit(void) {
	dostop = 1;
	if(timer_req != NULL) {
		uv_timer_stop(timer_req);
		uv_close((uv_handle_t *)timer_req, close_cb);
	}
	return 0;
}

static void *nano433Send(int reason, void *param, void *userdata) {
	struct reason_send_code_t *data = param;
	int *code = data->pulses;
	int rawlen = data->rawlen;
	int repeats = data->txrpt;

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

	memset(&data1.wbuf, 0, BUFFER_SIZE);
	uv_fs_t *write_req = NULL;
	if((write_req = MALLOC(sizeof(uv_fs_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_buf_t wbuf = uv_buf_init(data1.wbuf, BUFFER_SIZE);

	strcpy(wbuf.base, send);
	wbuf.len = len;

	struct data2_t *data2 = MALLOC(sizeof(struct data2_t));
	if(data2 == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(data2->message, data->message);
	strcpy(data2->uuid, data->uuid);
	write_req->data = data2;

	uv_fs_write(uv_default_loop(), write_req, data1.fd, &wbuf, 1, -1, on_write_code);

	return 0;
}

static unsigned short int nano433HwInit(void) {
	logprintf(LOG_NOTICE, "initializing pilight usb nano");

	if(timer_req == NULL) {
		timer_req = MALLOC(sizeof(uv_timer_t));
		if(timer_req == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		uv_timer_init(uv_default_loop(), timer_req);
		uv_timer_start(timer_req, reconnect, 1000, 1000);
	}

	dostop = 0;
	dowrite = 0;

	uv_fs_t *open_req = NULL;
	if((open_req = MALLOC(sizeof(uv_fs_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

#ifdef _WIN32
	char tmp[255];
	memset(tmp, '\0', 255);

	snprintf(tmp, 255, "\\\\.\\%s", com);

	uv_fs_open(uv_default_loop(), open_req, tmp, O_RDWR, 0, open_cb);
#else
	uv_fs_open(uv_default_loop(), open_req, com, O_RDWR, 0, open_cb);
#endif

	eventpool_callback(REASON_SEND_CODE, nano433Send, NULL);

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

	nano433->minrawlen = 1000;
	nano433->maxrawlen = 0;
	nano433->maxgaplen = 5100;
	nano433->mingaplen = 10000;

	nano433->hwtype=RF433;
	nano433->comtype=COMPLSTRAIN;
	nano433->init=&nano433HwInit;
	nano433->deinit=&nano433HwDeinit;
	nano433->settings=&nano433Settings;
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
