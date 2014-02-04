/*
	Copyright (C) 2014 1000io

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

#ifndef _PROTOCOL_GENWATTMETER_H_
#define _PROTOCOL_GENWATTMETER_H_

struct protocol_t *generic_wattmeter;

void genWattmeterInit(void);
void genWattmeterCreateMessage(int id, int watt, int price, char *coin)
int genWattmeterCreateCode(JsonNode *code);
void genWattmeterPrintHelp(void);

#endif
