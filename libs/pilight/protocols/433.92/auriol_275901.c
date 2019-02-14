/*
	Copyright (C) 2014 CurlyMo & Tommybear1979

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

//	PH 2017-10-14 0.1.4.0 - First working version battery not implemented, in some segments temperature may vary from displayed, do not know why. 

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
#include "auriol_275901.h"
//
// Protocol auriol_275901 characteristics: SYNC bit: 471/8007, Logical 0: 471/1884, Logical 1: 471/4239 //change PH 2017-08-07
//
#define PULSE_MULTIPLIER	16                                   //?
#define AVG_PULSE_LENGTH	471                      //change PH 2017-08-08
#define MIN_PULSE_LENGTH	AVG_PULSE_LENGTH * 0.75  //change PH 2017-08-08
#define MAX_PULSE_LENGTH	AVG_PULSE_LENGTH * 1.25  //change PH 2017-08-08
#define ZERO_PULSE		4710  //change PH 2017-08-07
#define ONE_PULSE					2355  //change PH 2017-08-07
#define AVG_PULSE					(ZERO_PULSE+ONE_PULSE)/2
#define RAW_LENGTH				82    //change PH 2017-08-07

typedef struct settings_t {
	double id;
	double temp;
	double humi;
	struct settings_t *next;
} settings_t;

static struct settings_t *settings = NULL;

static int validate(void) {
	if(auriol_275901->rawlen == RAW_LENGTH) {
		if(auriol_275901->raw[auriol_275901->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   auriol_275901->raw[auriol_275901->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)) {
			return 0;
		}
	}
	return -1;
}

static void parseCode(void) {
	int i = 0, x = 0, type = 0, id = 0, binary[RAW_LENGTH/2];
	int temp_offset = 0.0; 
	double humi_offset = 0.0;
	double humidity = 0.0, temperature = 0.0;
	int battery = 0;

	int checksum = 1;
	int channel = 0;
	int temp_in_fh = 0;

	if(auriol_275901->rawlen>RAW_LENGTH) {
		logprintf(LOG_ERR, "auriol_275901: parsecode - invalid parameter passed %d", auriol_275901->rawlen);
		return;
	}

	for(x=1;x<auriol_275901->rawlen;x+=2) {
		if(auriol_275901->raw[x] > AVG_PULSE) {
			//logprintf(LOG_INFO, "auriol_275901: parsecode binary[%d] = 1", i);
			binary[i++] = 1;			
		} else {
			//logprintf(LOG_INFO, "auriol_275901: parsecode binary[%d] = 0", i);
			binary[i++] = 0;
		}
	}

	struct settings_t *tmp = settings;
	while(tmp) {
		if(fabs(tmp->id-id) < EPSILON){
			humi_offset = tmp->humi;
			temp_offset = tmp->temp;
			break;
		}
		tmp = tmp->next;
	}
  	
	auriol_275901->message = json_mkobject();	
	
	ushort h0 = binToDecRev(binary, 28, 31);
	ushort h1 = binToDecRev(binary, 32, 35);
		
	temp_in_fh = binToDecRev(binary, 16, 27); //sets temperature offset      							
	temperature = (0x4c4 - temp_in_fh) * 5.0/9.0/-10;	//thanks to stuntteam
	id = binToDec(binary, 0, 7);
	humidity = h0 * 10.0 + h1;		
	channel = binToDecRev(binary, 36, 39);			
	
	
	//logprintf(LOG_INFO, "auriol_275901: parsecode debug t0 = %d t1 = %d / 10 h0 = %d * 10 h1 = %d temp_offset = %d", t0, t1, h0, h1, temp_offset);
	//logprintf(LOG_INFO, "auriol_275901: parsecode debug id = %d hum = %f ch = %d temp = %f", id, humidity, channel, temperature);			
	//logprintf(LOG_INFO, "auriol_275901: parsecode debug temp_in_fh=%d %d %f hum=%f", temp_in_fh, 0x4c4 - temp_in_fh, (0x4c4 - temp_in_fh) * 5.0/9.0/-10, humidity);
	
	json_append_member(auriol_275901->message, "id", json_mknumber(id, 0));
	json_append_member(auriol_275901->message, "temperature", json_mknumber(temperature, 1));
	json_append_member(auriol_275901->message, "humidity", json_mknumber(humidity, 1));
	json_append_member(auriol_275901->message, "battery", json_mknumber(battery, 0));
	json_append_member(auriol_275901->message, "channel", json_mknumber(channel, 0));
}

static int checkValues(struct JsonNode *jvalues) {
	struct JsonNode *jid = NULL;	

	if((jid = json_find_member(jvalues, "id"))) {
		struct settings_t *snode = NULL;
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

		struct settings_t *tmp = settings;
		while(tmp) {
			if(fabs(tmp->id-id) < EPSILON) {
				match = 1;
				break;
			}
			tmp = tmp->next;
		}

		if(match == 0) {
			if((snode = MALLOC(sizeof(struct settings_t))) == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(EXIT_FAILURE);
			}
			snode->id = id;
			snode->temp = 0;
			snode->humi = 0;

			json_find_number(jvalues, "temperature-offset", &snode->temp);
			json_find_number(jvalues, "humidity-offset", &snode->humi);

			snode->next = settings;
			settings = snode;
		}
	}
	return 0;
}

static void gc(void) {	
	struct settings_t *tmp = NULL;
	while(settings) {
		tmp = settings;
		settings = settings->next;
		FREE(tmp);
	}
	if(settings != NULL) {
		FREE(settings);
	}
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif

void auriol275901Init(void) {

	protocol_register(&auriol_275901);
	protocol_set_id(auriol_275901, "auriol_275901");
	protocol_device_add(auriol_275901, "auriol_275901", "Auriol 275901 Weather Station");
	auriol_275901->devtype = WEATHER;
	auriol_275901->hwtype = RF433;
	auriol_275901->minrawlen = RAW_LENGTH;
	auriol_275901->maxrawlen = RAW_LENGTH;
	auriol_275901->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	auriol_275901->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&auriol_275901->options, 't', "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");
	options_add(&auriol_275901->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "[0-9]");
	options_add(&auriol_275901->options, 'b', "battery", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "[0-9]");
	options_add(&auriol_275901->options, 'h', "humidity", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,3}$");	

//	GUI_SETTING	PH 2017-11-05
	options_add(&auriol_275901->options, 0, "temperature-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&auriol_275901->options, 0, "show-temperature", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&auriol_275901->options, 0, "humidity-decimals", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "[0-9]");
	options_add(&auriol_275901->options, 0, "show-humidity", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	

	auriol_275901->parseCode=&parseCode;
	auriol_275901->checkValues=&checkValues;
	auriol_275901->validate=&validate;
	auriol_275901->gc=&gc;
}

//#ifdef MODULAR	//changed PH 2017-09-01	0.1.5.1
#if defined(MODULE) && !defined(_WIN32)
//void compatibility(const char **version, const char **commit) {	//changed PH 2017-09-01
void compatibility(struct module_t *module) {
	module->name = "auriol_275901";
	module->version = "0.1.5.1";
	module->reqversion = "7.0";
	module->reqcommit = "84";
}

void init(void) {
	auriol275901Init();	
}
#endif

