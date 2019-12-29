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
#include "tasmota_switch.h"

static void *reason_send_code_free(void *param) {
	plua_metatable_free(param);
	return NULL;
}

static int createCode(struct JsonNode *code) {
	char *id = NULL;
	int state = -1;
	int readonly = -1;
	double itmp = -1;

	json_find_string(code, "id", &id);
	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;
	else if(json_find_number(code, "readonly", &itmp) == 0)
		readonly = itmp;

	if(id == NULL || (state == -1 && readonly == -1)) {
		logprintf(LOG_ERR, "tasmota_switch: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id == NULL) {
		logprintf(LOG_ERR, "tasmota_switch: invalid id");
		return EXIT_FAILURE;
	} else {
		struct plua_metatable_t *table = NULL;
		plua_metatable_init(&table);
		plua_metatable_set_string(table, "id", id);
		if(state == 1) {
			plua_metatable_set_string(table, "state", "on");
		} else if(state == 0) {
			plua_metatable_set_string(table, "state", "off");
		} else if(readonly != -1) {
			plua_metatable_set_number(table, "readonly", readonly);
		}
		plua_metatable_set_string(table, "protocol", tasmotaSwitch->id);
		plua_metatable_set_number(table, "hwtype", TASMOTA);
		plua_metatable_set_string(table, "uuid", "0");

		eventpool_trigger(REASON_SEND_CODE+10000, reason_send_code_free, table);
	}
	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -t --on\t\t\tsend an on signal\n");
	printf("\t -f --off\t\t\tsend an off signal\n");
	printf("\t -i --id=id\t\t\tcontrol a device with this id\n");
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

				json_append_element(jprotocol, json_mkstring("tasmota_switch"));

				json_append_member(jcode, "id", json_mkstring(id));
				json_append_member(jcode, "readonly", json_mknumber(1, 0));
				json_append_member(jcode, "protocol", jprotocol);

				json_append_member(jobject, "code", jcode);
				json_append_member(jobject, "action", json_mkstring("send"));

				if(pilight.send(jobject, PROTOCOL) == 0) {

				}
			}
			jchild = jchild->next;
		}
	}
	return NULL;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void tasmotaSwitchInit(void) {
	protocol_register(&tasmotaSwitch);
	protocol_set_id(tasmotaSwitch, "tasmota_switch");
	protocol_device_add(tasmotaSwitch, "tasmota_switch", "Tasmota Switch");
	tasmotaSwitch->devtype = SWITCH;
	tasmotaSwitch->hwtype = TASMOTA;

	options_add(&tasmotaSwitch->options, "i", "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, NULL);
	options_add(&tasmotaSwitch->options, "t", "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&tasmotaSwitch->options, "f", "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&tasmotaSwitch->options, "r", "readonly", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, (void *)1, "^[10]{1}$");

	tasmotaSwitch->initDev=&initDev;
	tasmotaSwitch->createCode=&createCode;
	tasmotaSwitch->printHelp=&printHelp;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "tasmota_switch";
	module->version = "1.6";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	tasmotaSwitchInit();
}
#endif
