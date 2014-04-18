/*
	Copyright (C) 2013 CurlyMo & Bram1337

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

#ifndef _PROTOCOL_QUIGG_SWITCH_H_
#define _PROTOCOL_QUIGG_SWITCH_H_

struct protocol_t *quigg_switch;

void quiGGSwCreateMessage(int id, int state, int unit, int all);
void quiGGSwParseCode(void);
void quiGGSwCreateLow(int s, int e);
void quiGGSwCreateHigh(int s, int e);
void quiGGSwClearCode(void);
void quiGGSwCreateId(int id);
void quiGGSwCreateStart(void);
void quiGGSwCreateState(int state);
void quiGGSwCreateFooter(void);
int quiGGSwCreateCode(JsonNode *code);
void quiGGSwPrintHelp(void);
void quiGGSwInit(void);

#endif
