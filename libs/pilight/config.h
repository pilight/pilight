/*
	Copyright (C) 2013 CurlyMo

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

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "protocol.h"

typedef struct conf_locations_t conf_locations_t;
typedef struct conf_devices_t conf_devices_t;
typedef struct conf_settings_t conf_settings_t;
typedef struct conf_values_t conf_values_t;

/*
|------------------|
| conf_locations_t |
|------------------|
| id               |
| name		       |
| devices	       | ---
|------------------|   |
				       |
|------------------|   |
|  conf_devices_t  | <--
|------------------|
| id               |
| name		       |
| protocol	       | --> protocol_t <protocol.h>
| settings	       | ---
|------------------|   |
				       |
|------------------|   |
| conf_settings_t  | <--
|------------------|
| name             |
| values	       | ---
|------------------|   |
				       |
|------------------|   |
|  conf_values_t   | <--
|------------------|
| value            |
| type		       |
|------------------|
*/

typedef enum {
	CONFIG_TYPE_UNDEFINED,
	CONFIG_TYPE_NUMBER,
	CONFIG_TYPE_STRING
} config_type_t;

struct conf_values_t {
	char *value;
	config_type_t type;
	struct conf_values_t *next;
};

struct conf_settings_t {
	char *name;
	struct conf_values_t *values;
	struct conf_settings_t *next;
};

struct conf_devices_t {
	char *id;
	char *name;
	char *protoname;
	struct protocol_t *protopt;
	struct conf_settings_t *settings;
	struct conf_devices_t *next;
};

struct conf_locations_t {
	char *id;
	char *name;
	struct conf_devices_t *devices;
	struct conf_locations_t *next;
};

char *progname;

/* Struct to store the locations */
struct conf_locations_t *conf_locations;
/* Struct to store the devices per location */
struct conf_devices_t *conf_devices;
/* Struct to store the device specific settings */
struct conf_settings_t *conf_settings;
/* Struct to store the device values list settings */
struct conf_values_t *conf_values;

/* The default config file location */
char *configfile;

int config_update(char *protoname, JsonNode *message, JsonNode *out);
int config_get_location(char *id, struct conf_locations_t **loc);
int config_get_device(char *lid, char *sid, struct conf_devices_t **dev);
int config_valid_state(char *lid, char *sid, char *state);
int config_valid_value(char *lid, char *sid, char *name, char *value);
JsonNode *config2json(void);
void config_print(void);
void config_save_setting(int i, JsonNode *jsetting, struct conf_settings_t *snode);
int config_check_state(int i, JsonNode *jsetting, struct conf_devices_t *device);
int config_parse_devices(JsonNode *jdevices, struct conf_devices_t *device);
int config_parse_locations(JsonNode *jlocations, struct conf_locations_t *location);
int config_parse(JsonNode *root);
int config_write(char *content);
int config_read(void);
int config_set_file(char *cfgfile);
int config_gc(void);

#endif
