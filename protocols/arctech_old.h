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

#ifndef PROTOCOL_ARCTECH_OLD_H_
#define PROTOCOL_ARCTECH_OLD_H_

protocol_t arctech_old;

void arctechOldInit();
void arctechOldCreateMessage(int id, int unit, int state);
void arctechOldParseBinary();
void arctechOldCreateCode(struct options_t *options);
void arctechOldCreateUnit(int unit);
void arctechOldCreateId(int id);
void arctechOldCreateState(int state);
void arctechOldPrintHelp();

#endif
