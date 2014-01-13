/*
  Copyright (C) 2014 CurlyMo & lvdp

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
#include "arctech_door.h"

void arctechDoorCreateMessage(int id, int unit, int state, int all) {
  arctech_door->message = json_mkobject();
  json_append_member(arctech_door->message, "id", json_mknumber(id));
  if(all == 1) {
          json_append_member(arctech_door->message, "all", json_mknumber(all));
  } else {
          json_append_member(arctech_door->message, "unit", json_mknumber(unit));
  }

  if(state == 1) {
          json_append_member(arctech_door->message, "state", json_mkstring("open"));
  } else {
          json_append_member(arctech_door->message, "state", json_mkstring("closed"));
  }
}

void arctechDoorParseBinary(void) {
  int unit = binToDecRev(arctech_door->binary, 28, 31);
  int state = arctech_door->binary[27];
  int all = arctech_door->binary[26];
  int id = binToDecRev(arctech_door->binary, 0, 25);

  arctechDoorCreateMessage(id, unit, state, all);
}

void arctechDoorInit(void) {

  protocol_register(&arctech_door);
  protocol_set_id(arctech_door, "arctech_doors");
  protocol_device_add(arctech_door, "kaku_door", "KlikAanKlikUit Door Sensor");
  protocol_plslen_add(arctech_door, 294);

  arctech_door->devtype = SWITCH;
  arctech_door->hwtype = RF433;
  arctech_door->pulse = 4;
  arctech_door->rawlen = 148;
  arctech_door->lsb = 3;

  options_add(&arctech_door->options, 'a', "all", no_value, 0, NULL);
  options_add(&arctech_door->options, 'u', "unit", has_value, config_id, "^([0-9]{1}|[1][0-5])$");
  options_add(&arctech_door->options, 'i', "id", has_value, config_id, "^([0-9]{1,7}|[1-5][0-9]{7}|6([0-6][0-9]{6}|7(0[0-9]{5}|10([0-7][0-9]{3}|8([0-7][0-9]{2}|8([0-5][0-9]|6[0-3]))))))$");
  options_add(&arctech_door->options, 't', "open", no_value, config_state, NULL);
  options_add(&arctech_door->options, 'f', "closed", no_value, config_state, NULL);


  protocol_setting_add_string(arctech_door, "states", "open,closed");
  protocol_setting_add_number(arctech_door, "readonly", 1);

  arctech_door->parseBinary=&arctechDoorParseBinary;
}
