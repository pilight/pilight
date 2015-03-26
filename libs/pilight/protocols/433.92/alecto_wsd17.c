/*
	Copyright (C) 2014 CurlyMo

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
#include <math.h>

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "alecto_wsd17.h"

typedef struct alecto_wsd17_settings_t {
	double id;
	double temp;
	struct alecto_wsd17_settings_t *next;
} alecto_wsd17_settings_t;

static struct alecto_wsd17_settings_t *alecto_wsd17_settings = NULL;

static void alectoWSD17ParseCode(void) {
	int i = 0, x = 0, id = 0;
	double temp_offset = 0.0, temperature = 0.0;

	for(i=1;i<alecto_wsd17->rawlen-1;i+=2) {
		alecto_wsd17->binary[x++] = alecto_wsd17->code[i];
	}

	id = binToDecRev(alecto_wsd17->binary, 0, 11);
	temperature = binToDecRev(alecto_wsd17->binary, 16, 27);

	struct alecto_wsd17_settings_t *tmp = alecto_wsd17_settings;
	while(tmp) {
		if(fabs(tmp->id-id) < EPSILON) {
			temp_offset = tmp->temp;
			break;
		}
		tmp = tmp->next;
	}

	temperature += temp_offset;

	alecto_wsd17->message = json_mkobject();
	json_append_member(alecto_wsd17->message, "id", json_mknumber(id, 0));
	json_append_member(alecto_wsd17->message, "temperature", json_mknumber(temperature/10, 1));
}

static int alectoWSD17CheckValues(struct JsonNode *jvalues) {
	struct JsonNode *jid = NULL;

	if((jid = json_find_member(jvalues, "id"))) {
		struct alecto_wsd17_settings_t *snode = NULL;
		struct JsonNode *jchild = NULL;
		struct JsonNode *jchild1 = NULL;
		double id = -1;
		int match = 0;

		jchild = json_first_child(jid);
		while(jchild) {
			jchild1 = json_first_child(jchild);
			while(jchild1) {
				if(strcmp(jchild1->key, "id") == 0) {
					id = jchild1->number_;
				}
				jchild1 = jchild1->next;
			}
			jchild = jchild->next;
		}

		struct alecto_wsd17_settings_t *tmp = alecto_wsd17_settings;
		while(tmp) {
			if(fabs(tmp->id-id) < EPSILON) {
				match = 1;
				break;
			}
			tmp = tmp->next;
		}

		if(!match) {
			if(!(snode = MALLOC(sizeof(struct alecto_wsd17_settings_t)))) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			snode->id = id;
			snode->temp = 0;

			json_find_number(jvalues, "temperature-offset", &snode->temp);

			snode->next = alecto_wsd17_settings;
			alecto_wsd17_settings = snode;
		}
	}
	return 0;
}

static void alectoWSD17GC(void) {
	struct alecto_wsd17_settings_t *tmp = NULL;
	while(alecto_wsd17_settings) {
		tmp = alecto_wsd17_settings;
		alecto_wsd17_settings = alecto_wsd17_settings->next;
		FREE(tmp);
	}
	if(alecto_wsd17_settings != NULL) {
		FREE(alecto_wsd17_settings);
	}
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void alectoWSD17Init(void) {
	protocol_register(&alecto_wsd17);
	protocol_set_id(alecto_wsd17, "alecto_wsd17");
	protocol_device_add(alecto_wsd17, "alecto_wsd17", "Alecto WSD-17 Weather Stations");
	protocol_plslen_add(alecto_wsd17, 270);
	alecto_wsd17->devtype = WEATHER;
	alecto_wsd17->hwtype = RF433;
	alecto_wsd17->pulse = 14;
	alecto_wsd17->rawlen = 74;
	alecto_wsd17->lsb = 3;

	options_add(&alecto_wsd17->options, 't', "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&alecto_wsd17->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "[0-9]");

	// options_add(&alecto_wsd17->options, 0, "decimals", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&alecto_wsd17->options, 0, "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&alecto_wsd17->options, 0, "temperature-offset", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_NUMBER, (void *)0, "[0-9]");
	options_add(&alecto_wsd17->options, 0, "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	alecto_wsd17->parseCode=&alectoWSD17ParseCode;
	alecto_wsd17->checkValues=&alectoWSD17CheckValues;
	alecto_wsd17->gc=&alectoWSD17GC;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "alecto_wsd17";
	module->version = "0.10";
	module->reqversion = "5.0";
	module->reqcommit = "187";
}

void init(void) {
	alectoWSD17Init();
}
#endif
