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
#include "arctech_contact.h"

void arctechContactCreateMessage(int id, int unit, int state, int all) {
  arctech_contact->message = json_mkobject();
  json_append_member(arctech_contact->message, "id", json_mknumber(id));
  if(all == 1) {
          json_append_member(arctech_contact->message, "all", json_mknumber(all));
  } else {
          json_append_member(arctech_contact->message, "unit", json_mknumber(unit));
  }

  if(state == 1) {
          json_append_member(arctech_contact->message, "state", json_mkstring("opened"));
  } else {
          json_append_member(arctech_contact->message, "state", json_mkstring("closed"));
  }
}

void arctechContactParseBinary(void) {
  int unit = binToDecRev(arctech_contact->binary, 28, 31);
  int state = arctech_contact->binary[27];
  int all = arctech_contact->binary[26];
  int id = binToDecRev(arctech_contact->binary, 0, 25);

  arctechContactCreateMessage(id, unit, state, all);
}

void arctechContactInit(void) {

  protocol_register(&arctech_contact);
  protocol_set_id(arctech_contact, "arctech_contact");
  protocol_device_add(arctech_contact, "kaku_contact", "KlikAanKlikUit Contact Sensor");
  protocol_plslen_add(arctech_contact, 294);

  arctech_contact->devtype = SWITCH;
  arctech_contact->hwtype = RF433;
  arctech_contact->pulse = 4;
  arctech_contact->rawlen = 148;
  arctech_contact->lsb = 3;

  options_add(&arctech_contact->options, 'a', "all", no_value, 0, NULL);
  options_add(&arctech_contact->options, 'u', "unit", has_value, config_id, "^([0-9]{1}|[1][0-5])$");
  options_add(&arctech_contact->options, 'i', "id", has_value, config_id, "^([0-9]{1,7}|[1-5][0-9]{7}|6([0-6][0-9]{6}|7(0[0-9]{5}|10([0-7][0-9]{3}|8([0-7][0-9]{2}|8([0-5][0-9]|6[0-3]))))))$");
  options_add(&arctech_contact->options, 't', "opened", no_value, config_state, NULL);
  options_add(&arctech_contact->options, 'f', "closed", no_value, config_state, NULL);


  protocol_setting_add_string(arctech_contact, "states", "opened,closed");
  protocol_setting_add_number(arctech_contact, "readonly", 1);

  arctech_contact->parseBinary=&arctechContactParseBinary;
}
