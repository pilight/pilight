/*
	Copyright (C) 2014 CurlyMo & wo_rasp

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

#define QUIGG_BINLEN		21
#define QUIGG_PULSE_HIGH	2	// 5 1300
#define QUIGG_PULSE_LENGTH	700	// 260
#define QUIGG_PULSE_LOW		1	// 3 780
#define QUIGG_PULSE_LSB		0
#define QUIGG_RAWLEN		42

struct protocol_t *quigg_switch;

void quiggSwCreateMessage(int id, int state, int unit, int all);
void quiggSwParseCode(void);
void quiggSwCreateLow(int s, int e);
void quiggSwCreateHigh(int s, int e);
void quiggSwClearCode(void);
void quiggSwCreateId(int id);
void quiggSwCreateHeader(void);
void quiggSwCreateState(int state);
void quiggSwCreateFooter(void);
int quiggSwCreateCode(JsonNode *code);
void quiggSwPrintHelp(void);
void quiggSwInit(void);

#endif
