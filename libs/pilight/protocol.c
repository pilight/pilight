/*
	Copyright (C) 2013 - 2014 CurlyMo

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
#include <regex.h>
#include <sys/time.h>

#include "../../pilight.h"
#include "common.h"
#include "options.h"
#include "protocol.h"
#include "log.h"
#include "../protocols/pilight_firmware.h"

#if defined(PROTOCOL_COCO_SWITCH) || defined(PROTOCOL_DIO_SWITCH) || defined(PROTOCOL_NEXA_SWITCH) || defined(PROTOCOL_KAKU_SWITCH) || defined(PROTOCOL_INTERTECHNO_SWITCH)
	#include "../protocols/arctech_switch.h"
#endif
#ifdef PROTOCOL_KAKU_SCREEN
	#include "../protocols/arctech_screen.h"
#endif
#ifdef PROTOCOL_KAKU_CONTACT
  #include "../protocols/arctech_contact.h"
#endif
#ifdef PROTOCOL_KAKU_DIMMER
	#include "../protocols/arctech_dimmer.h"
#endif
#if defined(PROTOCOL_COGEX_SWITCH) || defined(PROTOCOL_KAKU_SWITCH_OLD) || defined(PROTOCOL_INTERTECHNO_OLD)
	#include "../protocols/arctech_switch_old.h"
#endif
#ifdef PROTOCOL_KAKU_SCREEN_OLD
	#include "../protocols/arctech_screen_old.h"
#endif
#ifdef PROTOCOL_HOMEEASY_OLD
	#include "../protocols/home_easy_old.h"
#endif
#ifdef PROTOCOL_ELRO_SWITCH
	#include "../protocols/elro_he.h"
	#include "../protocols/elro_hc.h"
#endif
#if defined(PROTOCOL_SELECTREMOTE) || defined(PROTOCOL_IMPULS)
	#include "../protocols/impuls.h"
#endif
#ifdef PROTOCOL_ALECTO
	#include "../protocols/alecto.h"
#endif
#ifdef PROTOCOL_RAW
	#include "../protocols/raw.h"
#endif
#ifdef PROTOCOL_RELAY
	#include "../protocols/relay.h"
#endif
#ifdef PROTOCOL_REV
        #include "../protocols/rev.h"
#endif
#ifdef PROTOCOL_GENERIC_WEATHER
	#include "../protocols/generic_weather.h"
#endif
#ifdef PROTOCOL_GENERIC_SWITCH
	#include "../protocols/generic_switch.h"
#endif
#ifdef PROTOCOL_GENERIC_DIMMER
	#include "../protocols/generic_dimmer.h"
#endif
#ifdef PROTOCOL_DS18B20
	#include "../protocols/ds18b20.h"
#endif
#ifdef PROTOCOL_DS18S20
	#include "../protocols/ds18s20.h"
#endif
#ifdef PROTOCOL_DHT22
	#include "../protocols/dht22.h"
#endif
#ifdef PROTOCOL_DHT11
	#include "../protocols/dht11.h"
#endif
#ifdef PROTOCOL_CLARUS
	#include "../protocols/clarus.h"
#endif
#ifdef PROTOCOL_RPI_TEMP
	#include "../protocols/rpi_temp.h"
#endif
#ifdef PROTOCOL_CONRAD_RSL_SWITCH
	#include "../protocols/conrad_rsl_switch.h"
#endif
#ifdef PROTOCOL_CONRAD_RSL_CONTACT
	#include "../protocols/conrad_rsl_contact.h"
#endif
#if defined(PROTOCOL_LM75) || defined(PROTOCOL_LM76)
	#include "../protocols/lm75.h"
	#include "../protocols/lm76.h"
#endif
#ifdef PROTOCOL_POLLIN_SWITCH
	#include "../protocols/pollin.h"
#endif
#ifdef PROTOCOL_MUMBI_SWITCH
	#include "../protocols/mumbi.h"
#endif
#ifdef PROTOCOL_WUNDERGROUND
	#include "../protocols/wunderground.h"
#endif
#ifdef PROTOCOL_OPENWEATHERMAP
	#include "../protocols/openweathermap.h"
#endif
#ifdef PROTOCOL_SILVERCREST
	#include "../protocols/silvercrest.h"
#endif
#ifdef PROTOCOL_THREECHAN
	#include "../protocols/threechan.h"
#endif
#ifdef PROTOCOL_TEKNIHALL
	#include "../protocols/teknihall.h"
#endif
#ifdef PROTOCOL_X10
	#include "../protocols/x10.h"
#endif
#ifdef PROTOCOL_SUNRISESET
	#include "../protocols/sunriseset.h"
#endif

void protocol_init(void) {
	pilightFirmwareInit();
#if defined(PROTOCOL_COCO_SWITCH) || defined(PROTOCOL_DIO_SWITCH) || defined(PROTOCOL_NEXA_SWITCH) || defined(PROTOCOL_KAKU_SWITCH) || defined(PROTOCOL_INTERTECHNO_SWITCH)
	arctechSwInit();
#endif
#ifdef PROTOCOL_KAKU_SCREEN
	arctechSrInit();
#endif
#ifdef PROTOCOL_KAKU_CONTACT
  arctechContactInit();
#endif
#ifdef PROTOCOL_KAKU_DIMMER
	arctechDimInit();
#endif
#if defined(PROTOCOL_COGEX_SWITCH) || defined(PROTOCOL_KAKU_SWITCH_OLD) || defined(PROTOCOL_INTERTECHNO_OLD)
	arctechSwOldInit();
#endif
#ifdef PROTOCOL_KAKU_SCREEN_OLD
	arctechSrOldInit();
#endif
#ifdef PROTOCOL_HOMEEASY_OLD
	homeEasyOldInit();
#endif
#if defined(PROTOCOL_ELRO_SWITCH) || defined(PROTOCOL_BRENNENSTUHL_SWITCH)
	elroHEInit();
	elroHCInit();
#endif
#if defined(PROTOCOL_SELECTREMOTE) || defined(PROTOCOL_IMPULS)
	impulsInit();
#endif
#ifdef PROTOCOL_RELAY
	relayInit();
#endif
#ifdef PROTOCOL_RAW
	rawInit();
#endif
#ifdef PROTOCOL_REV
        revInit();
#endif
#ifdef PROTOCOL_ALECTO
	alectoInit();
#endif
#ifdef PROTOCOL_GENERIC_WEATHER
	genWeatherInit();
#endif
#ifdef PROTOCOL_GENERIC_SWITCH
	genSwitchInit();
#endif
#ifdef PROTOCOL_GENERIC_DIMMER
	genDimInit();
#endif
#ifdef PROTOCOL_DS18B20
	ds18b20Init();
#endif
#ifdef PROTOCOL_DS18S20
	ds18s20Init();
#endif
#ifdef PROTOCOL_DHT22
	dht22Init();
#endif
#ifdef PROTOCOL_DHT11
	dht11Init();
#endif
#ifdef PROTOCOL_CLARUS
	clarusSwInit();
#endif
#ifdef PROTOCOL_RPI_TEMP
	rpiTempInit();
#endif
#ifdef PROTOCOL_CONRAD_RSL_SWITCH
	conradRSLSwInit();
#endif
#ifdef PROTOCOL_CONRAD_RSL_CONTACT
	conradRSLCnInit();
#endif
#if defined(PROTOCOL_LM75) || defined(PROTOCOL_LM76)
	lm75Init();
	lm76Init();
#endif
#ifdef PROTOCOL_POLLIN_SWITCH
	pollinInit();
#endif
#ifdef PROTOCOL_MUMBI_SWITCH
	mumbiInit();
#endif
#ifdef PROTOCOL_WUNDERGROUND
	wundergroundInit();
#endif
#ifdef PROTOCOL_OPENWEATHERMAP
	openweathermapInit();
#endif
#ifdef PROTOCOL_SILVERCREST
	silvercrestInit();
#endif
#ifdef PROTOCOL_THREECHAN
	threechanInit();
#endif
#ifdef PROTOCOL_TEKNIHALL
	teknihallInit();
#endif
#ifdef PROTOCOL_X10
	x10Init();
#endif
#ifdef PROTOCOL_SUNRISESET
	sunRiseSetInit();
#endif
}

void protocol_register(protocol_t **proto) {
	*proto = malloc(sizeof(struct protocol_t));
	if(!*proto) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	(*proto)->options = NULL;
	(*proto)->devices = NULL;
	(*proto)->conflicts = NULL;
	(*proto)->plslen = NULL;

	(*proto)->pulse = 0;
	(*proto)->rawlen = 0;
	(*proto)->binlen = 0;
	(*proto)->lsb = 0;
	(*proto)->txrpt = 1;
	(*proto)->hwtype = 1;
	(*proto)->rxrpt = 1;
	(*proto)->parseRaw = NULL;
	(*proto)->parseBinary = NULL;
	(*proto)->parseCode = NULL;
	(*proto)->createCode = NULL;
	(*proto)->checkValues = NULL;
	(*proto)->initDev = NULL;
	(*proto)->printHelp = NULL;
	(*proto)->threadGC = NULL;
	(*proto)->gc = NULL;
	(*proto)->message = NULL;
	(*proto)->threads = NULL;

	(*proto)->repeats = 0;
	(*proto)->first = 0;
	(*proto)->second = 0;

	memset(&(*proto)->raw[0], 0, sizeof((*proto)->raw));
	memset(&(*proto)->code[0], 0, sizeof((*proto)->code));
	memset(&(*proto)->pCode[0], 0, sizeof((*proto)->pCode));
	memset(&(*proto)->binary[0], 0, sizeof((*proto)->binary));

	struct protocols_t *pnode = malloc(sizeof(struct protocols_t));
	if(!pnode) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	pnode->listener = *proto;
	pnode->name = malloc(4);
	if(!pnode->name) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	pnode->next = protocols;
	protocols = pnode;
}

struct protocol_threads_t *protocol_thread_init(protocol_t *proto, struct JsonNode *param) {
	struct protocol_threads_t *node = malloc(sizeof(struct protocol_threads_t));
	node->param = param;
	pthread_mutexattr_init(&node->attr);
	pthread_mutexattr_settype(&node->attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&node->mutex, &node->attr);
    pthread_cond_init(&node->cond, NULL);	
	node->next = proto->threads;
	proto->threads = node;
	return node;
}

int protocol_thread_wait(struct protocol_threads_t *node, int interval, int *nrloops) {
	struct timeval tp;
	struct timespec ts;

	pthread_mutex_unlock(&node->mutex);

	gettimeofday(&tp, NULL);
	ts.tv_sec = tp.tv_sec;
	ts.tv_nsec = tp.tv_usec * 1000;
	
	if(*nrloops == 0) {
		ts.tv_sec += 1;
		*nrloops = 1;
	} else {
		ts.tv_sec += interval;
	}

	pthread_mutex_lock(&node->mutex);

	return pthread_cond_timedwait(&node->cond, &node->mutex, &ts);
}

void protocol_thread_stop(protocol_t *proto) {
	if(proto->threads) {
		struct protocol_threads_t *tmp = proto->threads;
		while(tmp) {
			pthread_mutex_unlock(&tmp->mutex);
			pthread_cond_broadcast(&tmp->cond);
			tmp = tmp->next;
		}
	}
}

void protocol_thread_free(protocol_t *proto) {
	if(proto->threads) {
		struct protocol_threads_t *tmp = proto->threads;
		while(tmp) {
			tmp = proto->threads;
			if(tmp->param) {
				json_delete(tmp->param);
			}
			tmp = tmp->next;
			sfree((void *)&tmp);
		}
		sfree((void *)&proto->threads);
	}
}

void protocol_set_id(protocol_t *proto, const char *id) {
	proto->id = malloc(strlen(id)+1);
	if(!proto->id) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(proto->id, id);
}

void protocol_plslen_add(protocol_t *proto, int plslen) {
	struct protocol_plslen_t *pnode = malloc(sizeof(struct protocol_plslen_t));
	if(!pnode) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	pnode->length = plslen;
	pnode->next	= proto->plslen;
	proto->plslen = pnode;
}

void protocol_device_add(protocol_t *proto, const char *id, const char *desc) {
	struct protocol_devices_t *dnode = malloc(sizeof(struct protocol_devices_t));
	if(!dnode) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	dnode->id = malloc(strlen(id)+1);
	if(!dnode->id) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(dnode->id, id);
	dnode->desc = malloc(strlen(desc)+1);
	if(!dnode->desc) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(dnode->desc, desc);
	dnode->next	= proto->devices;
	proto->devices = dnode;
}

void protocol_conflict_add(protocol_t *proto, const char *id) {
	struct protocol_conflicts_t *cnode = malloc(sizeof(struct protocol_conflicts_t));
	if(!cnode) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	cnode->id = malloc(strlen(id)+1);
	if(!cnode->id) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(cnode->id, id);
	cnode->next	= proto->conflicts;
	proto->conflicts = cnode;
}

void protocol_conflict_remove(protocol_t **proto, const char *id) {
	struct protocol_conflicts_t *currP, *prevP;

	prevP = NULL;

	for(currP = (*proto)->conflicts; currP != NULL; prevP = currP, currP = currP->next) {

		if(strcmp(currP->id, id) == 0) {
			if(prevP == NULL) {
				(*proto)->conflicts = currP->next;
			} else {
				prevP->next = currP->next;
			}

			sfree((void *)&currP->id);
			sfree((void *)&currP);

			break;
		}
	}
}

int protocol_device_exists(protocol_t *proto, const char *id) {
	struct protocol_devices_t *temp = proto->devices;

	while(temp) {
		if(strcmp(temp->id, id) == 0) {
			return 0;
		}
		temp = temp->next;
	}
	sfree((void *)&temp);
	return 1;
}

int protocol_gc(void) {
	struct protocols_t *ptmp;
	struct protocol_devices_t *dtmp;
	struct protocol_conflicts_t *ctmp;
	struct protocol_plslen_t *ttmp;

	while(protocols) {
		ptmp = protocols;
		logprintf(LOG_DEBUG, "protocol %s", ptmp->listener->id);
		if(ptmp->listener->threadGC) {
			ptmp->listener->threadGC();
			logprintf(LOG_DEBUG, "ran garbage collector");
		}		
		if(ptmp->listener->gc) {
			ptmp->listener->gc();
			logprintf(LOG_DEBUG, "ran garbage collector");
		}
		sfree((void *)&ptmp->listener->id);
		sfree((void *)&ptmp->name);
		options_delete(ptmp->listener->options);
		if(ptmp->listener->plslen) {
			while(ptmp->listener->plslen) {
				ttmp = ptmp->listener->plslen;
				ptmp->listener->plslen = ptmp->listener->plslen->next;
				sfree((void *)&ttmp);
			}
		}
		sfree((void *)&ptmp->listener->plslen);
		if(ptmp->listener->devices) {
			while(ptmp->listener->devices) {
				dtmp = ptmp->listener->devices;
				sfree((void *)&dtmp->id);
				sfree((void *)&dtmp->desc);
				ptmp->listener->devices = ptmp->listener->devices->next;
				sfree((void *)&dtmp);
			}
		}
		sfree((void *)&ptmp->listener->devices);
		if(ptmp->listener->conflicts) {
			while(ptmp->listener->conflicts) {
				ctmp = ptmp->listener->conflicts;
				sfree((void *)&ctmp->id);
				ptmp->listener->conflicts = ptmp->listener->conflicts->next;
				sfree((void *)&ctmp);
			}
		}
		sfree((void *)&ptmp->listener->conflicts);
		sfree((void *)&ptmp->listener);
		protocols = protocols->next;
		sfree((void *)&ptmp);
	}
	sfree((void *)&protocols);

	logprintf(LOG_DEBUG, "garbage collected protocol library");
	return EXIT_SUCCESS;
}
