/*
	Copyright (C) 2014 CurlyMo & hstroh

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
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "threads.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "json.h"
#include "bmp180.h"
#include "../pilight/wiringX.h"

typedef struct bmp180data_t {
	char **id;
	int nrid;
	int *fd;
} bmp180data_t;

static unsigned short bmp180_loop = 1;
static int bmp180_threads = 0;

static pthread_mutex_t bmp180lock;
static pthread_mutexattr_t bmp180attr;

// helper function with built-in result conversion
static int readReg16(int fd, int reg) {
	int res = wiringXI2CReadReg16(fd, reg);
	// Convert result to 16 bits and swap bytes
	return ((res << 8) & 0xFF00) | ((res >> 8) & 0xFF);
}

static void *bmp180Parse(void *param) {
	struct protocol_threads_t *node = (struct protocol_threads_t *) param;
	struct JsonNode *json = (struct JsonNode *) node->param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct bmp180data_t *bmp180data = malloc(sizeof(struct bmp180data_t));
	int y = 0, interval = 10, nrloops = 0;
	char *stmp = NULL;
	double itmp = -1, temp_offset = 0.0, pressure_offset = 0.0;
	unsigned char oversampling = 1;

	if (!bmp180data) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}

	bmp180data->nrid = 0;
	bmp180data->id = NULL;
	bmp180data->fd = 0;

	bmp180_threads++;

	if ((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while (jchild) {
			if (json_find_string(jchild, "id", &stmp) == 0) {
				bmp180data->id = realloc(bmp180data->id, (sizeof(char *) * (size_t)(bmp180data->nrid + 1)));
				if (!bmp180data->id) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				bmp180data->id[bmp180data->nrid] = malloc(strlen(stmp) + 1);
				if (!bmp180data->id[bmp180data->nrid]) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				strcpy(bmp180data->id[bmp180data->nrid], stmp);
				bmp180data->nrid++;
			}
			jchild = jchild->next;
		}
	}

	if (json_find_number(json, "poll-interval", &itmp) == 0)
		interval = (int) round(itmp);
	json_find_number(json, "temperature-offset", &temp_offset);
	json_find_number(json, "pressure-offset", &pressure_offset);
	if (json_find_number(json, "oversampling", &itmp) == 0) {
		oversampling = (unsigned char) itmp;
	}

#ifndef __FreeBSD__
	// resize the memory blocks pointed to by the different pointers
	bmp180data->fd = realloc(bmp180data->fd, (sizeof(int) * (size_t) bmp180data->nrid + 1));
	if (!bmp180data->fd) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}

	// structure for calibration data (stored in each BMP180/085)
	struct {
		short ac1;
		short ac2;
		short ac3;
		unsigned short ac4;
		unsigned short ac5;
		unsigned short ac6;
		short b1;
		short b2;
		short mb;
		short mc;
		short md;
	} cd[bmp180data->nrid];

	for (y = 0; y < bmp180data->nrid; y++) {
		// setup i2c
		bmp180data->fd[y] = wiringXI2CSetup((int) strtol(bmp180data->id[y], NULL, 16));

		if (bmp180data->fd[y] > 0) {
			// read 0xD0 to check i2c setup: setup is ok if result contains constant value 0x55
			int id = wiringXI2CReadReg8(bmp180data->fd[y], 0xD0);
			if (id != 0x55) {
				logprintf(LOG_ERR, "wrong device detected");
				exit(EXIT_FAILURE);
			}

			// read calibration coefficients from register addresses
			cd[y].ac1 = (short) readReg16(bmp180data->fd[y], 0xAA);
			cd[y].ac2 = (short) readReg16(bmp180data->fd[y], 0xAC);
			cd[y].ac3 = (short) readReg16(bmp180data->fd[y], 0xAE);
			cd[y].ac4 = (unsigned short) readReg16(bmp180data->fd[y], 0xB0);
			cd[y].ac5 = (unsigned short) readReg16(bmp180data->fd[y], 0xB2);
			cd[y].ac6 = (unsigned short) readReg16(bmp180data->fd[y], 0xB4);
			cd[y].b1 = (short) readReg16(bmp180data->fd[y], 0xB6);
			cd[y].b2 = (short) readReg16(bmp180data->fd[y], 0xB8);
			cd[y].mb = (short) readReg16(bmp180data->fd[y], 0xBA);
			cd[y].mc = (short) readReg16(bmp180data->fd[y], 0xBC);
			cd[y].md = (short) readReg16(bmp180data->fd[y], 0xBE);

			// check communication: no result must equal 0 or 0xFFFF (=65535)
			if (cd[y].ac1 == 0 || cd[y].ac1 == 0xFFFF ||
					cd[y].ac2 == 0 || cd[y].ac2 == 0xFFFF ||
					cd[y].ac3 == 0 || cd[y].ac3 == 0xFFFF ||
					cd[y].ac4 == 0 || cd[y].ac4 == 0xFFFF ||
					cd[y].ac5 == 0 || cd[y].ac5 == 0xFFFF ||
					cd[y].ac6 == 0 || cd[y].ac6 == 0xFFFF ||
					cd[y].b1 == 0 || cd[y].b1 == 0xFFFF ||
					cd[y].b2 == 0 || cd[y].b2 == 0xFFFF ||
					cd[y].mb == 0 || cd[y].mb == 0xFFFF ||
					cd[y].mc == 0 || cd[y].mc == 0xFFFF ||
					cd[y].md == 0 || cd[y].md == 0xFFFF) {
				logprintf(LOG_ERR, "data communication error");
				exit(EXIT_FAILURE);
			}
		}
	}
#endif

	while (bmp180_loop) {
		if (protocol_thread_wait(node, interval, &nrloops) == ETIMEDOUT) {
#ifndef __FreeBSD__
			pthread_mutex_lock(&bmp180lock);
			for (y = 0; y < bmp180data->nrid; y++) {
				if (bmp180data->fd[y] > 0) {
					// uncompensated temperature value
					unsigned short ut = 0;

					// write 0x2E into Register 0xF4 to request a temperature reading.
					wiringXI2CWriteReg8(bmp180data->fd[y], 0xF4, 0x2E);

					// wait at least 4.5ms: we suspend execution for 5000 microseconds.
					usleep(5000);

					// read the two byte result from address 0xF6.
					ut = (unsigned short) readReg16(bmp180data->fd[y], 0xF6);

					// calculate temperature (in units of 0.1 deg C) given uncompensated value
					int x1, x2;
					x1 = (((int) ut - (int) cd[y].ac6)) * (int) cd[y].ac5 >> 15;
					x2 = ((int) cd[y].mc << 11) / (x1 + cd[y].md);
					int b5 = x1 + x2;
					int temp = ((b5 + 8) >> 4);

					// uncompensated pressure value
					unsigned int up = 0;

					// write 0x34+(BMP085_OVERSAMPLING_SETTING<<6) into register 0xF4
					// request a pressure reading with specified oversampling setting
					wiringXI2CWriteReg8(bmp180data->fd[y], 0xF4,
							0x34 + (oversampling << 6));

					// wait for conversion, delay time dependent on oversampling setting
					unsigned int delay = (unsigned int) ((2 + (3 << oversampling)) * 1000);
					usleep(delay);

					// read the three byte result (block data): 0xF6 = MSB, 0xF7 = LSB and 0xF8 = XLSB
					int msb = wiringXI2CReadReg8(bmp180data->fd[y], 0xF6);
					int lsb = wiringXI2CReadReg8(bmp180data->fd[y], 0xF7);
					int xlsb = wiringXI2CReadReg8(bmp180data->fd[y], 0xF8);
					up = (((unsigned int) msb << 16) | ((unsigned int) lsb << 8) | (unsigned int) xlsb)
							>> (8 - oversampling);

					// calculate pressure (in Pa) given uncompensated value
					int x3, b3, b6, pressure;
					unsigned int b4, b7;

					// calculate B6
					b6 = b5 - 4000;

					// calculate B3
					x1 = (cd[y].b2 * (b6 * b6) >> 12) >> 11;
					x2 = (cd[y].ac2 * b6) >> 11;
					x3 = x1 + x2;
					b3 = (((cd[y].ac1 * 4 + x3) << oversampling) + 2) >> 2;

					// calculate B4
					x1 = (cd[y].ac3 * b6) >> 13;
					x2 = (cd[y].b1 * ((b6 * b6) >> 12)) >> 16;
					x3 = ((x1 + x2) + 2) >> 2;
					b4 = (cd[y].ac4 * (unsigned int) (x3 + 32768)) >> 15;

					// calculate B7
					b7 = ((up - (unsigned int) b3) * ((unsigned int) 50000 >> oversampling));

					// calculate pressure in Pa
					pressure = b7 < 0x80000000 ? (int) ((b7 << 1) / b4) : (int) ((b7 / b4) << 1);
					x1 = (pressure >> 8) * (pressure >> 8);
					x1 = (x1 * 3038) >> 16;
					x2 = (-7357 * pressure) >> 16;
					pressure += (x1 + x2 + 3791) >> 4;

					bmp180->message = json_mkobject();
					JsonNode *code = json_mkobject();
					json_append_member(code, "id", json_mkstring(bmp180data->id[y]));
					json_append_member(code, "temperature", json_mknumber(((double) temp / 10) + temp_offset, 1)); // in deg C
					json_append_member(code, "pressure", json_mknumber(((double) pressure / 100) + pressure_offset, 1)); // in hPa

					json_append_member(bmp180->message, "message", code);
					json_append_member(bmp180->message, "origin", json_mkstring("receiver"));
					json_append_member(bmp180->message, "protocol", json_mkstring(bmp180->id));

					pilight.broadcast(bmp180->id, bmp180->message);
					json_delete(bmp180->message);
					bmp180->message = NULL;
				} else {
					logprintf(LOG_DEBUG, "error connecting to bmp180");
					logprintf(LOG_DEBUG, "(probably i2c bus error from wiringXI2CSetup)");
					logprintf(LOG_DEBUG, "(maybe wrong id? use i2cdetect to find out)");
					protocol_thread_wait(node, 1, &nrloops);
				}
			}
			pthread_mutex_unlock(&bmp180lock);
#endif
		}
	}

	if (bmp180data->id) {
		for (y = 0; y < bmp180data->nrid; y++) {
			sfree((void *) &bmp180data->id[y]);
		}
		sfree((void *) &bmp180data->id);
	}
	if (bmp180data->fd) {
		for (y = 0; y < bmp180data->nrid; y++) {
			if (bmp180data->fd[y] > 0) {
				close(bmp180data->fd[y]);
			}
		}
		sfree((void *) &bmp180data->fd);
	}
	sfree((void *) &bmp180data);
	bmp180_threads--;

	return (void *) NULL;
}

struct threadqueue_t *bmp180InitDev(JsonNode *jdevice) {
	bmp180_loop = 1;
	wiringXSetup();
	char *output = json_stringify(jdevice, NULL);
	JsonNode *json = json_decode(output);
	sfree((void *) &output);

	struct protocol_threads_t *node = protocol_thread_init(bmp180, json);
	return threads_register("bmp180", &bmp180Parse, (void *) node, 0);
}

static void bmp180ThreadGC(void) {
	bmp180_loop = 0;
	protocol_thread_stop(bmp180);
	while (bmp180_threads > 0) {
		usleep(10);
	}
	protocol_thread_free(bmp180);
}

#ifndef MODULE
__attribute__((weak))
#endif
void bmp180Init(void) {
	pthread_mutexattr_init(&bmp180attr);
	pthread_mutexattr_settype(&bmp180attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&bmp180lock, &bmp180attr);

	protocol_register(&bmp180);
	protocol_set_id(bmp180, "bmp180");
	protocol_device_add(bmp180, "bmp180", "I2C Barometric Pressure and Temperature Sensor");
	protocol_device_add(bmp180, "bmp085", "I2C Barometric Pressure and Temperature Sensor");
	bmp180->devtype = WEATHER;
	bmp180->hwtype = SENSOR;

	options_add(&bmp180->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "0x[0-9a-f]{2}");
	options_add(&bmp180->options, 'o', "oversampling", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *) 1,
			"^[0123]$");
	options_add(&bmp180->options, 'p', "pressure", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, (void *) 0,
			"^[0-9]{1,3}$");
	options_add(&bmp180->options, 't', "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, (void *) 0,
			"^[0-9]{1,3}$");

	options_add(&bmp180->options, 0, "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *) 10,
			"[0-9]");
	options_add(&bmp180->options, 0, "pressure-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *) 0,
			"[0-9]");
	options_add(&bmp180->options, 0, "temperature-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *) 0,
			"[0-9]");
	options_add(&bmp180->options, 0, "decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1, "[0-9]");
	options_add(&bmp180->options, 0, "show-pressure", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1,
			"^[10]{1}$");
	options_add(&bmp180->options, 0, "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1,
			"^[10]{1}$");

	bmp180->initDev = &bmp180InitDev;
	bmp180->threadGC = &bmp180ThreadGC;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "bmp180";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = "84";
}

void init(void) {
	bmp180Init();
}
#endif
