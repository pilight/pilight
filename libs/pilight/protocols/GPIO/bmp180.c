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
#ifndef _WIN32
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
#endif
#include <pthread.h>

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../../core/threadpool.h"
#include "../../core/eventpool.h"
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "../../core/json.h"
#include "../protocol.h"
#include "bmp180.h"

#if !defined(__FreeBSD__) && !defined(_WIN32)
#include "../../../wiringx/wiringX.h"

#define STEP1		1
#define STEP2		2
#define STEP3		3

typedef struct data_t {
	char *name;
	unsigned int id;
	int fd;
	// calibration values (stored in each BMP180/085)
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

	int x1;
	int x2;
	int temp;
	unsigned int b5;
	unsigned short up;

	unsigned char oversampling;

	int steps;
	int interval;

	double temp_offset;
	double pressure_offset;

	struct data_t *next;
} data_t;

static struct data_t *data = NULL;

static void *reason_code_received_free(void *param) {
	struct reason_code_received_t *data = param;
	FREE(data);
	return NULL;
}

// helper function with built-in result conversion
static int readReg16(int fd, int reg) {
	int res = wiringXI2CReadReg16(fd, reg);
	// convert result to 16 bits and swap bytes
	return ((res << 8) & 0xFF00) | ((res >> 8) & 0xFF);
}

static void *thread(void *param) {
	struct threadpool_tasks_t *task = param;
	struct data_t *settings = task->userdata;

	switch(settings->steps) {
		case STEP1: {
			// write 0x2E into Register 0xF4 to request a temperature reading.
			wiringXI2CWriteReg8(settings->fd, 0xF4, 0x2E);

			// wait at least 4.5ms: we suspend execution for 5000 microseconds.
			settings->steps = STEP2;

			struct timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = 5000;
			threadpool_add_scheduled_work(bmp180->id, thread, tv, (void *)task);
		} break;
		case STEP2: {
			// read the two byte result from address 0xF6.
			unsigned short ut = (unsigned short)readReg16(settings->fd, 0xF6);
			int delay = 0;

			// calculate temperature (in units of 0.1 deg C) given uncompensated value
			settings->x1 = (((int)ut - (int)settings->ac6)) * (int)settings->ac5 >> 15;
			settings->x2 = ((int)settings->mc << 11) / (settings->x1 + settings->md);
			settings->b5 = settings->x1 + settings->x2;
			settings->temp = ((settings->b5 + 8) >> 4);

			// uncompensated pressure value
			settings->up = 0;

			// write 0x34+(BMP085_OVERSAMPLING_SETTING<<6) into register 0xF4
			// request a pressure reading with specified oversampling setting
			wiringXI2CWriteReg8(settings->fd, 0xF4, 0x34 + (settings->oversampling << 6));

			// wait for conversion, delay time dependent on oversampling setting
			delay = (unsigned int)((2 + (3 << settings->oversampling)) * 1000);

			settings->steps = STEP3;

			struct timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = delay;
			threadpool_add_scheduled_work(bmp180->id, thread, tv, (void *)task);
		} break;
		case STEP3: {
			// read the three byte result (block data): 0xF6 = MSB, 0xF7 = LSB and 0xF8 = XLSB
			int msb = wiringXI2CReadReg8(settings->fd, 0xF6);
			int lsb = wiringXI2CReadReg8(settings->fd, 0xF7);
			int xlsb = wiringXI2CReadReg8(settings->fd, 0xF8);
			settings->up = (((unsigned int)msb << 16) | ((unsigned int)lsb << 8) | (unsigned int)xlsb) >> (8 - settings->oversampling);

			// calculate B6
			int b6 = settings->b5 - 4000;

			// calculate B3
			settings->x1 = (settings->b2 * (b6 * b6) >> 12) >> 11;
			settings->x2 = (settings->ac2 * b6) >> 11;
			int x3 = settings->x1 + settings->x2;
			int b3 = (((settings->ac1 * 4 + x3) << settings->oversampling) + 2) >> 2;

			// calculate B4
			settings->x1 = (settings->ac3 * b6) >> 13;
			settings->x2 = (settings->b1 * ((b6 * b6) >> 12)) >> 16;
			x3 = ((settings->x1 + settings->x2) + 2) >> 2;
			unsigned int b4 = (settings->ac4 * (unsigned int) (x3 + 32768)) >> 15;

			// calculate B7
			unsigned int b7 = ((settings->up - (unsigned int)b3) * ((unsigned int)50000 >> settings->oversampling));

			// calculate pressure in Pa
			int pressure = b7 < 0x80000000 ? (int) ((b7 << 1) / b4) : (int) ((b7 / b4) << 1);
			settings->x1 = (pressure >> 8) * (pressure >> 8);
			settings->x1 = (settings->x1 * 3038) >> 16;
			settings->x2 = (-7357 * pressure) >> 16;
			pressure += (settings->x1 + settings->x2 + 3791) >> 4;

			struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
			if(data == NULL) {
				OUT_OF_MEMORY
			}
			snprintf(data->message, 1024,
				"{\"id\":%d,\"temperature\":%.1f,\"pressure\":%.1f}",
				settings->id, (((double)settings->temp / 10) + settings->temp_offset), (((double)pressure / 100) + settings->pressure_offset)
			);
			strcpy(data->origin, "receiver");
			data->protocol = bmp180->id;
			if(strlen(pilight_uuid) > 0) {
				data->uuid = pilight_uuid;
			} else {
				data->uuid = NULL;
			}
			data->repeat = 1;
			eventpool_trigger(REASON_CODE_RECEIVED, reason_code_received_free, data);

			struct timeval tv;
			tv.tv_sec = settings->interval;
			tv.tv_usec = 0;
			threadpool_add_scheduled_work(settings->name, thread, tv, (void *)settings);
		} break;
	}

	return (void *)NULL;
}

static void *addDevice(void *param) {
	struct threadpool_tasks_t *task = param;
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct data_t *node = NULL;
	struct timeval tv;
	int match = 0, interval = 10;
	double itmp = 0.0;

	if(task->userdata == NULL) {
		return NULL;
	}

	if((jdevice = json_first_child(task->userdata)) == NULL) {
		return NULL;
	}

	if((jprotocols = json_find_member(jdevice, "protocol")) != NULL) {
		jchild = json_first_child(jprotocols);
		while(jchild) {
			if(strcmp(bmp180->id, jchild->string_) == 0) {
				match = 1;
				break;
		}
			jchild = jchild->next;
		}
	}

	if(match == 0) {
		return NULL;
	}

	if((node = MALLOC(sizeof(struct data_t))) == NULL) {
		OUT_OF_MEMORY
	}
	memset(node, '\0', sizeof(struct data_t));
	node->id = 0;
	node->fd = 0;
	// calibration values (stored in each BMP180/085)
	node->ac1 = 0;
	node->ac2 = 0;
	node->ac3 = 0;
	node->ac4 = 0;
	node->ac5 = 0;
	node->ac6 = 0;
	node->b1 = 0;
	node->b2 = 0;
	node->mb = 0;
	node->mc = 0;
	node->md = 0;
	node->x1 = 0;
	node->x2 = 0;
	node->temp = 0;
	node->b5 = 0;
	node->up = 0;
	node->steps = STEP1;
	node->oversampling = 0;
	node->temp_offset = 0.0;
	node->pressure_offset = 0.0;

	if((jid = json_find_member(jdevice, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_number(jchild, "id", &itmp) == 0) {
				node->id = (int)itmp;
			}
			jchild = jchild->next;
		}
	}

	if(json_find_number(jdevice, "poll-interval", &itmp) == 0) {
		interval = (int)round(itmp);
	}

	json_find_number(jdevice, "temperature-offset", &node->temp_offset);
	json_find_number(jdevice, "pressure-offset", &node->pressure_offset);
	if(json_find_number(jdevice, "oversampling", &itmp) == 0) {
		node->oversampling = (unsigned char)itmp;
	}

	node->fd = wiringXI2CSetup(node->id);
	if(node->fd <= 0) {
		logprintf(LOG_ERR, "%s could not open I2C device %02x", jdevice->key, node->id);
		FREE(node);
		return (void *)NULL;
	}

// read 0xD0 to check chip id: must equal 0x55 for BMP085/180
	int id = wiringXI2CReadReg8(node->fd, 0xD0);
	if(id != 0x55) {
		logprintf(LOG_ERR, "%s detected a wrong device %02x on I2C line %02x", jdevice->key, id, node->id);
		FREE(node);
		return (void *)NULL;
	}

	// read 0xD1 to check chip version: must equal 0x01 for BMP085 or 0x02 for BMP180
	int version = wiringXI2CReadReg8(node->fd, 0xD1);
	if(version != 0x01 && version != 0x02) {
		logprintf(LOG_ERR, "%s detected a wrong version %02x on I2C line %02x", jdevice->key, version, node->id);
		FREE(node);
		return (void *)NULL;
	}

	// read calibration coefficients from register addresses
	if((node->ac1 = (short)readReg16(node->fd, 0xAA)) == 0x0 || node->ac1 == 0xFFFF) {
		logprintf(LOG_ERR, "%s encountered an error while communicating on I2C line %02x", jdevice->key, node->id);
		FREE(node);
		return (void *)NULL;
	}
	if((node->ac2 = (short)readReg16(node->fd, 0xAC)) == 0x0 || node->ac1 == 0xFFFF) {
		logprintf(LOG_ERR, "%s encountered an error while communicating on I2C line %02x", jdevice->key, node->id);
		FREE(node);
		return (void *)NULL;
	}
	if((node->ac3 = (short)readReg16(node->fd, 0xAE)) == 0x0 || node->ac1 == 0xFFFF) {
		logprintf(LOG_ERR, "%s encountered an error while communicating on I2C line %02x", jdevice->key, node->id);
		FREE(node);
		return (void *)NULL;
	}
	if((node->ac4 = (unsigned short)readReg16(node->fd, 0xB0)) == 0x0 || node->ac1 == 0xFFFF) {
		logprintf(LOG_ERR, "%s encountered an error while communicating on I2C line %02x", jdevice->key, node->id);
		FREE(node);
		return (void *)NULL;
	}
	if((node->ac5 = (unsigned short)readReg16(node->fd, 0xB2)) == 0x0 || node->ac1 == 0xFFFF) {
		logprintf(LOG_ERR, "%s encountered an error while communicating on I2C line %02x", jdevice->key, node->id);
		FREE(node);
		return (void *)NULL;
	}
	if((node->ac6 = (unsigned short)readReg16(node->fd, 0xB4)) == 0x0 || node->ac1 == 0xFFFF) {
		logprintf(LOG_ERR, "%s encountered an error while communicating on I2C line %02x", jdevice->key, node->id);
		FREE(node);
		return (void *)NULL;
	}
	if((node->b1 = (short)readReg16(node->fd, 0xB6)) == 0x0 || node->ac1 == 0xFFFF) {
		logprintf(LOG_ERR, "%s encountered an error while communicating on I2C line %02x", jdevice->key, node->id);
		FREE(node);
		return (void *)NULL;
	}
	if((node->b2 = (short)readReg16(node->fd, 0xB8)) == 0x0 || node->ac1 == 0xFFFF) {
		logprintf(LOG_ERR, "%s encountered an error while communicating on I2C line %02x", jdevice->key, node->id);
		FREE(node);
		return (void *)NULL;
	}
	if((node->mb = (short)readReg16(node->fd, 0xBA)) == 0x0 || node->ac1 == 0xFFFF) {
		logprintf(LOG_ERR, "%s encountered an error while communicating on I2C line %02x", jdevice->key, node->id);
		FREE(node);
		return (void *)NULL;
	}
	if((node->mc = (short)readReg16(node->fd, 0xBC)) == 0x0 || node->ac1 == 0xFFFF) {
		logprintf(LOG_ERR, "%s encountered an error while communicating on I2C line %02x", jdevice->key, node->id);
		FREE(node);
		return (void *)NULL;
	}
	if((node->md = (short)readReg16(node->fd, 0xBE)) == 0x0 || node->ac1 == 0xFFFF) {
		logprintf(LOG_ERR, "%s encountered an error while communicating on I2C line %02x", jdevice->key, node->id);
		FREE(node);
		return (void *)NULL;
	}

	if((node->name = MALLOC(strlen(jdevice->key)+1)) == NULL) {
		OUT_OF_MEMORY
	}
	strcpy(node->name, jdevice->key);

	node->interval = interval;

	node->next = data;
	data = node;

	tv.tv_sec = interval;
	tv.tv_usec = 0;
	threadpool_add_scheduled_work(jdevice->key, thread, tv, (void *)node);

	return NULL;
}

static void gc(void) {
	struct data_t *tmp = NULL;
	while(data) {
		tmp = data;
		FREE(tmp->name);
		data = data->next;
		FREE(tmp);
	}
	if(data != NULL) {
		FREE(data);
	}
}
#endif

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void bmp180Init(void) {
	protocol_register(&bmp180);
	protocol_set_id(bmp180, "bmp180");
	protocol_device_add(bmp180, "bmp180", "I2C Barometric Pressure and Temperature Sensor");
	protocol_device_add(bmp180, "bmp085", "I2C Barometric Pressure and Temperature Sensor");
	bmp180->devtype = WEATHER;
	bmp180->hwtype = SENSOR;
	bmp180->multipleId = 0;

	options_add(&bmp180->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([1-9]|1[1-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
	options_add(&bmp180->options, 'o', "oversampling", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *) 1, "^[0123]$");
	options_add(&bmp180->options, 'p', "pressure", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, (void *) 0, "^[0-9]{1,3}$");
	options_add(&bmp180->options, 't', "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, (void *) 0, "^[0-9]{1,3}$");

	options_add(&bmp180->options, 0, "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *) 10, "[0-9]");
	options_add(&bmp180->options, 0, "pressure-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *) 0, "[0-9]");
	options_add(&bmp180->options, 0, "temperature-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *) 0, "[0-9]");
	options_add(&bmp180->options, 0, "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1, "[0-9]");
	options_add(&bmp180->options, 0, "humidity-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1, "[0-9]");
	options_add(&bmp180->options, 0, "pressure-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1, "[0-9]");
	options_add(&bmp180->options, 0, "show-pressure", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1, "^[10]{1}$");
	options_add(&bmp180->options, 0, "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1, "^[10]{1}$");

#if !defined(__FreeBSD__) && !defined(_WIN32)
	// bmp180->initDev = &initDev;
	bmp180->gc = &gc;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice);
#endif
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "bmp180";
	module->version = "3.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	bmp180Init();
}
#endif
