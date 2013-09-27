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

#include "options.h"
#include "protocol.h"
#include "log.h"

void protocol_register(protocol_t **proto) {
	*proto = malloc(sizeof(struct protocol_t));
	(*proto)->options = NULL;
	(*proto)->devices = NULL;
	(*proto)->conflicts = NULL;
	(*proto)->settings = NULL;

	(*proto)->header = 0;
	(*proto)->footer = 0;
	(*proto)->pulse = 0;
	(*proto)->rawLength = 0;
	(*proto)->binLength = 0;
	(*proto)->lsb = 0;
	(*proto)->bit = 0;
	(*proto)->recording = 0;
	(*proto)->send_repeats = 1;
	(*proto)->receive_repeats = 1;
	(*proto)->parseRaw = NULL;
	(*proto)->parseBinary = NULL;
	(*proto)->parseCode = NULL;
	(*proto)->createCode = NULL;
	(*proto)->printHelp = NULL;
	(*proto)->message = NULL;

	memset(&(*proto)->raw[0], 0, sizeof((*proto)->raw));
	memset(&(*proto)->code[0], 0, sizeof((*proto)->code));
	memset(&(*proto)->pCode[0], 0, sizeof((*proto)->pCode));
	memset(&(*proto)->binary[0], 0, sizeof((*proto)->binary));
	
	struct protocols_t *pnode = malloc(sizeof(struct protocols_t));
	pnode->listener = *proto;
	pnode->next = protocols;
	protocols = pnode;
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

			free(currP->id);
			free(currP);

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
	free(tmp_settings);
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
	free(tmp_settings);
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
	free(tmp_settings);
	return 1;	
}

void protocol_setting_add_string(protocol_t *proto, const char *name, const char *value) {
	int error = 0;
	switch(proto->type) {
		case DIMMER:
			if(!(!strcmp(name, "states"))) {
				logprintf(LOG_ERR, "protocol \"%s\" setting \"%s\" is invalid for dimmer type", proto->id, name);
				error = 1;
			}
		break;
		case RELAY:
			if(!(!strcmp(name, "default") || !strcmp(name, "states"))) {
				logprintf(LOG_ERR, "protocol \"%s\" setting \"%s\" is invalid for relay type", proto->id, name);
				error = 1;
			}
		break;
		case SWITCH:
			if(!(!strcmp(name, "states"))) {
				logprintf(LOG_ERR, "protocol \"%s\" setting \"%s\" is invalid for switch type", proto->id, name);
				error = 1;
			}
		break;
		case WEATHER:		
		case RAW:		
		default:
		break;
	}
	if(!error) {
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
	}
}

void protocol_setting_add_number(protocol_t *proto, const char *name, int value) {
	int error = 0;
	switch(proto->type) {
		case DIMMER:
			if(!(!strcmp(name, "max") || !strcmp(name, "min"))) {
				logprintf(LOG_ERR, "protocol \"%s\" setting \"%s\" is invalid for dimmer type", proto->id, name);
				error = 1;
			}
		break;
		case WEATHER:
			if(!(!strcmp(name, "decimals") || !strcmp(name, "battery") 
			     || !strcmp(name, "temperature") || !strcmp(name, "humidity"))) {
				logprintf(LOG_ERR, "protocol \"%s\" setting \"%s\" is invalid for weather type", proto->id, name);
				error = 1;
			}
		break;
		case SWITCH:
		case RELAY:
		case RAW:		
		default:
		break;
	}
	if(!error) {
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
	free(tmp_settings);
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
	free(tmp_settings);
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

			free(currP->name);
			free(currP->cur_value);
			free(currP);

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
	free(temp);
	return 1;
}

int protocol_gc(void) {
	struct protocols_t *ptmp;
	struct protocol_devices_t *dtmp;
	struct protocol_conflicts_t *ctmp;
	struct protocol_settings_t *stmp;

	while(protocols) {
		ptmp = protocols;
		free(ptmp->listener->id);
		options_delete(ptmp->listener->options);
		if(ptmp->listener->devices) {
			while(ptmp->listener->devices) {
				dtmp = ptmp->listener->devices;
				free(dtmp->id);
				free(dtmp->desc);
				ptmp->listener->devices = ptmp->listener->devices->next;
				free(dtmp);
			}
		}
		free(ptmp->listener->devices);
		if(ptmp->listener->conflicts) {
			while(ptmp->listener->conflicts) {
				ctmp = ptmp->listener->conflicts;
				free(ctmp->id);
				ptmp->listener->conflicts = ptmp->listener->conflicts->next;
				free(ctmp);
			}
		}
		free(ptmp->listener->conflicts);
		if(ptmp->listener->settings) {
			while(ptmp->listener->settings) {
				stmp = ptmp->listener->settings;
				free(stmp->name);
				free(stmp->cur_value);
				free(stmp->old_value);
				ptmp->listener->settings = ptmp->listener->settings->next;
				free(stmp);
			}
		}
		free(ptmp->listener->settings);
		free(ptmp->listener);
		protocols = protocols->next;
		free(ptmp);
	}
	free(protocols);

	logprintf(LOG_DEBUG, "garbage collected protocol library");
	return EXIT_SUCCESS;
}