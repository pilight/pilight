/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
#include "arctech_screen.h"

#define LEARN_REPEATS			40
#define NORMAL_REPEATS		10
#define PULSE_MULTIPLIER	5
#define MIN_PULSE_LENGTH	250
#define MAX_PULSE_LENGTH	320
#define AVG_PULSE_LENGTH	300
#define RAW_LENGTH				132

static int validate(void) {
	if(arctech_screen->rawlen == RAW_LENGTH) {
		if(arctech_screen->raw[arctech_screen->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
		   arctech_screen->raw[arctech_screen->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV) &&
			 arctech_screen->raw[1] >= AVG_PULSE_LENGTH*(PULSE_MULTIPLIER*1.5)) {
			return 0;
		}
	}

	return -1;
}

static void createMessage(char **message, int id, int unit, int state, int all, int learn) {
	int x = snprintf((*message), 255, "{\"id\":%d,", id);
	if(all == 1) {
		x += snprintf(&(*message)[x], 255-x, "\"all\":1,");
	} else {
		x += snprintf(&(*message)[x], 255-x, "\"unit\":%d,", unit);
	}

	if(state == 1) {
		x += snprintf(&(*message)[x], 255-x, "\"state\":\"up\"");
	} else {
		x += snprintf(&(*message)[x], 255-x, "\"state\":\"down\"");
	}
	x += snprintf(&(*message)[x], 255-x, "}");

	if(learn == 1) {
		arctech_screen->txrpt = LEARN_REPEATS;
	} else {
		arctech_screen->txrpt = NORMAL_REPEATS;
	}
}

static void parseCode(char **message) {
	int binary[RAW_LENGTH/4], x = 0, i = 0;

	if(arctech_screen->rawlen>RAW_LENGTH) {
		logprintf(LOG_ERR, "arctech_screen: parsecode - invalid parameter passed %d", arctech_screen->rawlen);
		return;
	}

	for(x=0;x<arctech_screen->rawlen;x+=4) {
		if(arctech_screen->raw[x+3] > (int)((double)AVG_PULSE_LENGTH*((double)PULSE_MULTIPLIER/2))) {
			binary[i++] = 1;
		} else {
			binary[i++] = 0;
		}
	}

	int unit = binToDecRev(binary, 28, 31);
	int state = binary[27];
	int all = binary[26];
	int id = binToDecRev(binary, 0, 25);

	createMessage(message, id, unit, state, all, 0);
}

static void createLow(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		arctech_screen->raw[i]=(AVG_PULSE_LENGTH);
		arctech_screen->raw[i+1]=(AVG_PULSE_LENGTH);
		arctech_screen->raw[i+2]=(AVG_PULSE_LENGTH);
		arctech_screen->raw[i+3]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
	}
}

static void createHigh(int s, int e) {
	int i;

	for(i=s;i<=e;i+=4) {
		arctech_screen->raw[i]=(AVG_PULSE_LENGTH);
		arctech_screen->raw[i+1]=(PULSE_MULTIPLIER*AVG_PULSE_LENGTH);
		arctech_screen->raw[i+2]=(AVG_PULSE_LENGTH);
		arctech_screen->raw[i+3]=(AVG_PULSE_LENGTH);
	}
}

static void clearCode(void) {
	createLow(2, 132);
}

static void createStart(void) {
	arctech_screen->raw[0]=(AVG_PULSE_LENGTH);
	arctech_screen->raw[1]=(9*AVG_PULSE_LENGTH);
}

static void createId(int id) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(id, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=((length-i)+1)*4;
			createHigh(106-x, 106-(x-3));
		}
	}
}

static void createAll(int all) {
	if(all == 1) {
		createHigh(106, 109);
	}
}

static void createState(int state) {
	if(state == 1) {
		createHigh(110, 113);
	}
}

static void createUnit(int unit) {
	int binary[255];
	int length = 0;
	int i=0, x=0;

	length = decToBin(unit, binary);
	for(i=0;i<=length;i++) {
		if(binary[i]==1) {
			x=((length-i)+1)*4;
			createHigh(130-x, 130-(x-3));
		}
	}
}

static void createFooter(void) {
	arctech_screen->raw[131]=(PULSE_DIV*AVG_PULSE_LENGTH);
}

static int createCode(struct JsonNode *code, char **message) {
	int id = -1;
	int unit = -1;
	int state = -1;
	int all = 0;
	int learn = -1;
	double itmp = -1;

	if(json_find_number(code, "id", &itmp) == 0)
		id = (int)round(itmp);
	if(json_find_number(code, "unit", &itmp) == 0)
		unit = (int)round(itmp);
	if(json_find_number(code, "all", &itmp) == 0)
		all = (int)round(itmp);
	if(json_find_number(code, "down", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "up", &itmp) == 0)
		state=1;
	if(json_find_number(code, "learn", &itmp) == 0)
		learn = 1;

	if(all > 0 && learn > -1) {
		logprintf(LOG_ERR, "arctech_screen: all and learn cannot be combined");
		return EXIT_FAILURE;
	} else if(id == -1 || (unit == -1 && all == 0) || state == -1) {
		logprintf(LOG_ERR, "arctech_screen: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 67108863 || id < 1) {
		logprintf(LOG_ERR, "arctech_screen: invalid id range");
		return EXIT_FAILURE;
	} else if((unit > 15 || unit < 0) && all == 0) {
		logprintf(LOG_ERR, "arctech_screen: invalid unit range");
		return EXIT_FAILURE;
	} else {
		if(unit == -1 && all == 1) {
			unit = 0;
		}
		createMessage(message, id, unit, state, all, learn);
		createStart();
		clearCode();
		createId(id);
		createAll(all);
		createState(state);
		createUnit(unit);
		createFooter();
		arctech_screen->rawlen = RAW_LENGTH;
	}
	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -t --up\t\t\tsend an up signal\n");
	printf("\t -f --down\t\t\tsend an down signal\n");
	printf("\t -u --unit=unit\t\t\tcontrol a device with this unit code\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
	printf("\t -a --all\t\t\tsend command to all devices with this id\n");
	printf("\t -l --learn\t\t\tsend multiple streams so screen can learn\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void arctechScreenInit(void) {

	protocol_register(&arctech_screen);
	protocol_set_id(arctech_screen, "arctech_screen");
	protocol_device_add(arctech_screen, "kaku_screen", "KlikAanKlikUit Screens");
	protocol_device_add(arctech_screen, "dio_screen", "DI-O Screens");
	arctech_screen->devtype = SCREEN;
	arctech_screen->hwtype = RF433;
	arctech_screen->txrpt = NORMAL_REPEATS;
	arctech_screen->minrawlen = RAW_LENGTH;
	arctech_screen->maxrawlen = RAW_LENGTH;
	arctech_screen->maxgaplen = MAX_PULSE_LENGTH*PULSE_DIV;
	arctech_screen->mingaplen = MIN_PULSE_LENGTH*PULSE_DIV;

	options_add(&arctech_screen->options, "t", "up", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&arctech_screen->options, "f", "down", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&arctech_screen->options, "u", "unit", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-9]{1}|[1][0-5])$");
	options_add(&arctech_screen->options, "i", "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-9]{1,7}|[1-5][0-9]{7}|6([0-6][0-9]{6}|7(0[0-9]{5}|10([0-7][0-9]{3}|8([0-7][0-9]{2}|8([0-5][0-9]|6[0-3]))))))$");
	options_add(&arctech_screen->options, "a", "all", OPTION_OPT_VALUE, DEVICES_OPTIONAL, JSON_NUMBER, NULL, NULL);
	options_add(&arctech_screen->options, "l", "learn", OPTION_NO_VALUE, DEVICES_OPTIONAL, JSON_NUMBER, NULL, NULL);

	options_add(&arctech_screen->options, "0", "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&arctech_screen->options, "0", "confirm", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	arctech_screen->parseCode=&parseCode;
	arctech_screen->createCode=&createCode;
	arctech_screen->printHelp=&printHelp;
	arctech_screen->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "arctech_screen";
	module->version = "4.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	arctechScreenInit();
}
#endif
