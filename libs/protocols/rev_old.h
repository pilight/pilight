/*
    Copyright (C) 2014 CurlyMo & easy12

    This file is part of pilight.

    pilight is free software: you can redistribute it and/or modify it under the
    terms of the GNU General Public License as published by the Free Software
    Foundation, either version 3 of the License, or (at your option) any later
    version.

    pilight is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
    A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with pilight. If not, see    <http://www.gnu.org/licenses/>
*/

#ifndef _PROTOCOL_REV_OLD_H_
#define _PROTOCOL_REV_OLD_H_

struct protocol_t *rev_old_switch;

void revOldInit(void);
void revOldCreateMessage(int id, int unit, int state);
void revOldParseBinary(void);
int revOldCreateCode(JsonNode *code);
void revOldCreateUnit(int unit);
void revOldCreateId(int id);
void revOldCreateState(int state);
void revOldPrintHelp(void);

#endif
