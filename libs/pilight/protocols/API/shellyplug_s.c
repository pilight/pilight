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

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/json.h"
#include "../../lua_c/table.h"
#include "shellyplug_s.h"

static void *reason_send_code_free(void *param) {
	plua_metatable_free(param);
	return NULL;
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
			logprintf(LOG_ERR, "shellyplug-s: connected must be either 1 or 0");
			return EXIT_FAILURE;
		}
		connected = itmp;
	}

	if(connected != -1 && state != -1) {
		logprintf(LOG_ERR, "shellyplug-s: connected and state cannot be combined");
		return EXIT_FAILURE;
	} else if(id == NULL || (state == -1 && connected == -1)) {
		logprintf(LOG_ERR, "shellyplug-s: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id == NULL) {
		logprintf(LOG_ERR, "shellyplug-s: invalid id");
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
		plua_metatable_set_string(table, "protocol", shellyPlugS->id);
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

				json_append_element(jprotocol, json_mkstring("shellyplug-s"));

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

static void printHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
	printf("\t -c --connected=0\t\toverride (dis)connected state\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void shellyPlugSInit(void) {
	protocol_register(&shellyPlugS);
	protocol_set_id(shellyPlugS, "shellyplug-s");
	protocol_device_add(shellyPlugS, "shellyplug-s", "Shelly Plug S");
	shellyPlugS->devtype = SWITCH;
	shellyPlugS->hwtype = SHELLY;

	options_add(&shellyPlugS->options, "i", "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, "[A-Z0-9]");
	options_add(&shellyPlugS->options, "t", "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&shellyPlugS->options, "f", "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&shellyPlugS->options, "energy", "energy", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&shellyPlugS->options, "power", "power", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&shellyPlugS->options, "overtemperature", "overtemperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&shellyPlugS->options, "temperature", "temperature", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&shellyPlugS->options, "c", "connected", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, (void *)1, "^[10]{1}$");

	options_add(&shellyPlugS->options, "0", "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	shellyPlugS->initDev=&initDev;
	shellyPlugS->createCode=&createCode;
	shellyPlugS->printHelp=&printHelp;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "shellyplug-s";
	module->version = "1.6";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	shellyPlugSInit();
}
#endif
