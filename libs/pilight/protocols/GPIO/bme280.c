/*
	Copyright (C) CurlyMo
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
	#include <wiringx.h>
#endif
#include <pthread.h>
#include <assert.h>

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../../core/threads.h"
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "../../core/json.h"
#include "../../config/settings.h"
#include "../protocol.h"
#include "bme280.h"

#if !defined(__FreeBSD__) && !defined(_WIN32)

typedef struct settings_t {
	char **id;
	int nrid;
	char path[PATH_MAX];
	int *fd;
	// calibration values (stored in each BME280)
	uint32_t *dig_t1;
	int32_t *dig_t2;
	int32_t *dig_t3;
	uint32_t *dig_p1;
	int32_t *dig_p2;
	int32_t *dig_p3;
	int32_t *dig_p4;
	int32_t *dig_p5;
	int32_t *dig_p6;
	int32_t *dig_p7;
	int32_t *dig_p8;
	int32_t *dig_p9;
	uint8_t *dig_h1;
	int32_t *dig_h2;
	uint8_t *dig_h3;
	int32_t *dig_h4;
	int32_t *dig_h5;
	int8_t *dig_h6;
} settings_t;

static unsigned short loop = 1;
static int threads = 0;

static pthread_mutex_t lock;
static pthread_mutexattr_t attr;

static void *thread(void *param) {
	struct protocol_threads_t *node = (struct protocol_threads_t *) param;
	struct JsonNode *json = (struct JsonNode *) node->param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct settings_t *bme280data = MALLOC(sizeof(struct settings_t));
	int y = 0, interval = 10, nrloops = 0;
	char *stmp = NULL;
	double itmp = -1, temp_offset = 0, pressure_offset = 0;
	uint8_t hum_oversampling = 1;
        uint8_t tem_oversampling = 1;
        uint8_t pre_oversampling = 1;

	if(bme280data == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}

	bme280data->nrid = 0;
	bme280data->id = NULL;
	bme280data->fd = 0;
	bme280data->dig_t1 = 0;
	bme280data->dig_t2 = 0;
	bme280data->dig_t3 = 0;
	bme280data->dig_p1 = 0;
	bme280data->dig_p2 = 0;
	bme280data->dig_p3 = 0;
	bme280data->dig_p4 = 0;
	bme280data->dig_p5 = 0;
	bme280data->dig_p6 = 0;
	bme280data->dig_p7 = 0;
	bme280data->dig_p8 = 0;
	bme280data->dig_p9 = 0;
	bme280data->dig_h1 = 0;
	bme280data->dig_h2 = 0;
	bme280data->dig_h3 = 0;
	bme280data->dig_h4 = 0;
	bme280data->dig_h5 = 0;
	bme280data->dig_h6 = 0;

	threads++;

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_string(jchild, "id", &stmp) == 0) {
				if((bme280data->id = REALLOC(bme280data->id, (sizeof(char *) * (size_t)(bme280data->nrid + 1)))) == NULL) {
					fprintf(stderr, "out of memory\n");
					exit(EXIT_FAILURE);
				}
				if((bme280data->id[bme280data->nrid] = MALLOC(strlen(stmp) + 1)) == NULL) {
					fprintf(stderr, "out of memory\n");
					exit(EXIT_FAILURE);
				}
				strcpy(bme280data->id[bme280data->nrid], stmp);
				bme280data->nrid++;
			}
			if(json_find_string(jchild, "i2c-path", &stmp) == 0) {
				strcpy(bme280data->path, stmp);
			}
			jchild = jchild->next;
		}
	}

	if(json_find_number(json, "poll-interval", &itmp) == 0)
		interval = (int) round(itmp);
	json_find_number(json, "temperature-offset", &temp_offset);
	json_find_number(json, "pressure-offset", &pressure_offset);
	if(json_find_number(json, "hum-oversampling", &itmp) == 0) {
		hum_oversampling = (uint8_t) itmp;
	}
	hum_oversampling &= 0x7;
        if(json_find_number(json, "tem-oversampling", &itmp) == 0) {
		tem_oversampling = (uint8_t) itmp;
        }
        tem_oversampling &= 0x7;

        if(json_find_number(json, "pre-oversampling", &itmp) == 0) {
		pre_oversampling = (uint8_t) itmp;
        }
        pre_oversampling &= 0x7;

	// resize the memory blocks pointed to by the different pointers
	size_t sz = (size_t) (bme280data->nrid + 1);
	unsigned long int sizeUChar = sizeof(uint8_t) * sz;
        unsigned long int sizeChar = sizeof(int8_t) * sz;
	unsigned long int sizeShort = sizeof(int32_t) * sz;
	unsigned long int sizeUShort = sizeof(uint32_t) * sz;
	bme280data->fd = REALLOC(bme280data->fd, (sizeof(int) * sz));
	bme280data->dig_t1 = REALLOC(bme280data->dig_t1, sizeUShort);
	bme280data->dig_t2 = REALLOC(bme280data->dig_t2, sizeShort);
	bme280data->dig_t3 = REALLOC(bme280data->dig_t3, sizeShort);
	bme280data->dig_p1 = REALLOC(bme280data->dig_p1, sizeUShort);
	bme280data->dig_p2 = REALLOC(bme280data->dig_p2, sizeShort);
	bme280data->dig_p3 = REALLOC(bme280data->dig_p3, sizeShort);
        bme280data->dig_p4 = REALLOC(bme280data->dig_p4, sizeShort);
        bme280data->dig_p5 = REALLOC(bme280data->dig_p5, sizeShort);
        bme280data->dig_p6 = REALLOC(bme280data->dig_p6, sizeShort);
        bme280data->dig_p7 = REALLOC(bme280data->dig_p7, sizeShort);
        bme280data->dig_p8 = REALLOC(bme280data->dig_p8, sizeShort);
        bme280data->dig_p9 = REALLOC(bme280data->dig_p9, sizeShort);
	bme280data->dig_h1 = REALLOC(bme280data->dig_h1, sizeUChar);
	bme280data->dig_h2 = REALLOC(bme280data->dig_h2, sizeShort);
        bme280data->dig_h3 = REALLOC(bme280data->dig_h3, sizeUChar);
        bme280data->dig_h4 = REALLOC(bme280data->dig_h4, sizeShort);
        bme280data->dig_h5 = REALLOC(bme280data->dig_h5, sizeShort);
        bme280data->dig_h6 = REALLOC(bme280data->dig_h6, sizeChar);

	if(bme280data->dig_t1 == NULL || bme280data->dig_t2 == NULL || bme280data->dig_t3 == NULL || bme280data->dig_p1 == NULL ||
	  bme280data->dig_p2 == NULL || bme280data->dig_p3 == NULL || bme280data->dig_p4 == NULL || bme280data->dig_p5 == NULL ||
          bme280data->dig_p6 == NULL || bme280data->dig_p7 == NULL || bme280data->dig_p8 == NULL || bme280data->dig_p9 == NULL ||
          bme280data->dig_h1 == NULL || bme280data->dig_h2 == NULL || bme280data->dig_h3 == NULL || bme280data->dig_h4 == NULL ||
          bme280data->dig_h5 == NULL || bme280data->dig_h6 == NULL || bme280data->fd == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}

	for(y = 0; y < bme280data->nrid; y++) {
		// setup i2c
		bme280data->fd[y] = wiringXI2CSetup(bme280data->path, (int)strtol(bme280data->id[y], NULL, 16));
		if(bme280data->fd[y] > 0) {
			// read 0xD0 to check chip id: must equal 0x60 for BME280
			int id = wiringXI2CReadReg8(bme280data->fd[y], 0xD0);
			if(id != 0x60) {
				logprintf(LOG_ERR, "wrong device detected");
				exit(EXIT_FAILURE);
			}

			// read calibration coefficients from register addresses
			bme280data->dig_t1[y] = (uint16_t) wiringXI2CReadReg16(bme280data->fd[y], 0x88);
			bme280data->dig_t2[y] = (int16_t) wiringXI2CReadReg16(bme280data->fd[y], 0x8A);
			bme280data->dig_t3[y] = (int16_t) wiringXI2CReadReg16(bme280data->fd[y], 0x8C);

			bme280data->dig_p1[y] = (uint16_t) wiringXI2CReadReg16(bme280data->fd[y], 0x8E);
			bme280data->dig_p2[y] = (int16_t) wiringXI2CReadReg16(bme280data->fd[y], 0x90);
			bme280data->dig_p3[y] = (int16_t) wiringXI2CReadReg16(bme280data->fd[y], 0x92);
			bme280data->dig_p4[y] = (int16_t) wiringXI2CReadReg16(bme280data->fd[y], 0x94);
			bme280data->dig_p5[y] = (int16_t) wiringXI2CReadReg16(bme280data->fd[y], 0x96);
			bme280data->dig_p6[y] = (int16_t) wiringXI2CReadReg16(bme280data->fd[y], 0x98);
			bme280data->dig_p7[y] = (int16_t) wiringXI2CReadReg16(bme280data->fd[y], 0x9A);
			bme280data->dig_p8[y] = (int16_t) wiringXI2CReadReg16(bme280data->fd[y], 0x9C);
                        bme280data->dig_p9[y] = (int16_t) wiringXI2CReadReg16(bme280data->fd[y], 0x9E);

                        bme280data->dig_h1[y] = (uint8_t) wiringXI2CReadReg8(bme280data->fd[y], 0xA1);
                        bme280data->dig_h2[y] = (int16_t) wiringXI2CReadReg16(bme280data->fd[y], 0xE1);
                        bme280data->dig_h3[y] = (uint8_t) wiringXI2CReadReg8(bme280data->fd[y], 0xE3);
			bme280data->dig_h4[y] = (int16_t) (wiringXI2CReadReg8(bme280data->fd[y], 0xE4) << 4) | (wiringXI2CReadReg8(bme280data->fd[y], 0xE5) & 0xF);
                        bme280data->dig_h5[y] = (int16_t) (wiringXI2CReadReg8(bme280data->fd[y], 0xE6) << 4) | (wiringXI2CReadReg8(bme280data->fd[y], 0xE5) >> 4);
                        bme280data->dig_h6[y] = (int8_t) wiringXI2CReadReg8(bme280data->fd[y], 0xE7);


			// check communication: no result must equal 0 or 0xFFFF (=65535)
			if (bme280data->dig_t1[y] == 0 || bme280data->dig_t1[y] == 0xFFFF ||
					bme280data->dig_t2[y] == 0 || bme280data->dig_t2[y] == 0xFFFF ||
					bme280data->dig_t3[y] == 0 || bme280data->dig_t3[y] == 0xFFFF ||
					bme280data->dig_p1[y] == 0 || bme280data->dig_p1[y] == 0xFFFF ||
					bme280data->dig_p2[y] == 0 || bme280data->dig_p2[y] == 0xFFFF ||
					bme280data->dig_p3[y] == 0 || bme280data->dig_p3[y] == 0xFFFF ||
					bme280data->dig_p4[y] == 0 || bme280data->dig_p4[y] == 0xFFFF ||
					bme280data->dig_p5[y] == 0 || bme280data->dig_p5[y] == 0xFFFF ||
					bme280data->dig_p6[y] == 0 || bme280data->dig_p6[y] == 0xFFFF ||
					bme280data->dig_p7[y] == 0 || bme280data->dig_p7[y] == 0xFFFF ||
                                        bme280data->dig_p8[y] == 0 || bme280data->dig_p8[y] == 0xFFFF ||
                                        bme280data->dig_p9[y] == 0 || bme280data->dig_p9[y] == 0xFFFF
                                      	|| bme280data->dig_h1[y] == 0 || bme280data->dig_h1[y] == 0xFF
                                    	|| bme280data->dig_h2[y] == 0 || bme280data->dig_h2[y] == 0xFFFF
                 //                       || bme280data->dig_h3[y] == 0 || bme280data->dig_h3[y] == 0xFF
                                      	|| bme280data->dig_h4[y] == 0 || bme280data->dig_h4[y] == 0xFFFF
                                        || bme280data->dig_h5[y] == 0 || bme280data->dig_h5[y] == 0xFFFF
		 			|| bme280data->dig_h6[y] == 0 || bme280data->dig_h6[y] == 0xFF
					) {
				logprintf(LOG_ERR, "data communication error");
				exit(EXIT_FAILURE);
			}
		}
	}

	while (loop) {
		if (protocol_thread_wait(node, interval, &nrloops) == ETIMEDOUT) {
			pthread_mutex_lock(&lock);
			for (y = 0; y < bme280data->nrid; y++) {
				if (bme280data->fd[y] > 0) {
				// start forced mode measurement
					// write hum_oversampling in Register 0xF2 (ctrl_hum) to set
					// humidity oversampling.
					wiringXI2CWriteReg8(bme280data->fd[y], 0xF2, hum_oversampling);
                                        // write tem_oversampling and pre-oversampling into
					// Register 0xF4 (ctrl_meas) to set temp and press oversampling
					// this starts a single measurement
                                        wiringXI2CWriteReg8(bme280data->fd[y], 0xF4,(tem_oversampling<<5 | pre_oversampling<<2 | 0x01));

					// wait at least 50 ms: we suspend execution for 50000 microseconds.
					usleep(50000);

					// read the eight bytes result from address 0xF7.
					wiringXI2CWrite(bme280data->fd[y], 0xF7);
					//ut = (unsigned short) readReg16(bme280data->fd[y], 0xF7);
					uint8_t pmsb = wiringXI2CRead(bme280data->fd[y]);
                                        uint8_t plsb = wiringXI2CRead(bme280data->fd[y]);
                                        uint8_t pxsb = wiringXI2CRead(bme280data->fd[y]);
                                        uint8_t tmsb = wiringXI2CRead(bme280data->fd[y]);
                                        uint8_t tlsb = wiringXI2CRead(bme280data->fd[y]);
                                        uint8_t txsb = wiringXI2CRead(bme280data->fd[y]);
                                        uint8_t hmsb = wiringXI2CRead(bme280data->fd[y]);
                                        uint8_t hlsb = wiringXI2CRead(bme280data->fd[y]);

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

					int32_t vart1 = ((((raw_temperature>>3) - ((int32_t)bme280data->dig_t1[y] <<1))) *
						((int32_t)bme280data->dig_t2[y])) >> 11;
					int32_t vart2 = (((((raw_temperature>>4) - ((int32_t)bme280data->dig_t1[y])) *
						((raw_temperature>>4) - ((int32_t)bme280data->dig_t1[y]))) >> 12) *
						((int32_t)bme280data->dig_t3[y])) >> 14;
					int32_t cal_temperature = vart1 + vart2;

					float compensateTemperature = (cal_temperature * 5 + 128) >> 8;
					compensateTemperature=compensateTemperature/100;

					float compensatePressure = 0;
					long long var1 = ((long long)cal_temperature) - 128000;
					long long var2 = var1 * var1 * (long long)bme280data->dig_p6[y];
					var2 = var2 + ((var1 * (long long)bme280data->dig_p5[y]) << 17);
					var2 = var2 + (((long long)bme280data->dig_p4[y]) << 35);
					var1 = ((var1 * var1 * (long long)bme280data->dig_p3[y]) >> 8) + ((var1 * (long long)bme280data->dig_p2[y]) << 12);
					var1 = (((((long long)1) << 47) + var1)) * ((long long)bme280data->dig_p1[y]) >> 33;
					if (var1 != 0) {
						long long p = 1048576 - raw_pressure;
						p = (((p << 31) - var2) * 3125) / var1;
						var1 = (((long long)bme280data->dig_p9[y]) * (p >> 13) * (p>>13)) >> 25;
						var2 = (((long long)bme280data->dig_p8[y]) * p) >> 19;
						p = ((p + var1 + var2) >> 8) + (((long long)bme280data->dig_p7[y])<<4);
						compensatePressure = (float)p / 256;
					}

					float compensateHumidity = 0;
					int v_x1_u32r = (cal_temperature - ((int)76800));
					v_x1_u32r = (((((raw_humidity << 14) - (((int32_t)bme280data->dig_h4[y]) << 20) -
						(((int32_t)bme280data->dig_h5[y]) * v_x1_u32r)) + ((int32_t)16384)) >> 15) *
						(((((((v_x1_u32r * ((int32_t)bme280data->dig_h6[y])) >> 10) *
						(((v_x1_u32r * ((int32_t)bme280data->dig_h3[y])) >> 11) + ((int32_t)32768))) >> 10) +
						((int32_t)2097152)) * ((int32_t)bme280data->dig_h2[y]) + 8192) >> 14));

					v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
						((int32_t)bme280data->dig_h1[y])) >> 4));

					v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
 					v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
 					compensateHumidity = ((float)(v_x1_u32r>>12))/1024.0;

					bme280->message = json_mkobject();
					JsonNode *code = json_mkobject();
					json_append_member(code, "id", json_mkstring(bme280data->id[y]));
					json_append_member(code, "temperature", json_mknumber(((double) compensateTemperature ) + temp_offset, 2)); // in deg C
					json_append_member(code, "pressure", json_mknumber(((double) compensatePressure) + pressure_offset, 2)); // in hPa
                                        json_append_member(code, "humidity", json_mknumber(((double) compensateHumidity), 2));
					json_append_member(bme280->message, "message", code);
					json_append_member(bme280->message, "origin", json_mkstring("receiver"));
					json_append_member(bme280->message, "protocol", json_mkstring(bme280->id));

					if(pilight.broadcast != NULL) {
						pilight.broadcast(bme280->id, bme280->message, PROTOCOL);
					}
					json_delete(bme280->message);
					bme280->message = NULL;
				} else {
					logprintf(LOG_NOTICE, "error connecting to bme280");
					logprintf(LOG_DEBUG, "(probably i2c bus error from wiringXI2CSetup)");
					logprintf(LOG_DEBUG, "(maybe wrong id? use i2cdetect to find out)");
					protocol_thread_wait(node, 1, &nrloops);
				}
			}
			pthread_mutex_unlock(&lock);
		}
	}

	if (bme280data->id) {
		for (y = 0; y < bme280data->nrid; y++) {
			FREE(bme280data->id[y]);
		}
		FREE(bme280data->id);
	}
	if (bme280data->dig_t1) {
		FREE(bme280data->dig_t1);
	}
	if (bme280data->dig_t2) {
		FREE(bme280data->dig_t2);
	}
	if (bme280data->dig_t3) {
		FREE(bme280data->dig_t3);
	}
	if (bme280data->dig_p1) {
		FREE(bme280data->dig_p1);
	}
	if (bme280data->dig_p2) {
		FREE(bme280data->dig_p2);
	}
	if (bme280data->dig_p3) {
		FREE(bme280data->dig_p3);
	}
	if (bme280data->dig_p4) {
		FREE(bme280data->dig_p4);
	}
	if (bme280data->dig_p5) {
		FREE(bme280data->dig_p5);
	}
	if (bme280data->dig_p6) {
		FREE(bme280data->dig_p6);
	}
	if (bme280data->dig_p7) {
		FREE(bme280data->dig_p7);
	}
	if (bme280data->dig_p8) {
		FREE(bme280data->dig_p8);
	}
        if (bme280data->dig_p9) {
                FREE(bme280data->dig_p9);
        }
        if (bme280data->dig_h1) {
                FREE(bme280data->dig_h1);
        }
        if (bme280data->dig_h2) {
                FREE(bme280data->dig_h2);
        }
        if (bme280data->dig_h3) {
                FREE(bme280data->dig_h3);
        }
        if (bme280data->dig_h4) {
                FREE(bme280data->dig_h4);
        }
        if (bme280data->dig_h5) {
                FREE(bme280data->dig_h5);
        }
        if (bme280data->dig_h6) {
                FREE(bme280data->dig_h6);
        }
	if (bme280data->fd) {
		for (y = 0; y < bme280data->nrid; y++) {
			if (bme280data->fd[y] > 0) {
				close(bme280data->fd[y]);
			}
		}
		FREE(bme280data->fd);
	}
	FREE(bme280data);
	threads--;

	return (void *) NULL;
}

static struct threadqueue_t *initDev(JsonNode *jdevice) {
	char *platform = GPIO_PLATFORM;

	struct lua_state_t *state = plua_get_free_state();
	if(config_setting_get_string(state->L, "gpio-platform", 0, &platform) != 0) {
		logprintf(LOG_ERR, "no gpio-platform configured");
		assert(lua_gettop(state->L) == 0);
		plua_clear_state(state);
		return NULL;
	}
	assert(lua_gettop(state->L) == 0);
	plua_clear_state(state);
	if(strcmp(platform, "none") == 0) {
		FREE(platform);
		logprintf(LOG_ERR, "no gpio-platform configured");
		return NULL;
	}
	if(wiringXSetup(platform, logprintf1) < 0) {
		FREE(platform);
		return NULL;
	} else {
		FREE(platform);
		loop = 1;
		char *output = json_stringify(jdevice, NULL);
		JsonNode *json = json_decode(output);
		json_free(output);

		struct protocol_threads_t *node = protocol_thread_init(bme280, json);
		return threads_register("bme280", &thread, (void *) node, 0);
	}
}

static void threadGC(void) {
	loop = 0;
	protocol_thread_stop(bme280);
	while (threads > 0) {
		usleep(10);
	}
	protocol_thread_free(bme280);
}
#endif

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void bme280Init(void) {
#if !defined(__FreeBSD__) && !defined(_WIN32)
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&lock, &attr);
#endif

	protocol_register(&bme280);
	protocol_set_id(bme280, "bme280");
	protocol_device_add(bme280, "bme280", "I2C Barometric Pressure, humidity and Temperature Sensor");
	bme280->devtype = WEATHER;
	bme280->hwtype = SENSOR;

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

#if !defined(__FreeBSD__) && !defined(_WIN32)
	bme280->initDev = &initDev;
	bme280->threadGC = &threadGC;
#endif
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "bme280";
	module->version = "0.1";
	module->reqversion = "0.0";
	module->reqcommit = "1";
}

void init(void) {
	bme280Init();
}
#endif
