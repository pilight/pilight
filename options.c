/*	
	Copyright (C) 2013 CurlyMo
	
	This file is part of the Raspberry Pi 433.92Mhz transceiver.

    Raspberry Pi 433.92Mhz transceiver is free software: you can redistribute 
	it and/or modify it under the terms of the GNU General Public License as 
	published by the Free Software Foundation, either version 3 of the License, 
	or (at your option) any later version.

    Raspberry Pi 433.92Mhz transceiver is distributed in the hope that it will 
	be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Raspberry Pi 433.92Mhz transceiver. If not, see 
	<http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "options.h"

struct option *setOptions(struct option *option) {
	int nr = 0, i = 0;
	struct option *backup_options;
	backup_options=option;
	while(backup_options->name != NULL) {
		backup_options++;
		nr++;
	}
	backup_options=option;
	struct option *new_options = malloc(sizeof(struct option)*nr);
	for(i=0;i<nr;i++) {
		new_options[i].name=backup_options->name;
		new_options[i].flag=backup_options->flag;
		new_options[i].has_arg=backup_options->has_arg;
		new_options[i].val=backup_options->val;
		backup_options++;
	}
	return new_options;
}

char *getOption(struct options *options, int id) {
	while(options != NULL) {
		if(options->id == id) {
			return options->value;
		}
		options = options->next;
	}
	return "\0";
}
