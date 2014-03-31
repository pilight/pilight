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

#ifndef _PROTOCOL_ELRO_AD_H_
#define _PROTOCOL_ELRO_AD_H_

struct protocol_t *elro_ad;

void elroADInit(void);
void elroADCreateMessage(unsigned long long systemcode, int unitcode, int state, int groupEnabled);
void elroADParseCode(void);
int elroADCreateCode(JsonNode *code);
void elroADCreateLow(int s, int e);
void elroADCreateHigh(int s, int e);
void elroADClearCode(void);
void elroADCreateSystemCode(unsigned long long systemcode);
void elroADCreateGroupCode(int group);
void elroADCreateUnitCode(int unitcode);
void elroADCreateState(int state);
void elroADCreatePreamble(void);
void elroADCreateFooter(void);
void elroADPrintHelp(void);

#endif
