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

#ifndef _PROTOCOL_SARTANO_H_
#define _PROTOCOL_SARTANO_H_

struct protocol_t *sartano;

void sartanoInit(void);
void sartanoCreateMessage(int systemcode, int unitcode, int state);
void sartanoParseBinary(void);
int sartanoCreateCode(JsonNode *code);
void sartanoCreateLow(int s, int e);
void sartanoCreateHigh(int s, int e);
void sartanoClearCode(void);
void sartanoCreateSystemCode(int systemcode);
void sartanoCreateUnitCode(int unitcode);
void sartanoCreateState(int state);
void sartanoCreateFooter(void);
void sartanoPrintHelp(void);

#endif
