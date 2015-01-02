<<<<<<< HEAD
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
#ifdef PROTOCOL_DHT22
	#include "../protocols/dht22.h"
#endif
<<<<<<< HEAD
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
=======
#ifdef PROTOCOL_CLARUS
	#include "../protocols/clarus.h"
#endif
>>>>>>> origin/master

void protocol_init(void) {
#if defined(PROTOCOL_COCO_SWITCH) || defined(PROTOCOL_DIO_SWITCH) || defined(PROTOCOL_NEXA_SWITCH) || defined(PROTOCOL_KAKU_SWITCH) || defined(PROTOCOL_INTERTECHNO_SWITCH)
	arctechSwInit();
#endif
#ifdef PROTOCOL_KAKU_SCREEN
	arctechSrInit();
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
#ifdef PROTOCOL_DHT22
	dht22Init();
#endif
<<<<<<< HEAD
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
=======
#ifdef PROTOCOL_CLARUS
	clarusSwInit();
#endif
>>>>>>> origin/master
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

/* http://www.cs.bu.edu/teaching/c/linked-list/delete/ */
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
=======
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
#include <dirent.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "dso.h"
#include "options.h"
#include "settings.h"
#include "protocol.h"
#include "log.h"

#include "protocol_header.h"

void protocol_remove(char *name) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	struct protocols_t *currP, *prevP;

	prevP = NULL;

	for(currP = protocols; currP != NULL; prevP = currP, currP = currP->next) {

		if(strcmp(currP->listener->id, name) == 0) {
			if(prevP == NULL) {
				protocols = currP->next;
			} else {
				prevP->next = currP->next;
			}

			struct protocol_devices_t *dtmp;
			struct protocol_plslen_t *ttmp;
			logprintf(LOG_DEBUG, "removed protocol %s", currP->listener->id);
			if(currP->listener->threadGC) {
				currP->listener->threadGC();
				logprintf(LOG_DEBUG, "stopped protocol threads");
			}
			if(currP->listener->gc) {
				currP->listener->gc();
				logprintf(LOG_DEBUG, "ran garbage collector");
			}
			sfree((void *)&currP->listener->id);
			sfree((void *)&currP->name);
			options_delete(currP->listener->options);
			if(currP->listener->plslen) {
				while(currP->listener->plslen) {
					ttmp = currP->listener->plslen;
					currP->listener->plslen = currP->listener->plslen->next;
					sfree((void *)&ttmp);
				}
			}
			sfree((void *)&currP->listener->plslen);
			if(currP->listener->devices) {
				while(currP->listener->devices) {
					dtmp = currP->listener->devices;
					sfree((void *)&dtmp->id);
					sfree((void *)&dtmp->desc);
					currP->listener->devices = currP->listener->devices->next;
					sfree((void *)&dtmp);
				}
			}
			sfree((void *)&currP->listener->devices);
			sfree((void *)&currP->listener);
			sfree((void *)&currP);

			break;
		}
	}
}

void protocol_init(void) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	#include "protocol_init.h"
	void *handle = NULL;
	void (*init)(void);
	void (*compatibility)(struct module_t *module);
	char path[255];
	struct module_t module;
	char pilight_version[strlen(VERSION)+1];
	char pilight_commit[3];
	char *protocol_root = NULL;
	int check1 = 0, check2 = 0, valid = 1, protocol_root_free = 0;
	strcpy(pilight_version, VERSION);

	struct dirent *file = NULL;
	DIR *d = NULL;
	struct stat s;

	memset(pilight_commit, '\0', 3);

	if(settings_find_string("protocol-root", &protocol_root) != 0) {
		/* If no protocol root was set, use the default protocol root */
		if(!(protocol_root = malloc(strlen(PROTOCOL_ROOT)+1))) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		strcpy(protocol_root, PROTOCOL_ROOT);
		protocol_root_free = 1;
	}
	size_t len = strlen(protocol_root);
	if(protocol_root[len-1] != '/') {
		strcat(protocol_root, "/");
	}

	if((d = opendir(protocol_root))) {
		while((file = readdir(d)) != NULL) {
			stat(file->d_name, &s);
			/* Check if file */
			if(S_ISREG(s.st_mode) == 0) {
				if(strstr(file->d_name, ".so") != NULL) {
					valid = 1;
					memset(path, '\0', 255);
					sprintf(path, "%s%s", protocol_root, file->d_name);

					if((handle = dso_load(path))) {
						init = dso_function(handle, "init");
						compatibility = dso_function(handle, "compatibility");
						if(init && compatibility) {
							compatibility(&module);
							if(module.name && module.version && module.reqversion) {
								char ver[strlen(module.reqversion)+1];
								strcpy(ver, module.reqversion);

								if((check1 = vercmp(ver, pilight_version)) > 0) {
									valid = 0;
								}

								if(check1 == 0 && module.reqcommit) {
									char com[strlen(module.reqcommit)+1];
									strcpy(com, module.reqcommit);
									sscanf(HASH, "v%*[0-9].%*[0-9]-%[0-9]-%*[0-9a-zA-Z\n\r]", pilight_commit);

									if(strlen(pilight_commit) > 0 && (check2 = vercmp(com, pilight_commit)) > 0) {
										valid = 0;
									}
								}
								if(valid) {
									char tmp[strlen(module.name)+1];
									strcpy(tmp, module.name);
									protocol_remove(tmp);
									init();
									logprintf(LOG_DEBUG, "loaded protocol %s", file->d_name);
								} else {
									if(module.reqcommit) {
										logprintf(LOG_ERR, "protocol %s requires at least pilight v%s (commit %s)", file->d_name, module.reqversion, module.reqcommit);
									} else {
										logprintf(LOG_ERR, "protocol %s requires at least pilight v%s", file->d_name, module.reqversion);
									}
								}
							} else {
								logprintf(LOG_ERR, "invalid module %s", file->d_name);
							}
						}
					}
				}
			}
		}
		closedir(d);
	}
	if(protocol_root_free) {
		sfree((void *)&protocol_root);
	}
}

void protocol_register(protocol_t **proto) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	if(!(*proto = malloc(sizeof(struct protocol_t)))) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	(*proto)->options = NULL;
	(*proto)->devices = NULL;
	(*proto)->plslen = NULL;

	(*proto)->pulse = 0;
	(*proto)->rawlen = 0;
	(*proto)->minrawlen = 0;
	(*proto)->maxrawlen = 0;
	(*proto)->binlen = 0;
	(*proto)->lsb = 0;
	(*proto)->txrpt = 1;
	(*proto)->hwtype = 1;
	(*proto)->rxrpt = 1;
	(*proto)->multipleId = 1;
	(*proto)->config = 1;
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
	if(!(pnode->name = malloc(4))) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	pnode->next = protocols;
	protocols = pnode;
}

struct protocol_threads_t *protocol_thread_init(protocol_t *proto, struct JsonNode *param) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

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
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

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
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

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
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	if(proto->threads) {
		struct protocol_threads_t *tmp = NULL;
		while(proto->threads) {
			tmp = proto->threads;
			if(proto->threads->param) {
				json_delete(proto->threads->param);
			}
			proto->threads = proto->threads->next;
			sfree((void *)&tmp);
		}
		sfree((void *)&proto->threads);
	}
}

void protocol_set_id(protocol_t *proto, const char *id) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	if(!(proto->id = malloc(strlen(id)+1))) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(proto->id, id);
}

void protocol_plslen_add(protocol_t *proto, int plslen) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

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
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	struct protocol_devices_t *dnode = malloc(sizeof(struct protocol_devices_t));
	if(!dnode) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	if(!(dnode->id = malloc(strlen(id)+1))) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(dnode->id, id);
	if(!(dnode->desc = malloc(strlen(desc)+1))) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	strcpy(dnode->desc, desc);
	dnode->next	= proto->devices;
	proto->devices = dnode;
}

int protocol_device_exists(protocol_t *proto, const char *id) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

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
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	struct protocols_t *ptmp;
	struct protocol_devices_t *dtmp;
	struct protocol_plslen_t *ttmp;

	while(protocols) {
		ptmp = protocols;
		logprintf(LOG_DEBUG, "protocol %s", ptmp->listener->id);
		if(ptmp->listener->threadGC) {
			ptmp->listener->threadGC();
			logprintf(LOG_DEBUG, "stopped protocol threads");
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
		sfree((void *)&ptmp->listener);
		protocols = protocols->next;
		sfree((void *)&ptmp);
	}
	sfree((void *)&protocols);

	logprintf(LOG_DEBUG, "garbage collected protocol library");
	return EXIT_SUCCESS;
}
>>>>>>> upstream/development
