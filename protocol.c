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

#include "protocol.h"

void protocol_unregister(protocol *proto) {
	unsigned i;

	for(i=0; i<protos.nr; ++i) {
		if(strcmp(protos.listeners[i]->id,proto->id) == 0) {
		  protos.nr--;
		  protos.listeners[i] = protos.listeners[protos.nr];
		}
	}
}

void protocol_register(protocol *proto) {
	protocol_unregister(proto);
	protos.listeners[protos.nr++] = proto;
}