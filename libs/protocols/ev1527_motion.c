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
#include "ev1527_motion.h"

void ev1527MotionCreateMessage(int id, int state) {
  ev1527_motion->message = json_mkobject();
  json_append_member(ev1527_motion->message, "id", json_mknumber(id));
  if(state == 1) {
          json_append_member(ev1527_motion->message, "state", json_mkstring("on"));
  } else {
          json_append_member(ev1527_motion->message, "state", json_mkstring("off"));
  }
}

void ev1527MotionParseCode(void) {
	int i = 0, x = 0;
	int id;
	int state;

	for(i=1;i<ev1527_motion->rawlen-1;i+=2) {
		ev1527_motion->binary[x++] = ev1527_motion->code[i];
	}

	id = binToDecRev(ev1527_motion->binary, 0, 19);
	state = ev1527_motion->binary[21];

	ev1527MotionCreateMessage(id, state);
}

int ev1527MotionCreateCode(JsonNode *code) {
    int id = -1;
	int state = -1;
	int tmp;

	json_find_number(code, "id", &id);
	if(json_find_number(code, "off", &tmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &tmp) == 0)
		state=1;

	if(id == -1 || state == -1) {
		logprintf(LOG_ERR, "ev1527_motion: insufficient number of arguments");
		return EXIT_FAILURE;
	} else {
		ev1527MotionCreateMessage(id, state);
	}

	return EXIT_SUCCESS;
}

void ev1527MotionPrintHelp(void) { 
   	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
}

void ev1527MotionInit(void) {

  protocol_register(&ev1527_motion);
  protocol_set_id(ev1527_motion, "ev1527_motion");
  protocol_device_add(ev1527_motion, "ev1527_motion", "EV1527 based Motion Sensor");
  protocol_plslen_add(ev1527_motion, 305);
  protocol_plslen_add(ev1527_motion, 306);
  protocol_conflict_add(ev1527_motion, "rev_switch");
  
  ev1527_motion->devtype = SWITCH;
  ev1527_motion->hwtype = RF433;
  ev1527_motion->pulse = 3;
  ev1527_motion->rawlen = 50;

  options_add(&ev1527_motion->options, 't', "on", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
  options_add(&ev1527_motion->options, 'f', "off", OPTION_NO_VALUE, CONFIG_STATE, JSON_STRING, NULL, NULL);
  options_add(&ev1527_motion->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]{1,})$");
  options_add(&ev1527_motion->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

  ev1527_motion->parseBinary=&ev1527MotionParseCode;
  ev1527_motion->createCode=&ev1527MotionCreateCode;
  ev1527_motion->printHelp=&ev1527MotionPrintHelp;
}
