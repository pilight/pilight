/*
	Copyright (C) 2017 Stefan Seegel

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

#include "868raspyrfm.h"

static rfm_settings_t rfmsettings = {
	0, //default SPI channel
	FREQTOFREG(868.35) //MHz
};

static unsigned short raspyrfm868Settings(JsonNode *json) {
	return raspyrfmSettings(json, &rfmsettings); 
}

static unsigned short raspyrfm868HwInit(void) {
	return raspyrfmHwInit(&rfmsettings);
}

static int raspyrfm868Send(int *code, int rawlen, int repeats) {
	return raspyrfmSend(code, rawlen, repeats, &rfmsettings);
}

static int raspyrfm868Receive(void) {
	sleep(1);
	return EXIT_SUCCESS;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void raspyrfm868Init(void) {
	hardware_register(&raspyrfm868);
	hardware_set_id(raspyrfm868, "raspyrfm868");
	
	options_add(&raspyrfm868->options, 'c', "spi_ch", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]+$");

	raspyrfm868->hwtype=RF868;
	raspyrfm868->comtype=COMOOK;
	raspyrfm868->settings=&raspyrfm868Settings;
	raspyrfm868->init=&raspyrfm868HwInit;
	raspyrfm868->sendOOK=&raspyrfm868Send;
	raspyrfm868->receiveOOK=&raspyrfm868Receive;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "RaspyRfm868";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	raspyrfm868Init();
}
#endif
