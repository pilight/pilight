/*
	Copyright (C) 2015 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _STORAGE_T_
#define _STORAGE_T_

#include "../core/json.h"
#include "../core/pilight.h"
#include "../core/common.h"
#include "../core/eventpool_structs.h"
#include "../datatypes/stack.h"
#include "../hardware/hardware.h"
#include "../events/action.h"

typedef enum config_objects_t {
	CONFIG_DEVICES = 0x1,
	CONFIG_RULES = 0x2,
	CONFIG_GUI = 0x4,
	CONFIG_SETTINGS = 0x8,
	CONFIG_HARDWARE = 0x10,
	CONFIG_REGISTRY = 0x20,
	CONFIG_ALL = 0x3F
} config_objects_t;

#define CONFIG_INTERNAL	0
#define CONFIG_FORWARD	1
#define CONFIG_USER			2

typedef struct storage_t {
	char *id;

	int (*gc)(void);
	int (*sync)(void);
	int (*read)(char *, int);

	int (*devices_select)(enum origin_t, char *, struct JsonNode **);
	int (*rules_select)(enum origin_t, char *, struct JsonNode **);
	int (*gui_select)(enum origin_t, char *, struct JsonNode **);
	int (*hardware_select)(enum origin_t, char *, struct JsonNode **);
	int (*settings_select)(enum origin_t, char *, struct JsonNode **);
	int (*registry_select)(enum origin_t, char *, struct JsonNode **);

	struct storage_t *next;
} storage_t;

typedef struct rules_values_t {
	char *device;
	char *name;
	struct devices_settings_t *settings;
	struct rules_values_t *next;
} rules_values_t;

typedef struct rules_actions_t {
	void *ptr;
	struct rules_t *rule;
	struct JsonNode *arguments;
	struct event_actions_t *action;

	struct rules_actions_t *next;
} rules_actions_t;

typedef struct rules_t {
	char *rule;
	char *name;
	char **devices;
	int nrdevices;
	int nr;
	int status;
	struct {
		struct timespec first;
		struct timespec second;
	}	timestamp;
	unsigned short active;

	struct {
		struct stack_dt *lexemes;
	} condition;

	struct {
		struct stack_dt *lexemes;
	} action;

	/* Values from received protocols */
	struct JsonNode *jtrigger;
	/* Arguments to be send to the action */
	struct rules_actions_t *actions;
	struct rules_values_t *values;
	struct tree_t *tree;
	struct rules_t *next;
} rules_t;

struct device_t {
	char *id;
	time_t timestamp;
	int nrthreads;
#ifdef EVENTS
	int lastrule;
	int prevrule;
	struct event_action_thread_t *action_thread;
#endif
	struct protocol_t **protocols;
	int nrprotocols;

	char **protocols1;
	int nrprotocols1;

	struct device_t *next;
};

// extern struct device_t *config_device;
// extern struct storage_t *config_storage;

void storage_init(void);
int storage_read(char *, unsigned long);
int storage_export(void);
int storage_gc(void);
int storage_import(struct JsonNode *);
void storage_register(struct storage_t **, const char *);

struct JsonNode *config_print(int, const char *);
struct JsonNode *values_print(const char *);
void *config_values_update(int, void *);

void devices_import(struct JsonNode *);
int devices_select(enum origin_t, char *, struct JsonNode **);
int devices_select_struct(enum origin_t, char *, struct device_t **);
int devices_select_protocol(enum origin_t, char *, int, struct protocol_t **);
int devices_select_string_setting(enum origin_t, char *, char *, char **);
int devices_select_number_setting(enum origin_t, char *, char *, double *, int *);
int devices_select_settings(enum origin_t, char *, int i, char **, struct varcont_t *);
int devices_select_id(enum origin_t, char *, int, char **, struct varcont_t *);

int settings_select(enum origin_t, char *, struct JsonNode **);
int settings_select_number(enum origin_t, char *, double *);
int settings_select_string(enum origin_t, char *, char **);
int settings_select_number_element(enum origin_t, char *id, int, double *);
int settings_select_string_element(enum origin_t, char *id, int, char **);

int rules_select(enum origin_t, char *, struct JsonNode **);
int rules_select_struct(enum origin_t origin, char *id, struct rules_t **out);

int gui_check_media(enum origin_t, char *, const char *);
int gui_select(enum origin_t, char *, struct JsonNode **);

int hardware_select(enum origin_t, char *, struct JsonNode **);
int hardware_select_struct(enum origin_t, char *, struct hardware_t **);

int devices_validate_settings(struct JsonNode *, int);
int devices_validate_protocols(struct JsonNode *, int);
int devices_validate_duplicates(struct JsonNode *, int);
int devices_validate_name(struct JsonNode *, int);
int devices_validate_state(struct JsonNode *, int);
int devices_validate_values(struct JsonNode *, int);
int devices_validate_id(struct JsonNode *, int);

void devices_struct_parse(struct JsonNode *, int);
void devices_init_event_threads(struct JsonNode *, int);
void devices_init_protocol_threads(struct JsonNode *, int);

int gui_validate_duplicates(struct JsonNode *, int);
int gui_validate_id(struct JsonNode *, int);
int gui_validate_settings(struct JsonNode *, int);
int gui_validate_media(struct JsonNode *, int);

int hardware_validate_duplicated(struct JsonNode *, int);
int hardware_validate_id(struct JsonNode *, int);
int hardware_validate_settings(struct JsonNode *, int);

int settings_validate_duplicates(struct JsonNode *, int);
int settings_validate_settings(struct JsonNode *, int);
int settings_validate_values(struct JsonNode *, int);

int registry_select(enum origin_t, char *, struct JsonNode **);
int registry_select_number(enum origin_t, char *, double *, int *);
int registry_select_string(enum origin_t, char *, char **);

int registry_update(enum origin_t origin, const char *key, struct JsonNode *jvalues);
int registry_delete(enum origin_t origin, const char *key);

/*
 *
 */

int devices_get_type(enum origin_t, char *, int, int *);
int devices_get_state(enum origin_t, char *, int, char **);
int devices_get_value(enum origin_t, char *, int, char *, char **);
int devices_get_string_setting(enum origin_t, char *, int, char *, char **);
int devices_get_number_setting(enum origin_t, char *, int, char *, int *);
int devices_is_state(enum origin_t, char *, int, char *);
int devices_has_parameter(enum origin_t, char *, int, char *);
#endif
