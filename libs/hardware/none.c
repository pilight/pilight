/*
	Copyright (C) 2013 - 2014 CurlyMo

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hardware.h"
#include "common.h"
#include "none.h"

int noneSend(int *code) {
	sleep(1);
	return EXIT_SUCCESS;
}

int noneReceive(void) {
	sleep(1);
	return EXIT_SUCCESS;
}

void noneInit(void) {
	hardware_register(&none);
	hardware_set_id(none, "none");
	none->type=NONE;
	none->receive=&noneReceive;
	none->send=&noneSend;
}

#ifdef MODULAR
void compatibility(const char **version, const char **commit) {
	*version = "4.0";
	*commit = "18";
}

void init(void) {
	noneInit();
}
#endif
