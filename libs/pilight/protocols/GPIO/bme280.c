/*
	Copyright (C) CurlyMo and MaGallant

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
#include "bme280.h"

#if !defined(__FreeBSD__) && !defined(_WIN32) && !defined(__sun)
#include <wiringx.h>

#define STEP1		1
#define STEP2		2

typedef struct data_t {
	char *name;

	unsigned int id;
	char path[PATH_MAX];
	int fd;

	uv_timer_t *interval_req;
	uv_timer_t *stepped_req;

	// calibration values (stored in each BME280)
	uint32_t dig_t1;
	int32_t dig_t2;
	int32_t dig_t3;
	uint32_t dig_p1;
	int32_t dig_p2;
	int32_t dig_p3;
	int32_t dig_p4;
	int32_t dig_p5;
	int32_t dig_p6;
	int32_t dig_p7;
	int32_t dig_p8;
	int32_t dig_p9;
	uint8_t dig_h1;
	int32_t dig_h2;
	uint8_t dig_h3;
	int32_t dig_h4;
	int32_t dig_h5;
	int8_t dig_h6;

	int x1;
	int x2;
	int x3;
	int b3;

	int interval;
	int steps;
	uint8_t tem_oversampling;
	uint8_t hum_oversampling;
	uint8_t pre_oversampling;

	double temp;
	double temp_offset;
	double pressure;
	double pressure_offset;
	double humidity;
	double humidity_offset;

	struct data_t *next;
} data_t;

static struct data_t *data = NULL;

static void *reason_code_received_free(void *param) {
	struct reason_code_received_t *data = param;
	FREE(data);
	return NULL;
}

static void thread(uv_work_t *req) {
	struct data_t *settings = req->data;

	switch(settings->steps) {
		case STEP1: {
			wiringXI2CWriteReg8(settings->fd, 0xF2, settings->hum_oversampling);
			wiringXI2CWriteReg8(settings->fd, 0xF4,(settings->tem_oversampling<<5 | settings->pre_oversampling<<2 | 0x01));
			settings->steps = STEP2;
			uv_timer_start(settings->stepped_req, (void (*)(uv_timer_t *))thread, 100, 0);
		} break;
		case STEP2: {
			// read the eight bytes result from address 0xF7.
			wiringXI2CWrite(settings->fd, 0xF7);
			//ut = (unsigned short) readReg16(settings->fd[y], 0xF7);
			uint8_t pmsb = wiringXI2CRead(settings->fd);
			uint8_t plsb = wiringXI2CRead(settings->fd);
			uint8_t pxsb = wiringXI2CRead(settings->fd);
			uint8_t tmsb = wiringXI2CRead(settings->fd);
			uint8_t tlsb = wiringXI2CRead(settings->fd);
			uint8_t txsb = wiringXI2CRead(settings->fd);
			uint8_t hmsb = wiringXI2CRead(settings->fd);
			uint8_t hlsb = wiringXI2CRead(settings->fd);

			uint32_t raw_temperature = 0;
			raw_temperature = (raw_temperature | tmsb) << 8;
			raw_temperature = (raw_temperature | tlsb) << 8;
			raw_temperature = (raw_temperature | txsb) >> 4;

			uint32_t raw_pressure = 0;
			raw_pressure = (raw_pressure | pmsb) << 8;
			raw_pressure = (raw_pressure | plsb) << 8;
			raw_pressure = (raw_pressure | pxsb) >> 4;

			uint32_t raw_humidity = 0;
			raw_humidity = (raw_humidity | hmsb) << 8;
			raw_humidity = (raw_humidity | hlsb);

			int32_t vart1 = ((((raw_temperature>>3) - ((int32_t)settings->dig_t1 <<1))) *
				((int32_t)settings->dig_t2)) >> 11;
			int32_t vart2 = (((((raw_temperature>>4) - ((int32_t)settings->dig_t1)) *
				((raw_temperature>>4) - ((int32_t)settings->dig_t1))) >> 12) *
				((int32_t)settings->dig_t3)) >> 14;
			int32_t cal_temperature = vart1 + vart2;

			settings->temp = (double) ((cal_temperature * 5 + 128) >> 8);
			settings->temp = settings->temp/100;

			settings->pressure = 0;
			long long var1 = ((long long)cal_temperature) - 128000;
			long long var2 = var1 * var1 * (long long)settings->dig_p6;
			var2 = var2 + ((var1 * (long long)settings->dig_p5) << 17);
			var2 = var2 + (((long long)settings->dig_p4) << 35);
			var1 = ((var1 * var1 * (long long)settings->dig_p3) >> 8) + ((var1 * (long long)settings->dig_p2) << 12);
			var1 = (((((long long)1) << 47) + var1)) * ((long long)settings->dig_p1) >> 33;
			if (var1 != 0) {
				long long p = 1048576 - raw_pressure;
				p = (((p << 31) - var2) * 3125) / var1;
				var1 = (((long long)settings->dig_p9) * (p >> 13) * (p>>13)) >> 25;
				var2 = (((long long)settings->dig_p8) * p) >> 19;
				p = ((p + var1 + var2) >> 8) + (((long long)settings->dig_p7)<<4);
				settings->pressure = (double)p / 256;
			}

			settings->humidity = 0;
			int v_x1_u32r = (cal_temperature - ((int)76800));
			v_x1_u32r = (((((raw_humidity << 14) - (((int32_t)settings->dig_h4) << 20) -
				(((int32_t)settings->dig_h5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) *
				(((((((v_x1_u32r * ((int32_t)settings->dig_h6)) >> 10) *
				(((v_x1_u32r * ((int32_t)settings->dig_h3)) >> 11) + ((int32_t)32768))) >> 10) +
				((int32_t)2097152)) * ((int32_t)settings->dig_h2) + 8192) >> 14));

			v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
				((int32_t)settings->dig_h1)) >> 4));

			v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
			v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
			settings->humidity = ((double)(v_x1_u32r>>12))/1024.0;

			struct reason_code_received_t *data = MALLOC(sizeof(struct reason_code_received_t));
			if(data == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			snprintf(data->message, 1024,
				"{\"id\":%d,\"temperature\":%.2f,\"pressure\":%.2f,\"humidity\":%.2f}",
				settings->id, ((double)settings->temp + settings->temp_offset),
				((double)settings->pressure + settings->pressure_offset),
				((double)settings->humidity)
			);
			strcpy(data->origin, "receiver");
			data->protocol = bme280->id;
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
			if(strcmp(bme280->id, jchild->string_) == 0) {
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
	node->tem_oversampling = 1;
	node->hum_oversampling = 1;
	node->pre_oversampling = 1;
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
	if(json_find_number(jdevice, "tem-oversampling", &itmp) == 0) {
		node->tem_oversampling = (unsigned char)itmp;
	}
	if(json_find_number(jdevice, "hum-oversampling", &itmp) == 0) {
		node->hum_oversampling = (unsigned char)itmp;
	}
	if(json_find_number(jdevice, "pre-oversampling", &itmp) == 0) {
		node->pre_oversampling = (unsigned char)itmp;
	}

	node->fd = wiringXI2CSetup(node->path, node->id);
	if(node->fd <= 0) {
		logprintf(LOG_ERR, "bme280: error connecting to i2c bus: %s", node->path);
		FREE(node);
		return NULL;
	}

	// read 0xD0 to check chip id: must equal 0x60 for BME280
	int id = wiringXI2CReadReg8(node->fd, 0xD0);
	if(id != 0x60) {
		logprintf(LOG_ERR, "bme280: wrong device on i2c bus: %s", node->path);
		FREE(node);
		return (void *)NULL;
	}

	// read calibration coefficients from register addresses
	node->dig_t1 = (uint16_t) wiringXI2CReadReg16(node->fd, 0x88);
	node->dig_t2 = (int16_t) wiringXI2CReadReg16(node->fd, 0x8A);
	node->dig_t3 = (int16_t) wiringXI2CReadReg16(node->fd, 0x8C);

	node->dig_p1 = (uint16_t) wiringXI2CReadReg16(node->fd, 0x8E);
	node->dig_p2 = (int16_t) wiringXI2CReadReg16(node->fd, 0x90);
	node->dig_p3 = (int16_t) wiringXI2CReadReg16(node->fd, 0x92);
	node->dig_p4 = (int16_t) wiringXI2CReadReg16(node->fd, 0x94);
	node->dig_p5 = (int16_t) wiringXI2CReadReg16(node->fd, 0x96);
	node->dig_p6 = (int16_t) wiringXI2CReadReg16(node->fd, 0x98);
	node->dig_p7 = (int16_t) wiringXI2CReadReg16(node->fd, 0x9A);
	node->dig_p8 = (int16_t) wiringXI2CReadReg16(node->fd, 0x9C);
	node->dig_p9 = (int16_t) wiringXI2CReadReg16(node->fd, 0x9E);

	node->dig_h1 = (uint8_t) wiringXI2CReadReg8(node->fd, 0xA1);
	node->dig_h2 = (int16_t) wiringXI2CReadReg16(node->fd, 0xE1);
	node->dig_h3 = (uint8_t) wiringXI2CReadReg8(node->fd, 0xE3);
	node->dig_h4 = (int16_t) (wiringXI2CReadReg8(node->fd, 0xE4) << 4) | (wiringXI2CReadReg8(node->fd, 0xE5) & 0xF);
	node->dig_h5 = (int16_t) (wiringXI2CReadReg8(node->fd, 0xE6) << 4) | (wiringXI2CReadReg8(node->fd, 0xE5) >> 4);
	node->dig_h6 = (int8_t) wiringXI2CReadReg8(node->fd, 0xE7);

	// check communication: no result must equal 0 or 0xFFFF (=65535)
	if (node->dig_t1 == 0 || node->dig_t1 == 0xFFFF
		|| node->dig_t2 == 0 || node->dig_t2 == 0xFFFF
		|| node->dig_t3 == 0 || node->dig_t3 == 0xFFFF
		|| node->dig_p1 == 0 || node->dig_p1 == 0xFFFF
		|| node->dig_p2 == 0 || node->dig_p2 == 0xFFFF
		|| node->dig_p3 == 0 || node->dig_p3 == 0xFFFF
		|| node->dig_p4 == 0 || node->dig_p4 == 0xFFFF
		|| node->dig_p5 == 0 || node->dig_p5 == 0xFFFF
		|| node->dig_p6 == 0 || node->dig_p6 == 0xFFFF
		|| node->dig_p7 == 0 || node->dig_p7 == 0xFFFF
		|| node->dig_p8 == 0 || node->dig_p8 == 0xFFFF
		|| node->dig_p9 == 0 || node->dig_p9 == 0xFFFF
		|| node->dig_h1 == 0 || node->dig_h1 == 0xFF
		|| node->dig_h2 == 0 || node->dig_h2 == 0xFFFF
		|| node->dig_h4 == 0 || node->dig_h4 == 0xFFFF
		|| node->dig_h5 == 0 || node->dig_h5 == 0xFFFF
		|| node->dig_h6 == 0 || node->dig_h6 == 0xFF )  {
		logprintf(LOG_ERR, "bme280: communication error on i2c bus: %s", node->path);
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
void bme280Init(void) {
	protocol_register(&bme280);
	protocol_set_id(bme280, "bme280");
	protocol_device_add(bme280, "bme280", "I2C Barometric Pressure and Temperature Sensor");
	bme280->devtype = WEATHER;
	bme280->hwtype = SENSOR;
	bme280->multipleId = 0;

	options_add(&bme280->options, "i", "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "0x[0-9a-f]{2}");
	options_add(&bme280->options, "p", "pressure", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, (void *) 0, "^[0-9]{1,3}$");
	options_add(&bme280->options, "t", "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, (void *) 0, "^[0-9]{1,3}$");
	options_add(&bme280->options, "h", "humidity", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, (void *) 0, "^[0-9]{1,3}$");
	options_add(&bme280->options, "d", "i2c-path", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "^/dev/i2c-[0-9]{1,2}$");

	options_add(&bme280->options, "0", "hum-oversampling", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *) 1, "^[0123]$");
	options_add(&bme280->options, "0", "tem-oversampling", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *) 1, "^[0123]$");
	options_add(&bme280->options, "0", "pre-oversampling", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *) 1, "^[0123]$");
	options_add(&bme280->options, "0", "poll-interval", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *) 10, "[0-9]");
	options_add(&bme280->options, "0", "pressure-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *) 0, "[0-9]");
	options_add(&bme280->options, "0", "temperature-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *) 0, "[0-9]");
	options_add(&bme280->options, "0", "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1, "[0-9]");
	options_add(&bme280->options, "0", "humidity-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1, "[0-9]");
	options_add(&bme280->options, "0", "pressure-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1, "[0-9]");
	options_add(&bme280->options, "0", "show-pressure", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1, "^[10]{1}$");
	options_add(&bme280->options, "0", "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *) 1, "^[10]{1}$");
	options_add(&bme280->options, "0", "show-humidity", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

#if !defined(__FreeBSD__) && !defined(_WIN32) && !defined(__sun)
	bme280->gc = &gc;

	eventpool_callback(REASON_DEVICE_ADDED, addDevice, NULL);
#endif
}

#if defined(MODULE) && !defined(_WIN32) && !defined(__sun)
void compatibility(struct module_t *module) {
	module->name = "bme280";
	module->version = "0.1";
	module->reqversion = "1.0";
	module->reqcommit = "1";
}

void init(void) {
	bme280Init();
}
#endif
