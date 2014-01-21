/*
	Copyright (C) 2013 CurlyMo

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

#include "../../pilight.h"
#include "common.h"
#include "options.h"
#include "protocol.h"
#include "log.h"

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
#include "../protocols/pilight_firmware.h"

void protocol_init(void) {
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
pilightFirmwareInit();
}

void protocol_register(protocol_t **proto) {
	*proto = malloc(sizeof(struct protocol_t));
	(*proto)->options = NULL;
	(*proto)->devices = NULL;
	(*proto)->conflicts = NULL;
	(*proto)->settings = NULL;
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
	(*proto)->message = NULL;

	(*proto)->repeats = 0;
	(*proto)->first = 0;
	(*proto)->second = 0;

	memset(&(*proto)->raw[0], 0, sizeof((*proto)->raw));
	memset(&(*proto)->code[0], 0, sizeof((*proto)->code));
	memset(&(*proto)->pCode[0], 0, sizeof((*proto)->pCode));
	memset(&(*proto)->binary[0], 0, sizeof((*proto)->binary));

	struct protocols_t *pnode = malloc(sizeof(struct protocols_t));
	pnode->listener = *proto;
	pnode->name = malloc(4);
	pnode->next = protocols;
	protocols = pnode;
}

void protocol_set_id(protocol_t *proto, const char *id) {
	proto->id = malloc(strlen(id)+1);
	strcpy(proto->id, id);
}

void protocol_plslen_add(protocol_t *proto, int plslen) {
	struct protocol_plslen_t *pnode = malloc(sizeof(struct protocol_plslen_t));
	pnode->length = plslen;
	pnode->next	= proto->plslen;
	proto->plslen = pnode;
}

void protocol_device_add(protocol_t *proto, const char *id, const char *desc) {
	struct protocol_devices_t *dnode = malloc(sizeof(struct protocol_devices_t));
	dnode->id = malloc(strlen(id)+1);
	strcpy(dnode->id, id);
	dnode->desc = malloc(strlen(desc)+1);
	strcpy(dnode->desc, desc);
	dnode->next	= proto->devices;
	proto->devices = dnode;
}

void protocol_conflict_add(protocol_t *proto, const char *id) {
	struct protocol_conflicts_t *cnode = malloc(sizeof(struct protocol_conflicts_t));
	cnode->id = malloc(strlen(id)+1);
	strcpy(cnode->id, id);
	cnode->next	= proto->conflicts;
	proto->conflicts = cnode;
}

/* http://www.cs.bu.edu/teaching/c/linked-list/delete/ */
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

int protocol_setting_update_string(protocol_t *proto, const char *name, const char *value) {
	struct protocol_settings_t *tmp_settings = proto->settings;

	while(tmp_settings) {
		if(strcmp(tmp_settings->name, name) == 0 && tmp_settings->type == 1) {
			tmp_settings->old_value = realloc(tmp_settings->old_value, strlen(tmp_settings->cur_value)+1);
			strcpy(tmp_settings->old_value, tmp_settings->cur_value);
			tmp_settings->cur_value = realloc(tmp_settings->cur_value, strlen(value)+1);
			strcpy(tmp_settings->cur_value, value);
			return 0;
		}
		tmp_settings = tmp_settings->next;
	}
	sfree((void *)&tmp_settings);
	return 1;
}

int protocol_setting_update_number(protocol_t *proto, const char *name, int value) {
	struct protocol_settings_t *tmp_settings = proto->settings;

	while(tmp_settings) {
		if(strcmp(tmp_settings->name, name) == 0 && tmp_settings->type == 2) {
			tmp_settings->old_value = realloc(tmp_settings->old_value, strlen(tmp_settings->cur_value)+1);
			strcpy(tmp_settings->old_value, tmp_settings->cur_value);
			tmp_settings->cur_value = realloc(tmp_settings->cur_value, sizeof(int)+1);
			sprintf(tmp_settings->cur_value, "%d", value);
			return 0;
		}
		tmp_settings = tmp_settings->next;
	}
	sfree((void *)&tmp_settings);
	return 1;
}

int protocol_setting_restore(protocol_t *proto, const char *name) {
	struct protocol_settings_t *tmp_settings = proto->settings;

	while(tmp_settings) {
		if(strcmp(tmp_settings->name, name) == 0) {
			tmp_settings->cur_value = realloc(tmp_settings->cur_value, strlen(tmp_settings->old_value)+1);
			strcpy(tmp_settings->cur_value, tmp_settings->old_value);
			tmp_settings->old_value = realloc(tmp_settings->old_value, 4);
			memset(tmp_settings->old_value, '\0', 4);
			return 0;
		}
		tmp_settings = tmp_settings->next;
	}
	sfree((void *)&tmp_settings);
	return 1;
}

int protocol_setting_check_string(protocol_t *proto, const char *name, const char *value) {
	int error = EXIT_SUCCESS;

	switch(proto->devtype) {
		case DIMMER:
			if(strcmp(name, "states") != 0) {
				error=EXIT_FAILURE;
			}
		break;
		case RELAY:
			if(strcmp(name, "default") != 0 && strcmp(name, "states") != 0) {
				error=EXIT_FAILURE;
			}
		break;
		case SWITCH:
		case SCREEN:
			if(strcmp(name, "states") != 0) {
				error=EXIT_FAILURE;
			}
		break;
		case WEATHER:
		case RAW:
		default:
			error=EXIT_FAILURE;
		break;
	}

	if(strcmp(name, "default") == 0 || strcmp(name, "states") == 0) {
		if(proto->options) {
			char *nvalue = malloc(strlen(value)+1);
			strcpy(nvalue, value);
			char *pch = strtok(nvalue, ",");
			while(pch) {
				int valid_state = 0;
				struct options_t *options = proto->options;
				while(options) {
					if(options->conftype == config_state && strcmp(options->name, pch) == 0) {
						valid_state = 1;
						break;
					}
					options = options->next;
				}
				if(valid_state == 0) {
					error=EXIT_FAILURE;
				}
				pch = strtok(NULL, ",");
			}
			sfree((void *)&nvalue);
		}
	}

	return error;
}

void protocol_setting_add_string(protocol_t *proto, const char *name, const char *value) {
	if(protocol_setting_check_string(proto, name, value) == 0) {
		protocol_setting_remove(&proto, name);
		struct protocol_settings_t *snode = malloc(sizeof(struct protocol_settings_t));
		snode->name = malloc(strlen(name)+1);
		strcpy(snode->name, name);
		snode->cur_value = malloc(strlen(value)+1);
		snode->old_value = malloc(4);
		strcpy(snode->cur_value, value);
		snode->type = 1;
		snode->next	= proto->settings;
		proto->settings = snode;
	} else {
		logprintf(LOG_ERR, "protocol \"%s\" setting \"%s\" is invalid", proto->id, name);
		exit(EXIT_FAILURE);
	}
}

int protocol_setting_check_number(protocol_t *proto, const char *name, int value) {

	int error = EXIT_SUCCESS;
#ifndef __FreeBSD__
	regex_t regex;
	int reti;
#endif

	switch(proto->devtype) {
		case DIMMER:
			if(strcmp(name, "max") != 0 && strcmp(name, "min") != 0 && strcmp(name, "readonly") != 0) {
				error=EXIT_FAILURE;
			}
		break;
		case WEATHER:
			if(strcmp(name, "decimals") != 0 && strcmp(name, "battery") != 0
			   && strcmp(name, "temperature") != 0 && strcmp(name, "humidity") != 0
			   && strcmp(name, "temp-corr") != 0 && strcmp(name, "humi-corr") != 0) {
			   if(proto->hwtype == SENSOR && strcmp(name, "interval") != 0) {
					error=EXIT_FAILURE;
				}
			}
		break;
		case SWITCH:
		case SCREEN:
			if(strcmp(name, "readonly") != 0) {
				error=EXIT_FAILURE;
			}
		break;
		case RELAY:
			if(strcmp(name, "readonly") != 0) {
				error=EXIT_FAILURE;
			}
		break;
		case RAW:
		default:
			error=EXIT_FAILURE;
		break;
	}

	if((strcmp(name, "readonly") == 0 ||
		strcmp(name, "temperature") == 0 ||
		strcmp(name, "battery") == 0 ||
		strcmp(name, "humidity") == 0) &&
		(value < 0 || value > 1)) {
			error=EXIT_FAILURE;
	}

	if(strcmp(name, "decimals") == 0 && (value < 0 || value > 3)) {
		error=EXIT_FAILURE;
	}

	if(strcmp(name, "interval") == 0 && value < 0) {
		error=EXIT_FAILURE;
	}

	if(strcmp(name, "min") == 0 || strcmp(name, "max") == 0) {
		struct options_t *tmp_options = proto->options;
		while(tmp_options) {
			if(tmp_options->conftype == config_value && strlen(tmp_options->mask) > 0) {
#ifndef __FreeBSD__
				/* If the argument has a regex mask, check if it passes */
				reti = regcomp(&regex, tmp_options->mask, REG_EXTENDED);
				if(reti) {
					logprintf(LOG_ERR, "could not compile regex");
					error=EXIT_FAILURE;
				}
				char *tmp = malloc(sizeof(value)+1);
				sprintf(tmp, "%d", value);
				reti = regexec(&regex, tmp, 0, NULL, 0);
				if(reti == REG_NOMATCH || reti != 0) {
					sfree((void *)&tmp);
					regfree(&regex);
					error=EXIT_FAILURE;
				}
				sfree((void *)&tmp);
				regfree(&regex);
#endif
			}
			tmp_options = tmp_options->next;
		}
	}

	return error;
}

void protocol_setting_add_number(protocol_t *proto, const char *name, int value) {
	if(protocol_setting_check_number(proto, name, value) == 0) {
		protocol_setting_remove(&proto, name);
		struct protocol_settings_t *snode = malloc(sizeof(struct protocol_settings_t));
		snode->name = malloc(strlen(name)+1);
		strcpy(snode->name, name);
		snode->cur_value = malloc(sizeof(value)+1);
		snode->old_value = malloc(4);
		sprintf(snode->cur_value, "%d", value);
		snode->type = 2;
		snode->next	= proto->settings;
		proto->settings = snode;
	} else {
		logprintf(LOG_ERR, "protocol \"%s\" setting \"%s\" is invalid", proto->id, name);
		exit(EXIT_FAILURE);
	}
}

int protocol_setting_get_string(protocol_t *proto, const char *name, char **out) {
	struct protocol_settings_t *tmp_settings = proto->settings;

	while(tmp_settings) {
		if(strcmp(tmp_settings->name, name) == 0 && tmp_settings->type == 1) {
			*out = tmp_settings->cur_value;
			return 0;
		}
		tmp_settings = tmp_settings->next;
	}
	sfree((void *)&tmp_settings);
	return 1;
}

int protocol_setting_get_number(protocol_t *proto, const char *name, int *out) {
	struct protocol_settings_t *tmp_settings = proto->settings;

	while(tmp_settings) {
		if(strcmp(tmp_settings->name, name) == 0 && tmp_settings->type == 2) {
			*out = atoi(tmp_settings->cur_value);
			return 0;
		}
		tmp_settings = tmp_settings->next;
	}
	sfree((void *)&tmp_settings);
	return 1;
}

void protocol_setting_remove(protocol_t **proto, const char *name) {
	struct protocol_settings_t *currP, *prevP;

	prevP = NULL;

	for(currP = (*proto)->settings; currP != NULL; prevP = currP, currP = currP->next) {

		if(strcmp(currP->name, name) == 0) {
			if(prevP == NULL) {
				(*proto)->settings = currP->next;
			} else {
				prevP->next = currP->next;
			}

			sfree((void *)&currP->name);
			sfree((void *)&currP->cur_value);
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
	struct protocol_settings_t *stmp;
	struct protocol_plslen_t *ttmp;

	while(protocols) {
		ptmp = protocols;
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
		if(ptmp->listener->settings) {
			while(ptmp->listener->settings) {
				stmp = ptmp->listener->settings;
				sfree((void *)&stmp->name);
				sfree((void *)&stmp->cur_value);
				sfree((void *)&stmp->old_value);
				ptmp->listener->settings = ptmp->listener->settings->next;
				sfree((void *)&stmp);
			}
		}
		sfree((void *)&ptmp->listener->settings);
		sfree((void *)&ptmp->listener);
		protocols = protocols->next;
		sfree((void *)&ptmp);
	}
	sfree((void *)&protocols);

	logprintf(LOG_DEBUG, "garbage collected protocol library");
	return EXIT_SUCCESS;
}
