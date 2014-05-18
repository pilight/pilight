/*
	Copyright (C) 2013 CurlyMo

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

#ifndef _PROTOCOL_ARCTECH_SWITCH_H_
#define _PROTOCOL_ARCTECH_SWITCH_H_

struct protocol_t *arctech_switch;

void arctechSwInit(void);
void arctechSwCreateMessage(int id, int unit, int state, int all);
void arctechSwParseBinary(void);
int arctechSwCreateCode(JsonNode *code);
void arctechSwCreateLow(int s, int e);
void arctechSwCreateHigh(int s, int e);
void arctechSwClearCode(void);
void arctechSwCreateStart(void);
void arctechSwCreateId(int id);
void arctechSwCreateAll(int all);
void arctechSwCreateState(int state);
void arctechSwCreateUnit(int unit);
void arctechSwCreateFooter(void);
void arctechSwPrintHelp(void);

#endif
