/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _HARDWARE_ZWAVE_H_
#define _HARDWARE_ZWAVE_H_

#include "../hardware/hardware.h"

#define COMMAND_CLASS_SWITCH_BINARY 		0x25
#define COMMAND_CLASS_SWITCH_MULTILEVEL 0x26
#define COMMAND_CLASS_SWITCH_ALL 				0x27
#define COMMAND_CLASS_CONFIGURATION			0x70
#define COMMAND_CLASS_POWERLEVEL 				0x73

typedef struct zwave_values_t {
	int nodeId;
	int cmdId;
	int valueId;
	union {
		int number_;
		char *string_;
	};
	int valueType;
	int instance;
	int genre;
	char *label;
	struct zwave_values_t *next;
} zwave_values_t;

struct zwave_values_t *zwave_values;

struct hardware_t *zwave;
void zwaveInit(void);
unsigned long long zwaveGetValueId(int nodeId, int cmdId);
void zwaveSetValue(int nodeId, int cmdId, char *label, char *value);
void zwaveStartInclusion(void);
void zwaveStartExclusion(void);
void zwaveStopCommand(void);
void zwaveSoftReset(void);
void zwaveGetConfigParam(int nodeId, int paramId);
void zwaveSetConfigParam(int nodeId, int paramId, int valueId, int len);

#endif
