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

#ifndef _PROTOCOL_ARCTECH_DIMMER_H_
#define _PROTOCOL_ARCTECH_DIMMER_H_

struct protocol_t *arctech_dimmer;

void arctechDimInit(void);
void arctechDimCreateMessage(int id, int unit, int state, int all, int dimlevel);
void arctechDimParseBinary(void);
int arctechDimCheckSettings(JsonNode *code);
int arctechDimCreateCode(JsonNode *code);
void arctechDimCreateLow(int s, int e);
void arctechDimCreateHigh(int s, int e);
void arctechDimClearCode(void);
void arctechDimCreateStart(void);
void arctechDimCreateId(int id);
void arctechDimCreateAll(int all);
void arctechDimCreateState(int state);
void arctechDimCreateUnit(int unit);
void arctechDimCreateDimlevel(int dimlevel);
void arctechDimCreateFooter(void);
void arctechDimPrintHelp(void);

#endif
