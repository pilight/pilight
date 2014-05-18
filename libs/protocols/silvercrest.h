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

#ifndef _PROTOCOL_SILVERCREST_H_
#define _PROTOCOL_SILVERCREST_H_

struct protocol_t *silvercrest;

void silvercrestInit(void);
void silvercrestCreateMessage(int systemcode, int unitcode, int state);
void silvercrestParseBinary(void);
int silvercrestCreateCode(JsonNode *code);
void silvercrestCreateLow(int s, int e);
void silvercrestCreateHigh(int s, int e);
void silvercrestClearCode(void);
void silvercrestCreateSystemCode(int systemcode);
void silvercrestCreateUnitCode(int unitcode);
void silvercrestCreateState(int state);
void silvercrestCreateFooter(void);
void silvercrestPrintHelp(void);

#endif
