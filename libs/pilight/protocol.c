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

void protocol_add_device(protocol_t *proto, const char *id, const char *desc) {
	struct devices_t *dnode = malloc(sizeof(struct devices_t));
	dnode->id = malloc(strlen(id)+1);
	strcpy(dnode->id, id);
	dnode->desc = malloc(strlen(desc)+1);
	strcpy(dnode->desc, desc);
	dnode->next	= proto->devices;
	proto->devices = dnode;
}

void protocol_add_conflict(protocol_t *proto, const char *id) {
	struct conflicts_t *cnode = malloc(sizeof(struct conflicts_t));
	cnode->id = malloc(strlen(id)+1);
	strcpy(cnode->id, id);
	cnode->next	= proto->conflicts;
	proto->conflicts = cnode;
}

/* http://www.cs.bu.edu/teaching/c/linked-list/delete/ */
void protocol_remove_conflict(protocol_t **proto, const char *id) {
	struct conflicts_t *currP, *prevP;

	prevP = NULL;

	for(currP = (*proto)->conflicts; currP != NULL; prevP = currP, currP = currP->next) {

		if(strcmp(currP->id, id) == 0) {
			if(prevP == NULL) {
				(*proto)->conflicts = currP->next;
			} else {
				prevP->next = currP->next;
			}

			free(currP);

			break;
		}
	}
}

int protocol_has_device(protocol_t *proto, const char *id) {
	struct devices_t *temp = proto->devices;

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
	struct protocols_t *stmp;
	struct devices_t *dtmp;
	struct conflicts_t *ctmp;
	while(protocols) {
		stmp = protocols;
		free(stmp->listener->id);
		options_delete(stmp->listener->options);
		if(stmp->listener->devices) {
			while(stmp->listener->devices) {
				dtmp = stmp->listener->devices;
				free(dtmp->id);
				free(dtmp->desc);
				stmp->listener->devices = stmp->listener->devices->next;
				free(dtmp);
			}
		}
		free(stmp->listener->devices);
		if(stmp->listener->conflicts) {
			while(stmp->listener->conflicts) {
				ctmp = stmp->listener->conflicts;
				free(ctmp->id);
				stmp->listener->conflicts = stmp->listener->conflicts->next;
				free(ctmp);
			}
		}
		free(stmp->listener->conflicts);
		free(stmp->listener);
		protocols = protocols->next;
		free(stmp);
	}
	free(protocols);

	logprintf(LOG_DEBUG, "garbage collected protocol library");
	return EXIT_SUCCESS;
}
