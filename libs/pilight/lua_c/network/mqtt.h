/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _LUA_NETWORK_MQTT_H_
#define _LUA_NETWORK_MQTT_H_

typedef struct lua_mqtt_t {
	PLUA_INTERFACE_FIELDS

	struct mqtt_client_t *client;

	char *callback;
} lua_mqtt_t;

int plua_network_mqtt(struct lua_State *L);

#endif