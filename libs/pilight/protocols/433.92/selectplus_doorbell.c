/*
	Copyright (C) 2020 woutput

	This file is part of pilight.
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
#include "../../core/gc.h"
#include "selectplus_doorbell.h"

// Timing from TODO

// Increase this value to be more robust, but also create more false positives
#define PEAK_TO_PEAK_JITTER	80

// The same jitter is assumed on every pulse type (as may be expected)

// Short pulse timing
#define MIN_SHORT_PULSE_LENGTH	(AVG_SHORT_PULSE_LENGTH - 0.5 * PEAK_TO_PEAK_JITTER)
#define AVG_SHORT_PULSE_LENGTH	372
#define MAX_SHORT_PULSE_LENGTH	(AVG_SHORT_PULSE_LENGTH + 0.5 * PEAK_TO_PEAK_JITTER)

// Medium pulse timing
#define MIN_MEDIUM_PULSE_LENGTH	(AVG_MEDIUM_PULSE_LENGTH - 0.5 * PEAK_TO_PEAK_JITTER)
#define AVG_MEDIUM_PULSE_LENGTH	1094
#define MAX_MEDIUM_PULSE_LENGTH	(AVG_MEDIUM_PULSE_LENGTH + 0.5 * PEAK_TO_PEAK_JITTER)

// Long pulse timing
#define MIN_LONG_PULSE_LENGTH	(AVG_LONG_PULSE_LENGTH - 0.5 * PEAK_TO_PEAK_JITTER)
#define AVG_LONG_PULSE_LENGTH	6536
#define MAX_LONG_PULSE_LENGTH	(AVG_LONG_PULSE_LENGTH + 0.5 * PEAK_TO_PEAK_JITTER)

#define RAW_LENGTH		36
// One pulse per bit, first 27 pulses are ID, last 9 pulses are footer
#define BINARY_LENGTH		27

#define NORMAL_REPEATS		68

static int validate(void) {
	// Check for match in raw length
	if (selectplus_doorbell->rawlen == RAW_LENGTH) {
		// Check for match in only part of footer
		if ((selectplus_doorbell->raw[34] >= MIN_MEDIUM_PULSE_LENGTH) &&
		    (selectplus_doorbell->raw[34] <= MAX_MEDIUM_PULSE_LENGTH) &&
		    (selectplus_doorbell->raw[35] >= MIN_LONG_PULSE_LENGTH) &&
		    (selectplus_doorbell->raw[35] <= MAX_LONG_PULSE_LENGTH)) {
			return 0;
		}
	}

	return -1;
}

static void createMessage(int id) {
	selectplus_doorbell->message = json_mkobject();
	json_append_member(selectplus_doorbell->message, "id", json_mknumber(id, 0));
	selectplus_doorbell->txrpt = NORMAL_REPEATS;
}

static void parseCode(void) {
	int binary[BINARY_LENGTH], x;

	for (x = 0; x < BINARY_LENGTH; x += 1) {
		if ((selectplus_doorbell->raw[x] >= MIN_SHORT_PULSE_LENGTH) &&
		    (selectplus_doorbell->raw[x] <= MAX_SHORT_PULSE_LENGTH)) {
			binary[x] = 0;
		} else if ((selectplus_doorbell->raw[x] >= MIN_MEDIUM_PULSE_LENGTH) &&
			   (selectplus_doorbell->raw[x] <= MAX_MEDIUM_PULSE_LENGTH)) {
			binary[x] = 1;
		} else {
			return; // decoding failed, return without creating message
		}
	}

	int id = binToDec(binary, 0, BINARY_LENGTH - 1);
	createMessage(id);
}

static void createId(int id) {
	int binary[255];
	int length = 0;
	int i = 0, x = 0;

	length = decToBinRev(id, binary);
	for (i = 0; i <= length; i++) {
		if (binary[i] == 0) {
			selectplus_doorbell->raw[i] = AVG_SHORT_PULSE_LENGTH;
		} else { //so binary[i] == 1
			selectplus_doorbell->raw[i] = AVG_MEDIUM_PULSE_LENGTH;
		}
	}
}

static void createFooter(void) {
	selectplus_doorbell->raw[27] = AVG_SHORT_PULSE_LENGTH;
	selectplus_doorbell->raw[28] = AVG_MEDIUM_PULSE_LENGTH;
	selectplus_doorbell->raw[29] = AVG_SHORT_PULSE_LENGTH;
	selectplus_doorbell->raw[30] = AVG_MEDIUM_PULSE_LENGTH;
	selectplus_doorbell->raw[31] = AVG_SHORT_PULSE_LENGTH;
	selectplus_doorbell->raw[32] = AVG_MEDIUM_PULSE_LENGTH;
	selectplus_doorbell->raw[33] = AVG_SHORT_PULSE_LENGTH;
	selectplus_doorbell->raw[34] = AVG_MEDIUM_PULSE_LENGTH;
	selectplus_doorbell->raw[35] = AVG_LONG_PULSE_LENGTH;
}

static int createCode(struct JsonNode *code) {
	int id = -1;
	double itmp = -1;

	if (json_find_number(code, "id", &itmp) == 0)
		id = (int)round(itmp);

	if (id == -1) {
		logprintf(LOG_ERR, "selectplus_doorbell: insufficient number of arguments; provide id");
		return EXIT_FAILURE;
	} else if (id > 134217727 || id < 0) { // 2 ^ 27 - 1
		logprintf(LOG_ERR, "selectplus_doorbell: invalid id range. id should be between 0 and 134217727");
		return EXIT_FAILURE;
	} else {
		createMessage(id);
		createId(id);
		createFooter();
		selectplus_doorbell->rawlen = RAW_LENGTH;
	}
	return EXIT_SUCCESS;
}


static void printHelp(void) {
	printf("\t -i --id=id\t\t\tring the doorbell with this id\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void selectplusDoorbellInit(void) {

	protocol_register(&selectplus_doorbell);
	protocol_set_id(selectplus_doorbell, "selectplus_doorbell");
	protocol_device_add(selectplus_doorbell, "selectplus_doorbell", "SelectPlus doorbell");
	selectplus_doorbell->devtype = SWITCH;
	selectplus_doorbell->hwtype = RF433;
	selectplus_doorbell->minrawlen = RAW_LENGTH;
	selectplus_doorbell->maxrawlen = RAW_LENGTH;
	selectplus_doorbell->maxgaplen = MAX_LONG_PULSE_LENGTH;
	selectplus_doorbell->mingaplen = MIN_LONG_PULSE_LENGTH;

	options_add(&selectplus_doorbell->options, "i", "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-9]{1}|[0-9]{2}|[0-9]{3}|[0-9]{4}|[0-9]{5}|[0-9]{6}|[0-9]{7}|[0-9]{8}|[0-9]{9})$");

	options_add(&selectplus_doorbell->options, "0", "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]$");
	options_add(&selectplus_doorbell->options, "0", "confirm", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]$");

	selectplus_doorbell->parseCode = &parseCode;
	selectplus_doorbell->createCode = &createCode;
	selectplus_doorbell->printHelp = &printHelp;
	selectplus_doorbell->validate = &validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "selectplus_doorbell";
	module->version = "1.0";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	selectplusDoorbellInit();
}
#endif
