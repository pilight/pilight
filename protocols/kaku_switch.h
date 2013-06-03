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

#ifndef KAKU_SWITCH_H_
#define KAKU_SWITCH_H_

#include "protocol.h"
#include "binary.h"

protocol kaku_switch;

void kakuSwInit();
void kakuSwParseRaw();
void kakuSwParseCode();
int kakuSwParseBinary();
void kakuSwCreateCode(struct options *options);
void kakuSwCreateLow(int s, int e);
void kakuSwCreateHigh(int s, int e);
void kakuSwClearCode();
void kakuSwCreateStart();
void kakuSwCreateId(int id);
void kakuSwCreateAll(int all);
void kakuSwCreateState(int state);
void kakuSwCreateUnit(int unit);
void kakuSwCreateFooter();
void kakuSwPrintHelp();

#endif