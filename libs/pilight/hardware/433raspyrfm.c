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

#include "433raspyrfm.h"

static rfm_settings_t rfmsettings = {
	0, //default SPI channel
	FREQTOFREG(433.920) //MHz
};

static unsigned short raspyrfm433Settings(JsonNode *json) {
	return raspyrfmSettings(json, &rfmsettings); 
}

static unsigned short raspyrfm433HwInit(void) {
	return raspyrfmHwInit(&rfmsettings);
}

static int raspyrfm433Send(int *code, int rawlen, int repeats) {
	return raspyrfmSend(code, rawlen, repeats, &rfmsettings);
}

static int raspyrfm433Receive(void) {
	sleep(1);
	return EXIT_SUCCESS;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void raspyrfm433Init(void) {
	hardware_register(&raspyrfm433);
	hardware_set_id(raspyrfm433, "raspyrfm433");
	
	options_add(&raspyrfm433->options, 'c', "spi_ch", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]+$");

	raspyrfm433->hwtype=RF433;
	raspyrfm433->comtype=COMOOK;
	raspyrfm433->settings=&raspyrfm433Settings;
	raspyrfm433->init=&raspyrfm433HwInit;
	raspyrfm433->sendOOK=&raspyrfm433Send;
	raspyrfm433->receiveOOK=&raspyrfm433Receive;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "RaspyRfm433";
	module->version = "1.0";
	module->reqversion = "5.0";
	module->reqcommit = NULL;
}

void init(void) {
	raspyrfm433Init();
}
#endif
