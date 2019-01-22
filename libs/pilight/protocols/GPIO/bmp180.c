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
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <assert.h>
#include <sys/stat.h>
#ifndef _WIN32
	#include <unistd.h>
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
#endif

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../../core/eventpool.h"
#include "../../core/binary.h"
#include "../../core/json.h"
#include "../protocol.h"
#include "bmp180.h"

#if !defined(__FreeBSD__) && !defined(_WIN32) && !defined(__sun)
#include <wiringx.h>

#define STEP1		1
#define STEP2		2
#define STEP3		3

typedef struct data_t {
	char *name;

	unsigned int id;
	char path[PATH_MAX];
	int fd;

	uv_timer_t *interval_req;
	uv_timer_t *stepped_req;
	
	short ac1;
	short ac2;
	short ac3;
	short b1;
	short b2;
	short mb;
	short mc;
	short md;
	unsigned short ac4;
	unsigned short ac5;
	unsigned short ac6;
	unsigned int ut;
	unsigned int b4;
	unsigned int b6;
	unsigned int b7;
	unsigned int b5;
	int x1;
	int x2;
	int x3;
	int b3;

	int interval;
	int steps;
	int oversampling;
	
	double temp;
	double temp_offset;
	double pressure;
	double pressure_offset;

	struct data_t *next;
} data_t;

static struct data_t *data = NULL;

static int I2C2Int(int a) {
	return ((a << 8) & 0xFF00) + (a >> 8);
}

static int I2C2Int1(int a) {
	int b = I2C2Int(a);
	if(a == 0x0 || a == 0xFFFF) {
		return -1;
	}
	return b;
}

static void *reason_code_received_free(void *param) {
	struct reason_code_received_t *data = param;
	FREE(data);
	return NULL;
}

static void thread(uv_work_t *req) {
	struct data_t *settings = req->data;

	switch(settings->steps) {
		case STEP1: {
			wiringXI2CWriteReg8(settings->fd, 0xF4, 0x2E);
			settings->steps = STEP2;
			uv_timer_start(settings->stepped_req, (void (*)(uv_timer_t *))thread, 100, 0);
		} break;
		case STEP2: {
			settings->ut = I2C2Int(wiringXI2CReadReg16(settings->fd, 0xF6));

			settings->x1 = (((int)settings->ut - (int)settings->ac6)*(int)settings->ac5) >> 15;
			settings->x2 = ((int)settings->mc << 11)/(settings->x1 + settings->md);
			settings->b5 = settings->x1 + settings->x2;

			int rawTemperature = ((settings->b5 + 8) >> 4); 
			settings->temp = ((double)rawTemperature)/10;

			wiringXI2CWriteReg8(settings->fd, 0xF4, 0x34 + (settings->oversampling << 6));

			settings->steps = STEP3;

			uv_timer_start(settings->stepped_req, (void (*)(uv_timer_t *))thread, 500, 0);
		} break;
		case STEP3: {
			unsigned int msb = wiringXI2CReadReg8(settings->fd, 0xF6);
			unsigned int lsb = wiringXI2CReadReg8(settings->fd, 0xF7);
			unsigned int xlsb = wiringXI2CReadReg8(settings->fd, 0xF8);
			unsigned int up = (((unsigned int)msb << 16) | ((unsigned int)lsb << 8) | (unsigned int)xlsb) >> (8-settings->oversampling);

			settings->b6 = settings->b5 - 4000;
			settings->x1 = (settings->b2 * (settings->b6 * settings->b6) >> 12) >> 11;
			settings->x2 = (settings->ac2 * settings->b6) >> 11;
			settings->x3 = settings->x1 + settings->x2;
			settings->b3 = (((((int)settings->ac1) * 4 + settings->x3) << settings->oversampling) + 2) >> 2;

			settings->x1 = (settings->ac3 * settings->b6) >> 13;
			settings->x2 = (settings->b1 * ((settings->b6 * settings->b6) >> 12)) >> 16;
			settings->x3 = ((settings->x1 + settings->x2) + 2) >> 2;
			settings->b4 = (settings->ac4 * (unsigned long)(settings->x3 + 32768)) >> 15;

			settings->b7 = ((unsigned long)(up - settings->b3) * (50000 >> settings->oversampling));
			int p = 0;
			if(settings->b7 < 0x80000000) {
				p = (settings->b7 * 2) / settings->b4;
			} else {
				p = (settings->b7 / settings->b4) * 2;
			}
			settings->x1 = (p >> 8) * (p >> 8);
			settings->x1 = (settings->x1 * 3038) >> 16;
			settings->x2 = (-7357 * p) >> 16;
			p += (settings->x1 + settings->x2 + 3791) >> 4;

			settings->pressure = ((double)p)/100;

			struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
			if(data == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			snprintf(data->message, 1024,
				"{\"id\":%d,\"temperature\":%.2f,\"pressure\":%.2f}",
				settings->id, ((double)settings->temp + settings->temp_offset), ((double)settings->pressure + settings->pressure_offset)
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
		} break;
	}

	return;
}

static void restart(uv_work_t *req) {
	struct data_t *settings = req->data;
	settings->steps = STEP1;
	thread(req);
	return;
}

static void *addDevice(int reason, void *param, void *userdata) {
	struct JsonNode *jdevice = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct data_t *node = NULL;
	char *stmp = NULL;
	int match = 0, interval = 10;
	double itmp = 0.0;

	if(param == NULL) {
		return NULL;
	}

	if((jdevice = json_first_child(param)) == NULL) {
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
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(node, '\0', sizeof(struct data_t));
	node->oversampling = 3;
	node->interval = interval;
	node->steps = STEP1;

	match = 0;
	if((jid = json_find_member(jdevice, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_number(jchild, "id", &itmp) == 0) {
				node->id = (int)itmp;
				match++;
			}
			if(json_find_string(jchild, "i2c-path", &stmp) == 0) {
				strcpy(node->path, stmp);
				match++;
			}
			jchild = jchild->next;
		}
	}

	if(match != 2) {
		FREE(node);
		return NULL;
	}

	if(json_find_number(jdevice, "poll-interval", &itmp) == 0) {
		node->interval = (int)round(itmp);
	}

	json_find_number(jdevice, "temperature-offset", &node->temp_offset);
	json_find_number(jdevice, "pressure-offset", &node->pressure_offset);
	if(json_find_number(jdevice, "oversampling", &itmp) == 0) {
		node->oversampling = (unsigned char)itmp;
	}

	node->fd = wiringXI2CSetup(node->path, node->id);
	if(node->fd <= 0) {
		logprintf(LOG_ERR, "bmp180: error connecting to i2c bus: %s", node->path);
		FREE(node);
		return NULL;
	}

	// read 0xD0 to check chip id: must equal 0x55 for BMP085/180
	int id = wiringXI2CReadReg8(node->fd, 0xD0);
	if(id != 0x55) {
		logprintf(LOG_ERR, "bmp180: wrong device on i2c bus: %s", node->path);
		FREE(node);
		return (void *)NULL;
	}

	// read 0xD1 to check chip version: must equal 0x01 for BMP085 or 0x02 for BMP180
	int version = wiringXI2CReadReg8(node->fd, 0xD1);
	if(version != 0x01 && version != 0x02) {
		logprintf(LOG_ERR, "bmp180: wrong device version on i2c bus: %s", node->path);
		FREE(node);
		return (void *)NULL;
	}

	node->ac1 = I2C2Int1(wiringXI2CReadReg16(node->fd, 0xAA));
	node->ac2 = I2C2Int1(wiringXI2CReadReg16(node->fd, 0xAC));
	node->ac3 = I2C2Int1(wiringXI2CReadReg16(node->fd, 0xAE));
	node->ac4 = I2C2Int1(wiringXI2CReadReg16(node->fd, 0xB0));
	node->ac5 = I2C2Int1(wiringXI2CReadReg16(node->fd, 0xB2));
	node->ac6 = I2C2Int1(wiringXI2CReadReg16(node->fd, 0xB4));

	node->b1 = I2C2Int1(wiringXI2CReadReg16(node->fd, 0xB6));
	node->b2 = I2C2Int1(wiringXI2CReadReg16(node->fd, 0xB8));

	node->mb = I2C2Int1(wiringXI2CReadReg16(node->fd, 0xBA));
	node->mc = I2C2Int1(wiringXI2CReadReg16(node->fd, 0xBC));
	node->md = I2C2Int1(wiringXI2CReadReg16(node->fd, 0xBE));

	if(node->ac1 == -1 || node->ac2 == -1 || node->ac3 == -1 || node->ac4 == -1 ||
	   node->ac5 == -1 || node->ac6 == -1 || node->b1 == -1 || node->b2 == -1 ||
		 node->mb == -1 || node->mc == -1 || node->md == -1) {
		logprintf(LOG_ERR, "bmp180: communication error on i2c bus: %s", node->path);
		FREE(node);
		return NULL;
	}

	if((node->name = MALLOC(strlen(jdevice->key)+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(node->name, jdevice->key);

	node->next = data;
	data = node;

	if((node->interval_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}	

	if((node->stepped_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	node->interval_req->data = node;
	uv_timer_init(uv_default_loop(), node->interval_req);
	assert(node->interval > 0);
	uv_timer_start(node->interval_req, (void (*)(uv_timer_t *))restart, node->interval*1000, node->interval*1000);

	node->stepped_req->data = node;
	uv_timer_init(uv_default_loop(), node->stepped_req);

	return NULL;
}

static void gc(void) {
	struct data_t *tmp = NULL;
	while(data) {
		tmp = data;
		if(data->name != NULL) {
			FREE(data->name);
		}
		if(data->fd > 0) {
			close(data->fd);
		}
		data = data->next;
		FREE(tmp);
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

	options_add(&bmp180->options, "i", "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([1-9]|1[1-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
	options_add(&bmp180->options, "o", "oversampling", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *) 3, "^[0123]$");
	options_add(&bmp180->options, "p", "pressure", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, (void *) 0, "^[0-9]{1,3}$");
	options_add(&bmp180->options, "t", "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, (void *) 0, "^[0-9]{1,3}$");
	options_add(&bmp180->options, "d", "i2c-path", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "^/dev/i2c-[0-9]{1,2}%");

	options_add(&bmp180->options, "0", "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *) 10, "[0-9]");
	options_add(&bmp180->options, "0", "pressure-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *) 0, "[0-9]");
	options_add(&bmp180->options, "0", "temperature-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *) 0, "[0-9]");
	options_add(&bmp180->options, "0", "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1, "[0-9]");
	options_add(&bmp180->options, "0", "humidity-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1, "[0-9]");
	options_add(&bmp180->options, "0", "pressure-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1, "[0-9]");
	options_add(&bmp180->options, "0", "show-pressure", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1, "^[10]{1}$");
	options_add(&bmp180->options, "0", "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1, "^[10]{1}$");

#if !defined(__FreeBSD__) && !defined(_WIN32) && !defined(__sun)
	bmp180->gc = &gc;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice, NULL);
#endif
}

#if defined(MODULE) && !defined(_WIN32) && !defined(__sun)
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
