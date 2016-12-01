/*
	Copyright (C) 2014 CurlyMo

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
#include "../../core/threads.h"
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "../../core/json.h"
#ifndef _WIN32
	#include "../../../wiringx/wiringX.h"
#endif
#include "../protocol.h"
#include "tsl2561.h"

#if !defined(__FreeBSD__) && !defined(_WIN32)
typedef struct settings_t {
	char **id;
	int nrid;
	int *fd;
} settings_t;

static unsigned short loop = 1;
static int threads = 0;

static pthread_mutex_t lock;
static pthread_mutexattr_t attr;

// this is a example sourcecode from the datasheet
unsigned int CalculateLux(unsigned int iGain, unsigned int tInt, unsigned int ch0,unsigned int ch1, int iType)
{
    unsigned long chScale;
    unsigned long channel1;
    unsigned long channel0;
    switch (tInt)
    {
	case 0: // 13.7 msec
	    chScale = CHSCALE_TINT0;
	    break;
	case 1: // 101 msec
	    chScale = CHSCALE_TINT1;
	    break;
	default: // assume no scaling
	    chScale = (1 << CH_SCALE);
	    break;
    }

    if (!iGain) chScale = chScale << 4;

    channel0 = (ch0 * chScale) >> CH_SCALE;
    channel1 = (ch1 * chScale) >> CH_SCALE;

    unsigned long ratio1 = 0;
    if (channel0 != 0) 
	ratio1 = (channel1 << (RATIO_SCALE+1)) / channel0;

    unsigned long ratio = (ratio1 + 1) >> 1;

    unsigned int b, m;
    switch (iType)
    {
	case 0: // T, FN and CL package
	    if ((ratio >= 0) && (ratio <= K1T))
		{b=B1T; m=M1T;}
	    else if (ratio <= K2T)
	    	{b=B2T; m=M2T;}
	    else if (ratio <= K3T)
	    	{b=B3T; m=M3T;}
	    else if (ratio <= K4T)
	    	{b=B4T; m=M4T;}
	    else if (ratio <= K5T)
	    	{b=B5T; m=M5T;}
	    else if (ratio <= K6T)
	    	{b=B6T; m=M6T;}
	    else if (ratio <= K7T)
		{b=B7T; m=M7T;}
	    else if (ratio > K8T)
	    	{b=B8T; m=M8T;}
	    break;
	case 1:// CS package
	    if ((ratio >= 0) && (ratio <= K1C))
	    	{b=B1C; m=M1C;}
	    else if (ratio <= K2C)
	    	{b=B2C; m=M2C;}
	    else if (ratio <= K3C)
	    	{b=B3C; m=M3C;}
	    else if (ratio <= K4C)
	    	{b=B4C; m=M4C;}
	    else if (ratio <= K5C)
	    	{b=B5C; m=M5C;}
	    else if (ratio <= K6C)
	    	{b=B6C; m=M6C;}
	    else if (ratio <= K7C)
	    	{b=B7C; m=M7C;}
	    else if (ratio > K8C)
		{b=B8C; m=M8C;}
	    break;
    }

    unsigned long temp;
    temp = ((channel0 * b) - (channel1 * m));

    if (temp < 0) temp = 0;

    temp += (1 << (LUX_SCALE-1));

    unsigned long lux = temp >> LUX_SCALE;

    return(lux);
}


static void *thread(void *param) {
	struct protocol_threads_t *node = (struct protocol_threads_t *)param;
	struct JsonNode *json = (struct JsonNode *)node->param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	struct settings_t *tsl2561data = MALLOC(sizeof(struct settings_t));
	int y = 0, interval = 10, nrloops = 0 ;
	int ch0 = 0, ch1 = 0;
	int lux = 0 , ptype = 0;
	char *stmp = NULL;
	double itmp = -1 ;

	if(tsl2561data == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}

	tsl2561data->nrid = 0;
	tsl2561data->id = NULL;
	tsl2561data->fd = 0;

	threads++;

	if((jid = json_find_member(json, "id"))) {
		jchild = json_first_child(jid);
		while(jchild) {
			if(json_find_string(jchild, "id", &stmp) == 0) {
				if((tsl2561data->id = REALLOC(tsl2561data->id, (sizeof(char *)*(size_t)(tsl2561data->nrid+1)))) == NULL) {
					fprintf(stderr, "out of memory\n");
					exit(EXIT_FAILURE);
				}
				if((tsl2561data->id[tsl2561data->nrid] = MALLOC(strlen(stmp)+1)) == NULL) {
					fprintf(stderr, "out of memory\n");
					exit(EXIT_FAILURE);
				}
				strcpy(tsl2561data->id[tsl2561data->nrid], stmp);
				tsl2561data->nrid++;
			}
			jchild = jchild->next;
		}
	}

	if(json_find_number(json, "poll-interval", &itmp) == 0)
		interval = (int)round(itmp);

	if(json_find_number(json, "packagetype", &itmp) == 0)
		ptype = (int)round(itmp);

	if((tsl2561data->fd = REALLOC(tsl2561data->fd, (sizeof(int)*(size_t)(tsl2561data->nrid+1)))) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}

	for(y=0;y<tsl2561data->nrid;y++) {
		tsl2561data->fd[y] = wiringXI2CSetup((int)strtol(tsl2561data->id[y], NULL, 16));
	}

	while(loop) {
		if(protocol_thread_wait(node, interval, &nrloops) == ETIMEDOUT) {
			pthread_mutex_lock(&lock);
			for(y=0;y<tsl2561data->nrid;y++) {
				if(tsl2561data->fd[y] > 0) {
					wiringXI2CWriteReg8(tsl2561data->fd[y],0x80,0x03);
					wiringXI2CWriteReg8(tsl2561data->fd[y],0x81,0x02);
					usleep (0.403 * 1000000);
					ch0 = wiringXI2CReadReg16(tsl2561data->fd[y], 0xac);
					ch1 = wiringXI2CReadReg16(tsl2561data->fd[y], 0xae);
					wiringXI2CWriteReg8(tsl2561data->fd[y],0x80,0x00);
					lux = CalculateLux(0, 2, ch0, ch1, ptype);

					tsl2561->message = json_mkobject();
					JsonNode *code = json_mkobject();
					json_append_member(code, "id", json_mkstring(tsl2561data->id[y]));
					json_append_member(code, "illuminance", json_mknumber(lux, 0));
					json_append_member(tsl2561->message, "message", code);
					json_append_member(tsl2561->message, "origin", json_mkstring("receiver"));
					json_append_member(tsl2561->message, "protocol", json_mkstring(tsl2561->id));

					if(pilight.broadcast != NULL) {
						pilight.broadcast(tsl2561->id, tsl2561->message, PROTOCOL);
					}
					json_delete(tsl2561->message);
					tsl2561->message = NULL;
				} else {
					logprintf(LOG_NOTICE, "error connecting to tsl2561");
					logprintf(LOG_DEBUG, "(probably i2c bus error from wiringXI2CSetup)");
					logprintf(LOG_DEBUG, "(maybe wrong id? use i2cdetect to find out)");
					protocol_thread_wait(node, 1, &nrloops);
				}
			}
			pthread_mutex_unlock(&lock);
		}
	}
	pthread_mutex_unlock(&lock);

	if(tsl2561data->id) {
		for(y=0;y<tsl2561data->nrid;y++) {
			FREE(tsl2561data->id[y]);
		}
		FREE(tsl2561->id);
	}
	if(tsl2561data->fd) {
		for(y=0;y<tsl2561data->nrid;y++) {
			if(tsl2561data->fd[y] > 0) {
				close(tsl2561data->fd[y]);
			}
		}
		FREE(tsl2561data->fd);
	}
	FREE(tsl2561data);
	threads--;

	return (void *)NULL;
}

static struct threadqueue_t *initDev(JsonNode *jdevice) {
	if(wiringXSupported() == 0 && wiringXSetup() == 0) {
		loop = 1;
		char *output = json_stringify(jdevice, NULL);
		JsonNode *json = json_decode(output);
		json_free(output);

		struct protocol_threads_t *node = protocol_thread_init(tsl2561, json);
		return threads_register("tsl2561", &thread, (void *)node, 0);
	} else {
		return NULL;
	}
}

static void threadGC(void) {
	loop = 0;
	protocol_thread_stop(tsl2561);
	while(threads > 0) {
		usleep(10);
	}
	protocol_thread_free(tsl2561);
}


#endif

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void tsl2561Init(void) {
#if !defined(__FreeBSD__) && !defined(_WIN32)
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&lock, &attr);
#endif

	protocol_register(&tsl2561);
	protocol_set_id(tsl2561, "tsl2561");
	protocol_device_add(tsl2561, "tsl2561", "TAOS I2C illuminance sensor");
	tsl2561->devtype = WEATHER;
	tsl2561->hwtype = SENSOR;
	
	options_add(&tsl2561->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "0x[0-9a-fA-F]{2}");
	options_add(&tsl2561->options, 'e', "illuminance", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1}$");
	options_add(&tsl2561->options, 0, "packagetype", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&tsl2561->options, 0, "poll-intervall", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)10, "[0-9]");
	options_add(&tsl2561->options, 0, "show-illuminance", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

#if !defined(__FreeBSD__) && !defined(_WIN32)
	tsl2561->initDev=&initDev;
	tsl2561->threadGC=&threadGC;
#endif
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "tsl2561";
	module->version = "1.0";
	module->reqversion = "7.0";
	module->reqcommit = "1";
}

void init(void) {
	tsl2561Init();
}
#endif
