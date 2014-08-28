/*
	Copyright (C) 2014 wo_rasp & CurlyMo

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

#include "../../pilight.h"
#include "common.h"
#include "dso.h"
#include "log.h"
#include "protocol.h"
#include "hardware.h"
#include "binary.h"
#include "gc.h"
#include "ninjablocks_weather.h"

#define	PULSE_NINJA_WEATHER_SHORT	1000
#define PULSE_NINJA_WEATHER_LONG	2000
#define PULSE_NINJA_WEATHER_FOOTER	2120	// 72080/PULSE_DIV
#define PULSE_NINJA_WEATHER_LOWER	750	// SHORT*0,75
#define PULSE_NINJA_WEATHER_UPPER	1250	// SHORT * 1,25

typedef struct ninjablocks_weather_settings_t {
	double id;
	double unit;
	double temp;
	double humi;
	struct ninjablocks_weather_settings_t *next;
} ninjablocks_weather_settings_t;

static struct ninjablocks_weather_settings_t *ninjablocks_weather_settings = NULL;

static void ninjablocksWeatherCreateMessage(int id, int unit, int temperature, int humidity) {
	ninjablocks_weather->message = json_mkobject();
	json_append_member(ninjablocks_weather->message, "id", json_mknumber(id));
	json_append_member(ninjablocks_weather->message, "unit", json_mknumber(unit));
	json_append_member(ninjablocks_weather->message, "temperature", json_mknumber(temperature));
	json_append_member(ninjablocks_weather->message, "humidity", json_mknumber(humidity*100));
}

static void ninjablocksWeatherParseCode(void) {
	int x = 0, pRaw = 0;
	int iParity = 1, iParityData = -1;	// init for even parity
	int iHeaderSync = 12;				// 1100
	int iDataSync = 6;					// 110
	int temp_offset = 0;
	int humi_offset = 0;
	
	// Decode Biphase Mark Coded Differential Manchester (BMCDM) pulse stream into binary
	for(x=0; x<=ninjablocks_weather->binlen; x++) {
		if(ninjablocks_weather->raw[pRaw] > PULSE_NINJA_WEATHER_LOWER && ninjablocks_weather->raw[pRaw] < PULSE_NINJA_WEATHER_UPPER) {
			ninjablocks_weather->binary[x] = 1;
			iParityData=iParity;
			iParity=-iParity;
			pRaw++;
		} else {
			ninjablocks_weather->binary[x] = 0;
		}
		pRaw++;
	}
	if(iParityData<0)
		iParityData=0;

	// Binary record: 0-3 sync0, 4-7 unit, 8-9 id, 10-12 sync1, 13-19 humidity, 20-34 temperature, 35 even par, 36 footer
	int headerSync = binToDecRev(ninjablocks_weather->binary, 0,3);
	int unit = binToDecRev(ninjablocks_weather->binary, 4,7);
	int id = binToDecRev(ninjablocks_weather->binary, 8,9);
	int dataSync = binToDecRev(ninjablocks_weather->binary, 10,12);
	int humidity = binToDecRev(ninjablocks_weather->binary, 13,19);	// %
	int temperature = binToDecRev(ninjablocks_weather->binary, 20,34);
	temperature *= (100 / 128) - 5000;			// temp=(temp+50)*128 °C, 2 digits
//	int parity = binToDecRev(ninjablocks_weather->binary, 35,35);

	struct ninjablocks_weather_settings_t *tmp = ninjablocks_weather_settings;
	while(tmp) {
		if(fabs(tmp->id-id) < EPSILON && fabs(tmp->unit-unit) < EPSILON) {
			humi_offset = (int)tmp->humi;
			temp_offset = (int)tmp->temp;
			break;
		}
		tmp = tmp->next;
	}

	temperature += temp_offset;
	humidity += humi_offset;	

	if(iParityData == 0 && (iHeaderSync == headerSync || dataSync == iDataSync)) {
		ninjablocksWeatherCreateMessage(id, unit, temperature, humidity);
	}/* else {
		if(log_level_get() >= LOG_DEBUG) {
			logprintf(LOG_ERR, "Parsecode Error: Invalid Parity Bit or Header:");
			for(x=0;x<=ninjablocks_weather->rawlen;x++) {
				printf("%d ", ninjablocks_weather->raw[x]);
			}
			printf("\n");
			for(x=0;x<=ninjablocks_weather->binlen;x++) {
				printf("%d", ninjablocks_weather->binary[x]);
				switch (x) {
					case 3:
					case 7:
					case 9:
					case 12:
					case 19:
					case 34:
					printf(" ");
					break;
					default:
					break;
				}
			}
		}
		id=-1;
		unit=-1;
		humidity=0;
		temperature=0;
	}*/
}

/*static void ninjablocksWeatherCreateZero(int e) {
	int i = 0, k = ninjablocks_weather->rawlen+e-1;

	if(k<=ninjablocks_weather->maxrawlen) {
		for(i=ninjablocks_weather->rawlen;i<=k;i++) {
			ninjablocks_weather->raw[i] = (int)PULSE_NINJA_WEATHER_LONG;
			ninjablocks_weather->rawlen++;
		}
	}
}

static void ninjablocksWeatherCreateOne(int e) {
	int i = 0, k = ninjablocks_weather->rawlen+e+e-1;

	if((k+1)<=ninjablocks_weather->maxrawlen) {
		for(i=ninjablocks_weather->rawlen;i<=k;i+=2) {
			ninjablocks_weather->raw[i] = (int)PULSE_NINJA_WEATHER_SHORT;	// code a logical 1 pulse
			ninjablocks_weather->raw[i+1] = ninjablocks_weather->raw[i];
			// toggle parity bit
			ninjablocks_weather->raw[ninjablocks_weather->maxrawlen+1]=-ninjablocks_weather->raw[ninjablocks_weather->maxrawlen+1];
			ninjablocks_weather->rawlen+=2;
		}
	}
}

static void ninjablocksWeatherCreateData(int iParam, int iLength) {
	int binary[255];
	int i, length, emptylength;

	length = decToBin(iParam, binary);
	// Create leading empty zero pulses and data pulses
	emptylength=iLength-length-1;
	for(i=0;i<emptylength;i++) {
		ninjablocksWeatherCreateZero(1);
	}
	for(i=0;i<=length;i++) {
		if(binary[i] == 1) {
			ninjablocksWeatherCreateOne(1);
		} else {
			ninjablocksWeatherCreateZero(1);
		}
	}
}

static void ninjablocksWeatherCreateHeader(void) {
	ninjablocks_weather->rawlen=0;							// There is plenty of space in the structure:
	ninjablocks_weather->raw[ninjablocks_weather->maxrawlen+1]=1;	// init local parity storage to even in memory location after maxlen of raw data buffer
	ninjablocksWeatherCreateOne(2);
	ninjablocksWeatherCreateZero(2);

}

static void ninjablocksWeatherClearCode(void) {
	ninjablocksWeatherCreateHeader();
	ninjablocksWeatherCreateZero(64);
	ninjablocksWeatherCreateHeader();
}

static void ninjablocksWeatherCreateUnit(int unit) {
	ninjablocksWeatherCreateData(unit, 4);
}

static void ninjablocksWeatherCreateId(int id) {
	ninjablocksWeatherCreateData(id, 2);
}

static void ninjablocksWeatherCreateSync(void) {
	ninjablocksWeatherCreateOne(2);
	ninjablocksWeatherCreateZero(1);
}

static void ninjablocksWeatherCreateHumidity(int humidity) {
	ninjablocksWeatherCreateData(humidity, 7);
}

static void ninjablocksWeatherCreateTemperature(int temperature) {
	ninjablocksWeatherCreateData(temperature, 15);
}

static void ninjablocksWeatherCreateParity(void) {
	if(ninjablocks_weather->raw[ninjablocks_weather->maxrawlen+1] == 1) {
		ninjablocksWeatherCreateZero(1);	// 1 is Even parity
	} else {
		ninjablocksWeatherCreateOne(1);
	}
}

static void ninjablocksWeatherCreateFooter(void) {
	if (ninjablocks_weather->rawlen<=ninjablocks_weather->maxrawlen) {
		ninjablocks_weather->raw[ninjablocks_weather->rawlen] = PULSE_DIV*ninjablocks_weather->plslen->length;
		ninjablocks_weather->rawlen++;
	}
}

static int ninjablocksWeatherCreateCode(JsonNode *code) {
	int unit = -1;
	int id = -1;
	int humidity = -1;
	int temperature = -1;
	double itmp = -1;

	if(json_find_number(code, "id", &itmp) == 0)	id = (int)round(itmp);
	if(json_find_number(code, "unit", &itmp) == 0)	unit = (int)round(itmp);
	if(json_find_number(code, "temperature", &itmp) == 0)	temperature = (int)round(itmp);
	if(json_find_number(code, "humidity", &itmp) == 0)	humidity = (int)round(itmp)/100;

	if(id==-1 || unit==-1 || (temperature==-1 && humidity ==-1)) {
		logprintf(LOG_ERR, "insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 3 || id < 0) {
		logprintf(LOG_ERR, "invalid channel id range");
		return EXIT_FAILURE;
	} else if(unit > 15 || unit < 0) {
		logprintf(LOG_ERR, "invalid main code unit range");
		return EXIT_FAILURE;
	} else {

		ninjablocksWeatherCreateMessage(id, unit, temperature, humidity);
		ninjablocksWeatherClearCode();
		ninjablocksWeatherCreateUnit(unit);
		ninjablocksWeatherCreateId(id);
		ninjablocksWeatherCreateSync();
		ninjablocksWeatherCreateHumidity(humidity);
		ninjablocksWeatherCreateTemperature(temperature);
		ninjablocksWeatherCreateParity();
		ninjablocksWeatherCreateFooter();

	}
	return EXIT_SUCCESS;
}*/

/*static void ninjablocksWeatherPrintHelp(void) {
	printf("\t -i --id=id\t\t\tchannel code id of reporting unit\n");
	printf("\t -u --unit=unit\t\t\tmain code unit of reporting unit\n");
	printf("\t -t --temperature\t\t\ttemperature reported by device\n");
	printf("\t -h --humidity\t\t\thumidity reported by device\n");
}*/

static int ninjablocksWeatherCheckValues(struct JsonNode *jvalues) {
	struct JsonNode *jid = NULL;

	if((jid = json_find_member(jvalues, "id"))) {
		struct ninjablocks_weather_settings_t *snode = NULL;
		struct JsonNode *jchild = NULL;
		struct JsonNode *jchild1 = NULL;
		double unit = -1, id = -1;
		int match = 0;

		jchild = json_first_child(jid);
		while(jchild) {
			jchild1 = json_first_child(jchild);
			while(jchild1) {
				if(strcmp(jchild1->key, "unit") == 0) {
					unit = jchild1->number_;
				}
				if(strcmp(jchild1->key, "id") == 0) {
					id = jchild1->number_;
				}
				jchild1 = jchild1->next;
			}
			jchild = jchild->next;
		}

		struct ninjablocks_weather_settings_t *tmp = ninjablocks_weather_settings;
		while(tmp) {
			if(fabs(tmp->id-id) < EPSILON && fabs(tmp->unit-unit) < EPSILON) {
				match = 1;
				break;
			}
			tmp = tmp->next;
		}

		if(!match) {
			if(!(snode = malloc(sizeof(struct ninjablocks_weather_settings_t)))) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			snode->id = id;
			snode->unit = unit;

			json_find_number(jvalues, "device-temperature-offset", &snode->temp);
			json_find_number(jvalues, "device-humidity-offset", &snode->humi);

			snode->next = ninjablocks_weather_settings;
			ninjablocks_weather_settings = snode;
		}
	}
	return 0;
}

static void ninjablocksWeatherGC(void) {
	struct ninjablocks_weather_settings_t *tmp = NULL;
	while(ninjablocks_weather_settings) {
		tmp = ninjablocks_weather_settings;
		ninjablocks_weather_settings = ninjablocks_weather_settings->next;
		sfree((void *)&tmp);
	}
	sfree((void *)&ninjablocks_weather_settings);
}

#ifndef MODULE
__attribute__((weak))
#endif
void ninjablocksWeatherInit(void) {

	protocol_register(&ninjablocks_weather);
	protocol_set_id(ninjablocks_weather, "ninjablocks_weather");
	protocol_device_add(ninjablocks_weather, "ninjablocks_weather", "Ninjablocks Weather Sensors");
	protocol_plslen_add(ninjablocks_weather, PULSE_NINJA_WEATHER_FOOTER);		// Footer length ratio: (72080/PULSE_DIV)/2120=2,120
	ninjablocks_weather->devtype = SENSOR;
	ninjablocks_weather->hwtype = RF433;
	ninjablocks_weather->pulse = 2;		// LONG=ninjablocks_PULSE_HIGH*SHORT
	ninjablocks_weather->rawlen = 70;	// dynamically between 41..70 footer is depending on raw pulse code
	ninjablocks_weather->minrawlen = 41;
	ninjablocks_weather->maxrawlen = 70;
	ninjablocks_weather->binlen = 35;	// sync-id[4]; Homecode[4], Channel Code[2], Sync[3], Humidity[7], Temperature[15], Footer [1]

	options_add(&ninjablocks_weather->options, 'u', "unit", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]|1[0-5])$");
	options_add(&ninjablocks_weather->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-3])$");
	options_add(&ninjablocks_weather->options, 't', "temperature", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");
	options_add(&ninjablocks_weather->options, 'h', "humidity", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");

	options_add(&ninjablocks_weather->options, 0, "device-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&ninjablocks_weather->options, 0, "gui-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&ninjablocks_weather->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&ninjablocks_weather->options, 0, "gui-show-humidity", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&ninjablocks_weather->options, 0, "gui-show-temperature", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	ninjablocks_weather->parseCode=&ninjablocksWeatherParseCode;
	/*ninjablocks->createCode=&ninjablocksWeatherCreateCode;
	ninjablocks->printHelp=&ninjablocksWeatherPrintHelp;*/
	ninjablocks_weather->checkValues=&ninjablocksWeatherCheckValues;
	ninjablocks_weather->gc=&ninjablocksWeatherGC;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name =  "ninjablocks_weather";
	module->version =  "0.9";
	module->reqversion =  "5.0";
	module->reqcommit =  NULL;
}

void init(void) {
	ninjablocksWeatherInit();
}
#endif

