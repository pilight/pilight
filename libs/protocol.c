/*
	Copyright (C) 2013 CurlyMo

	This file is part of the Raspberry Pi 433.92Mhz transceiver.

    Raspberry Pi 433.92Mhz transceiver is free software: you can redistribute
	it and/or modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation, either version 3 of the License,
	or (at your option) any later version.

    Raspberry Pi 433.92Mhz transceiver is distributed in the hope that it will
	be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Raspberry Pi 433.92Mhz transceiver. If not, see
	<http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "options.h"
#include "protocol.h"

void protocol_unregister(protocol_t *proto) {
	unsigned i;

	for(i=0; i<protocols.nr; ++i) {
		if(strcmp(protocols.listeners[i]->id,proto->id) == 0) {
		  protocols.nr--;
		  protocols.listeners[i] = protocols.listeners[protocols.nr];
		}
	}
}

void protocol_register(protocol_t *proto) {
	protocol_unregister(proto);
	protocols.listeners[protocols.nr++] = proto;
}