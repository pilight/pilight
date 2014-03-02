/*
	Copyright (C) 2014 CurlyMo

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
#include <stdarg.h>
#include <unistd.h>

#include "../../pilight.h"
#include "common.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "sensor_1527.h"

void sensor1527ParseCode(void) {
	int i = 0, x = 0;
	int id;
	int state;

	for(i=1;i<sensor_1527->rawlen-1;i+=2) {
		sensor_1527->binary[x++] = sensor_1527->code[i];
	}

	id = binToDecRev(sensor_1527->binary, 0, 19);
	state = binToDecRev(sensor_1527->binary, 20, 23);

	sensor_1527->message = json_mkobject();
	json_append_member(sensor_1527->message, "id", json_mknumber(id));
	if(state == 6)
		json_append_member(sensor_1527->message, "state", json_mkstring("on"));
	else
		json_append_member(sensor_1527->message, "state", json_mkstring("off"));
}
	
void sensor1527Init(void) {

	protocol_register(&sensor_1527);
	protocol_set_id(sensor_1527, "sensor_1527");
	protocol_device_add(sensor_1527, "sensor_1527", "ev1527 Based Sensor");
	protocol_plslen_add(sensor_1527, 256);
	protocol_conflict_add(sensor_1527, "rev_switch");
	sensor_1527->devtype = SWITCH;
	sensor_1527->hwtype = RF433;
	sensor_1527->pulse = 3;
	sensor_1527->rawlen = 50;
	sensor_1527->binlen = 24;
	
	options_add(&sensor_1527->options, 'i', "id", has_value, config_id, "[0-9]");
	options_add(&sensor_1527->options, 't', "on", no_value, config_state, NULL);
	options_add(&sensor_1527->options, 'f', "off", no_value, config_state, NULL);
	
	protocol_setting_add_string(sensor_1527, "states", "on,off");
	protocol_setting_add_number(sensor_1527, "readonly", 0);

	sensor_1527->parseCode=&sensor1527ParseCode;
}
