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

#ifndef _PROTOCOL_MUMBI_H_
#define _PROTOCOL_MUMBI_H_

struct protocol_t *mumbi;

void mumbiInit(void);
void mumbiCreateMessage(int systemcode, int unitcode, int state);
void mumbiParseBinary(void);
int mumbiCreateCode(JsonNode *code);
void mumbiCreateLow(int s, int e);
void mumbiCreateHigh(int s, int e);
void mumbiClearCode(void);
void mumbiCreateSystemCode(int systemcode);
void mumbiCreateUnitCode(int unitcode);
void mumbiCreateState(int state);
void mumbiCreateFooter(void);
void mumbiPrintHelp(void);

#endif
