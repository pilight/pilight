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
#include "ninjablocks.h"

static void ninjablocksSwCreateMessage(int id, int unit, int temperature, int humidity) {
	ninjablocks->message = json_mkobject();
	json_append_member(ninjablocks->message, "id", json_mknumber(id));
	json_append_member(ninjablocks->message, "unit", json_mknumber(unit));
	json_append_member(ninjablocks->message, "temperature", json_mknumber(temperature));
	json_append_member(ninjablocks->message, "humidity", json_mknumber(humidity));
}

static void ninjablocksSwParseCode(void) {
        int x=0, pRaw=0;
	int iPlslenUpper=(int)(ninjablocks->plslen->length/2.1199 * 1.25);
	int iPlslenLower=(int)(ninjablocks->plslen->length/2.1199 * 0.75);
	int iParity=1, iParityData=-1;	// init for even parity
	int iHeaderSync=12;		// 1100
	int iDataSync=6;		// 110

// Decode Biphase Mark Coded Differential Manchester (BMCDM) pulse stream into binary
	for(x=0; x<=ninjablocks->binlen; x++) {
		if( ninjablocks->raw[pRaw] > iPlslenLower && ninjablocks->raw[pRaw] < iPlslenUpper) {
			ninjablocks->binary[x] = 1;
			iParityData=iParity;
			iParity=-iParity;
			pRaw++;
		} else {
			ninjablocks->binary[x] = 0;
		}
		pRaw++;
	}
	if (iParityData<0) iParityData=0;

// Binary record: 0-3 sync0, 4-7 unit, 8-9 id, 10-12 sync1, 13-19 humidity, 20-34 temperature, 35 even par, 36 footer
	int headerSync = binToDecRev(ninjablocks->binary, 0,3);
	int unit = binToDecRev(ninjablocks->binary, 4,7);
	int id = binToDecRev(ninjablocks->binary, 8,9);
	int dataSync = binToDecRev(ninjablocks->binary, 10,12);
	int humidity = binToDecRev(ninjablocks->binary, 13,19);	// %
	int temperature = binToDecRev(ninjablocks->binary, 20,34);
	temperature=temperature*100/128-5000;			// temp=(temp+50)*128 °C, 2 digits
	int parity = binToDecRev(ninjablocks->binary, 35,35);

	if (iParityData==0 && (iHeaderSync==headerSync || dataSync==iDataSync)) {
		ninjablocksSwCreateMessage(id, unit, temperature, humidity);
	} else {
		logprintf(LOG_ERR, "**** ninjablocks parsecode Error: invalid Parity Bit or Header:");
		if(log_level_get() >= LOG_DEBUG) {
			for(x=0;x<=ninjablocks->rawlen;x++) {
				printf("%d ", ninjablocks->raw[x]);
			}
			printf("\n");
			for(x=0;x<=ninjablocks->binlen;x++) {
				printf("%d", ninjablocks->binary[x]);
				switch (x) {
					case 3:
					printf(" ");
					break;
					case 7:
					printf(" ");
					break;
					case 9:
					printf(" ");
					break;
					case 12:
					printf(" ");
					break;
					case 19:
					printf(" ");
					break;
					case 34:
					printf(" ");
					break;
					default:
					break;
				}
			}
			printf("**** ninjablocks: Parity Bit found: %d, expected: %d. Sync values found - Header: %d Data: %d.\n", parity, iParityData, headerSync, dataSync);
		}
		id=-1;
		unit=-1;
		humidity=0;
		temperature=0;
	}
}

static void ninjablocksSwCreateZero(int e) {
	int i,k;
	k = ninjablocks->rawlen+e-1;
	if (k>ninjablocks->maxrawlen) {
		logprintf(LOG_ERR, "**** ninjablocks create zero error: invalid raw data pointer %d", k);
	} else {
printf("-ZpRaw %d k: %d-",ninjablocks->rawlen,k);
		for(i=ninjablocks->rawlen;i<=k;i++) {
			ninjablocks->raw[i] = (int)(ninjablocks->pulse*ninjablocks->plslen->length/2.1199);
			ninjablocks->rawlen++;
printf("ZERO ");
		}
	}
}

static void ninjablocksSwCreateOne(int e) {
	int i,k;
	k = ninjablocks->rawlen+e+e-1;
	if ((k+1)>ninjablocks->maxrawlen) {
		logprintf(LOG_ERR, "**** ninjablocks create one error: invalid raw data pointer %d", k);
	} else {
printf("-OpRaw %d k: %d-",ninjablocks->rawlen,k);
		for(i=ninjablocks->rawlen;i<=k;i+=2) {
			ninjablocks->raw[i] = (int)(ninjablocks->plslen->length/2.1199);	// code a logical 1 pulse
			ninjablocks->raw[i+1] = ninjablocks->raw[i];
			ninjablocks->raw[ninjablocks->maxrawlen+1]=-ninjablocks->raw[ninjablocks->maxrawlen+1];		// toogle parity bit
			ninjablocks->rawlen+=2;
printf("ONE ");
		}
	}
}

static void ninjablocksSwCreateData(int iParam, int iLength) {
	int binary[255];
	int i, length, emptylength;

	length = decToBin(iParam, binary);
printf("\nData length: %d iParam: %d",length,iParam);
// Create leading empty zero pulses and data pulses
	emptylength=iLength-length-1;
	for(i=0;i<emptylength;i++) {
		ninjablocksSwCreateZero(1);
	}
	for(i=0;i<=length;i++) {
		if(binary[i] == 1) {
			ninjablocksSwCreateOne(1);
		} else {
			ninjablocksSwCreateZero(1);
		}
	}
}

static void ninjablocksSwCreateHeader(void) {
	ninjablocks->rawlen=0;				// There is plenty of space in the structure:
	ninjablocks->raw[ninjablocks->maxrawlen+1]=1;	// init local parity storage to even in memory location after maxlen of raw data buffer
	ninjablocksSwCreateOne(2);
	ninjablocksSwCreateZero(2);

}

static void ninjablocksSwClearCode(void) {
	ninjablocksSwCreateHeader();
	ninjablocksSwCreateZero(64);
	ninjablocksSwCreateHeader();
}

static void ninjablocksSwCreateUnit(int unit) {
	ninjablocksSwCreateData(unit, 4);
}

static void ninjablocksSwCreateId(int id) {
	ninjablocksSwCreateData(id, 2);
}

static void ninjablocksSwCreateSync(void) {
	ninjablocksSwCreateOne(2);
	ninjablocksSwCreateZero(1);
}

static void ninjablocksSwCreateHumidity(int humidity) {
	ninjablocksSwCreateData(humidity, 7);
}

static void ninjablocksSwCreateTemperature(int temperature) {
	ninjablocksSwCreateData(temperature, 15);
}

static void ninjablocksSwCreateParity(void) {
	if(ninjablocks->raw[ninjablocks->maxrawlen+1] == 1) {
		ninjablocksSwCreateZero(1);	// 1 is Even parity
	} else {
		ninjablocksSwCreateOne(1);
	}
}

static void ninjablocksSwCreateFooter(void) {
	if (ninjablocks->rawlen>ninjablocks->maxrawlen) {
		logprintf(LOG_ERR, "**** ninjablocks create footer: invalid raw data pointer %d", ninjablocks->rawlen);
	} else {
//		ninjablocks->raw[ninjablocks->rawlen] = 72075;
		ninjablocks->raw[ninjablocks->rawlen] = PULSE_DIV*ninjablocks->plslen->length;
		ninjablocks->rawlen++;
	}
}

static int ninjablocksSwCreateCode(JsonNode *code) {
	int unit = -1;
	int id = -1;
	int humidity = -1;
	int temperature = -1;
	double itmp = -1;

	if(json_find_number(code, "id", &itmp) == 0)
		id = (int)round(itmp);
	if(json_find_number(code, "unit", &itmp) == 0)
		unit = (int)round(itmp);
	if(json_find_number(code, "temperature", &itmp) == 0)
		temperature = (int)round(itmp);
	if(json_find_number(code, "humidity", &itmp) == 0)
		humidity = (int)round(itmp);

	if(id==-1 || unit==-1 || (temperature==-1 && humidity ==-1)) {
		logprintf(LOG_ERR, "**** ninjablocks: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 3 || id < 0) {
		logprintf(LOG_ERR, "**** ninjablocks: invalid channel id range");
		return EXIT_FAILURE;
	} else if(unit > 15 || unit < 0) {
		logprintf(LOG_ERR, "**** ninjablocks: invalid main code unit range");
		return EXIT_FAILURE;
	} else {

		ninjablocksSwCreateMessage(id, unit, temperature, humidity);
		ninjablocksSwClearCode();
		ninjablocksSwCreateUnit(unit);
		ninjablocksSwCreateId(id);
		ninjablocksSwCreateSync();
		ninjablocksSwCreateHumidity(humidity);
		ninjablocksSwCreateTemperature(temperature);
		ninjablocksSwCreateParity();
		ninjablocksSwCreateFooter();

	}
	return EXIT_SUCCESS;
}

static void ninjablocksSwPrintHelp(void) {
	printf("\t -i --id=id\t\t\tchannel code id of reporting unit\n");
	printf("\t -u --unit=unit\t\t\tmain code unit of reporting unit\n");
	printf("\t -t --temperature\t\t\ttemperature reported by device\n");
	printf("\t -h --humidity\t\t\thumidity reported by device\n");
}

#ifndef MODULE
__attribute__((weak))
#endif
void ninjablocksSwInit(void) {

	protocol_register(&ninjablocks);
	protocol_set_id(ninjablocks, "ninjablocks");
	protocol_device_add(ninjablocks, "ninjablocks", "ninjablocks Sensors");
//	protocol_plslen_add(ninjablocks, 1000);		// SHORT: 1000
	protocol_plslen_add(ninjablocks, 2120);		// Footer length: 72075/PULSE_DIV/2120=2,1199
//	ninjablocks->plslen->footerlength=2120;
	ninjablocks->devtype = SENSOR;
	ninjablocks->hwtype = RF433;
	ninjablocks->pulse = 2;		// LONG=ninjablocks_PULSE_HIGH*SHORT
	ninjablocks->rawlen = 70;	// dynamically between 41..70 footer is depending on raw pulse code
	ninjablocks->minrawlen = 41;
	ninjablocks->maxrawlen = 70;
	ninjablocks->binlen = 35;      // sync-id[4]; Homecode[4], Channel Code[2], Sync[3], Humidity[7], Temperature[15], Footer [1]

	options_add(&ninjablocks->options, 'u', "unit", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-9]|1[0-5])$");
	options_add(&ninjablocks->options, 'i', "id", OPTION_HAS_VALUE, CONFIG_ID, JSON_NUMBER, NULL, "^([0-3])$");
	options_add(&ninjablocks->options, 't', "temperature", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");
	options_add(&ninjablocks->options, 'h', "humidity", OPTION_HAS_VALUE, CONFIG_VALUE, JSON_NUMBER, NULL, "^[0-9]{1,5}$");

	options_add(&ninjablocks->options, 0, "device-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&ninjablocks->options, 0, "gui-decimals", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)2, "[0-9]");
	options_add(&ninjablocks->options, 0, "gui-readonly", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&ninjablocks->options, 0, "gui-show-humidity", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");
	options_add(&ninjablocks->options, 0, "gui-show-temperature", OPTION_HAS_VALUE, CONFIG_SETTING, JSON_NUMBER, (void *)1, "^[10]{1}$");

	ninjablocks->parseCode=&ninjablocksSwParseCode;
	ninjablocks->createCode=&ninjablocksSwCreateCode;
	ninjablocks->printHelp=&ninjablocksSwPrintHelp;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name =  "ninjablocks";
	module->version =  "1.0";
	module->reqversion =  "5.0";
	module->reqcommit =  NULL;
}

void init(void) {
	ninjablocksSwInit();
}
#endif
