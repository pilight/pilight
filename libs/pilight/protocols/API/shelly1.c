/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>
#include <assert.h>
#ifndef _WIN32
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
#endif
#include <pthread.h>

#include "../../core/threads.h"
#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/json.h"
#include "../../core/gc.h"
#include "../../lua_c/table.h"
#include "shelly1.h"

static void *reason_send_code_free(void *param) {
	plua_metatable_free(param);
	return NULL;
}

static void printHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
	printf("\t -c --connected=1|0\t\toverride (dis)connected state\n");
}

static int createCode(struct JsonNode *code) {
	char *id = NULL;
	int state = -1;
	int connected = -1;
	double itmp = -1;

	json_find_string(code, "id", &id);
	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;
	if(json_find_number(code, "connected", &itmp) == 0) {
		if(itmp < 0 || itmp > 1) {
			logprintf(LOG_ERR, "shelly1: connected must be either 1 or 0");
			return EXIT_FAILURE;
		}
		connected = itmp;
	}

	if(connected != -1 && state != -1) {
		logprintf(LOG_ERR, "shelly1: connected and state cannot be combined");
		return EXIT_FAILURE;
	} else if(id == NULL || (state == -1 && connected == -1)) {
		logprintf(LOG_ERR, "shelly1: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id == NULL) {
		logprintf(LOG_ERR, "shelly1: invalid id");
		return EXIT_FAILURE;
	} else {
		struct plua_metatable_t *table = NULL;
		plua_metatable_init(&table);
		plua_metatable_set_string(table, "id", id);
		if(state == 1) {
			plua_metatable_set_string(table, "state", "on");
		} else if(state == 0) {
			plua_metatable_set_string(table, "state", "off");
		}
		if(connected != -1) {
			plua_metatable_set_number(table, "connected", connected);
		}
		plua_metatable_set_string(table, "protocol", shellySwitch->id);
		plua_metatable_set_number(table, "hwtype", SHELLY);
		plua_metatable_set_string(table, "uuid", "0");

		eventpool_trigger(REASON_SEND_CODE+10000, reason_send_code_free, table);
	}
	return EXIT_SUCCESS;
}

static struct threadqueue_t *initDev(JsonNode *jdevice) {
	if(pilight.send != NULL) {
		struct JsonNode *jid = json_find_member(jdevice, "id");
		struct JsonNode *jchild = json_first_child(jid);
		char *id = NULL;
		while(jchild != NULL) {
			if(json_find_string(jchild, "id", &id) == 0) {
				struct JsonNode *jobject = json_mkobject();
				struct JsonNode *jcode = json_mkobject();
				struct JsonNode *jprotocol = json_mkarray();

				json_append_element(jprotocol, json_mkstring("shelly1"));

				json_append_member(jcode, "id", json_mkstring(id));
				json_append_member(jcode, "connected", json_mknumber(1, 0));
				json_append_member(jcode, "protocol", jprotocol);

				json_append_member(jobject, "code", jcode);
				json_append_member(jobject, "action", json_mkstring("send"));

				if(pilight.send(jobject, PROTOCOL) == 0) {

				}
				json_delete(jobject);
			}
			jchild = jchild->next;
		}
	}
	return NULL;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void shellySwitchInit(void) {
	protocol_register(&shellySwitch);
	protocol_set_id(shellySwitch, "shelly1");
	protocol_device_add(shellySwitch, "shelly1", "Shelly Switch");
	shellySwitch->devtype = SWITCH;
	shellySwitch->hwtype = SHELLY;

	options_add(&shellySwitch->options, "i", "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "[A-Z0-9]");
	options_add(&shellySwitch->options, "t", "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&shellySwitch->options, "f", "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&shellySwitch->options, "c", "connected", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, (void *)1, "^[10]{1}$");

	options_add(&shellySwitch->options, "0", "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	shellySwitch->initDev=&initDev;
	shellySwitch->createCode=&createCode;
	shellySwitch->printHelp=&printHelp;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "shelly1";
	module->version = "1.6";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	shellySwitchInit();
}
#endif
