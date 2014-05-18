/*
	Copyright (C) 2013 CurlyMo, neevedr

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

#ifndef _PROTOCOL_REV2_H_
#define _PROTOCOL_REV2_H_

struct protocol_t *rev2_switch;

void rev2Init(void);
void rev2CreateMessage(char *id, int unit, int state);
void rev2ParseCode(void);
int rev2CreateCode(JsonNode *code);
void rev2CreateUnit(int unit);
void rev2CreateId(char *id);
void rev2CreateState(int state);
void rev2PrintHelp(void);

#endif
