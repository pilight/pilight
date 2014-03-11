/*
  Copyright (C) 2014 CurlyMo & Meloen

  This file is part of pilight.

    pilight is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version.

    pilight is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
  A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with pilight. If not, see <http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../pilight.h"
#include "common.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "ev1527_contact.h"

void ev1527ContactCreateMessage(int id, int state) {
  ev1527_contact->message = json_mkobject();
  json_append_member(ev1527_contact->message, "id", json_mknumber(id));
  if(state == 1) {
          json_append_member(ev1527_contact->message, "state", json_mkstring("opened"));
  } else {
          json_append_member(ev1527_contact->message, "state", json_mkstring("closed"));
  }
}

void ev1527ContactParseCode(void) {
	int i = 0, x = 0;
	int id;
	int state;

	for(i=1;i<ev1527_contact->rawlen-1;i+=2) {
		ev1527_contact->binary[x++] = ev1527_contact->code[i];
	}

	id = binToDecRev(ev1527_contact->binary, 0, 19);
	state = ev1527_contact->binary[21];

	ev1527ContactCreateMessage(id, state);
}

int ev1527ContactCreateCode(JsonNode *code) {
	int id = -1;
	int state = -1;
	int tmp;

	json_find_number(code, "id", &id);
	if(json_find_number(code, "closed", &tmp) == 0)
		state=0;
	else if(json_find_number(code, "opened", &tmp) == 0)
		state=1;

	if(id == -1 || state == -1) {
		logprintf(LOG_ERR, "ev1527_contact: insufficient number of arguments");
		return EXIT_FAILURE;
	} else {
		ev1527ContactCreateMessage(id, state);
	}

	return EXIT_SUCCESS;
}

void ev1527ContactPrintHelp(void) { 
   	printf("\t -t --opened\t\t\tsend an opened signal\n");
	printf("\t -f --closed\t\t\tsend an closed signal\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

void ev1527ContactInit(void) {

  protocol_register(&ev1527_contact);
  protocol_set_id(ev1527_contact, "ev1527_contact");
  protocol_device_add(ev1527_contact, "ev1527_contact", "EV1527 Based Contact Sensor");
  protocol_plslen_add(ev1527_contact, 256);
  protocol_conflict_add(ev1527_contact, "rev_switch");
  
  ev1527_contact->devtype = SWITCH;
  ev1527_contact->hwtype = RF433;
  ev1527_contact->pulse = 3;
  ev1527_contact->rawlen = 50;

  options_add(&ev1527_contact->options, 't', "opened", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
  options_add(&ev1527_contact->options, 'f', "closed", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
  options_add(&ev1527_contact->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]{1,})$");
  options_add(&ev1527_contact->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

  ev1527_contact->parseBinary=&ev1527ContactParseCode;
  ev1527_contact->createCode=&ev1527ContactCreateCode;
  ev1527_contact->printHelp=&ev1527ContactPrintHelp;
}
