/*
	Copyright (C) 2013 CurlyMo

	This file is part of the QPido.

    QPido is free software: you can redistribute it and/or modify it 
	under the terms of the GNU General Public License as published by 
	the Free Software Foundation, either version 3 of the License, or 
	(at your option) any later version.

    QPido is distributed in the hope that it will be useful, but 
	WITHOUT ANY WARRANTY; without even the implied warranty of 
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
	General Public License for more details.

    You should have received a copy of the GNU General Public License 
	along with QPido. If not, see <http://www.gnu.org/licenses/>
*/

#ifndef PROTOCOL_SARTANO_H_
#define PROTOCOL_SARTANO_H_

protocol_t sartano;

void sartanoInit(void);
void sartanoCreateMessage(int id, int unit, int state);
void sartanoParseBinary(void);
int sartanoCreateCode(JsonNode *code);
void sartanoCreateUnit(int unit);
void sartanoCreateId(int id);
void sartanoCreateState(int state);
void sartanoPrintHelp(void);

#endif
