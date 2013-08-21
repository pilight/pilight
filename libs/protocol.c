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

void protocol_register(protocol_t **proto) {
	*proto = malloc(sizeof(struct protocol_t));
	(*proto)->options = NULL;

	struct protocols_t *pnode = malloc(sizeof(struct protocols_t));
	pnode->listener = *proto;
	pnode->next = protocols;
	protocols = pnode;
}

void protocol_add_device(protocol_t *proto, const char *id, const char *desc) {
	struct devices_t *dnode = malloc(sizeof(struct devices_t));
	dnode->id = strdup(id);
	dnode->desc = strdup(desc);
	dnode->next	= proto->devices;
	proto->devices = dnode;
}

void protocol_add_conflict(protocol_t *proto, const char *id) {
	struct conflicts_t *cnode = malloc(sizeof(struct conflicts_t));
	cnode->id = strdup(id);
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
