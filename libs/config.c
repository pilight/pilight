/*
	Copyright 2013 CurlyMo

	This file is part of the Raspberry Pi 433.92Mhz transceiver.

    Raspberry Pi 433.92Mhz transceiver is free software: you can redistribute
	it and/or modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation, either version 3 of the License,
	or (at your option) any later version.

    Raspberry Pi 433.92Mhz transceiver is distributed in the hope that it will
	be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Raspberry Pi 433.92Mhz transceiver. If not, see
	<http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <regex.h>
#include <sys/stat.h>

#include "config.h"
#include "log.h"
#include "options.h"
#include "protocol.h"

void config_update(char *protoname, JsonNode *json) {
	/* The pointer to the config locations */
	struct conf_locations_t *lptr = locations;
	/* The pointer to the config devices */
	struct conf_devices_t *dptr = NULL;
	/* The pointer to the device settings */
	struct conf_settings_t *sptr = NULL;
	/* The pointer to the registered protocols */
	struct protocol_t *protocol = NULL;
	/* The pointer to the protocol options */
	struct options_t *opt = NULL;
	/* Get the code part of the sended code */
	JsonNode *code = json_find_member(json, "code");

	/* Temporarily char pointer */
	char *stmp = NULL;
	/* Temporarily char array */
	char ctmp[10];
	/* Temporarily int */
	int itmp;
	/* Loop short */
	unsigned short i = 0;
	/* Do we need to update the config file */
	unsigned short update = 0;
	/* The new state value */
	char state[10];

	/* Make sure the character poinrter are empty */
	memset(state, '\0', sizeof(state));
	memset(ctmp, '\0', sizeof(ctmp));

	/* Check if the found settings matches the send code */
	int match1 = 0;
	int match2 = 0;

	/* Retrieve the used protocol */
	for(i=0;i<protocols.nr; ++i) {
		protocol = protocols.listeners[i];

		/* Check if the protocol exists */

		if(strcmp(protocol->id, protoname) == 0) {
			break;
		}
	}

	/* Only loop through all locations if the protocol has options */
	if((opt = protocol->options) != NULL) {
		/* Loop through all location */
		while(lptr) {
			dptr = lptr->devices;
			/* Loop through all devices of this location */
			while(dptr) {
				match1 = 0; match2 = 0;

				if(providesDevice(&protocol, dptr->protoname) == 0) {
					opt = protocol->options;
					/* Loop through all protocol options */
					while(opt) {
						sptr = dptr->settings;
						/* Loop through all settings */
						while(sptr) {

							/* Check the config id's to match a device */
							if(opt->conftype == config_id && strcmp(sptr->name, opt->name) == 0) {
								match1++;
								if(json_find_string(code, opt->name, &stmp) == 0) {
									strcpy(ctmp, stmp);
								}
								if(json_find_number(code, opt->name, &itmp) == 0) {
									sprintf(ctmp, "%d", itmp);
								}

								if(strcmp(ctmp, sptr->values->value) == 0) {
									match2++;
								}
							}
							/* Retrieve the new device state */
							if(opt->conftype == config_state && strlen(state) == 0) {
								if(opt->argtype == no_value) {
									if(json_find_string(code, "state", &stmp) == 0) {
										strcpy(state, stmp);
									}
									if(json_find_number(code, "state", &itmp) == 0) {
										sprintf(state, "%d", itmp);
									}
								} else if(opt->argtype == has_value) {
									if(json_find_string(code, opt->name, &stmp) == 0) {
										strcpy(state, stmp);
									}
									if(json_find_number(code, opt->name, &itmp) == 0) {
										sprintf(state, "%d", itmp);
									}
								}
							}
							sptr = sptr->next;
						}
						opt = opt->next;
					}
					/* If we matched a config device, update it's state */
					if(match1 > 0 && match2 > 0 && match1 == match2) {
						sptr = dptr->settings;
						while(sptr) {
							opt = protocol->options;
							/* Loop through all protocol options */
							while(opt) {
								/* Check if there are values that can be updated */
								if(strcmp(sptr->name, opt->name) == 0 && opt->conftype == config_value && opt->argtype == has_value) {
									if(json_find_string(code, opt->name, &stmp) == 0) {
										strcpy(ctmp, stmp);
									}
									if(json_find_number(code, opt->name, &itmp) == 0) {
										sprintf(ctmp, "%d", itmp);
									}
									if(strcmp(sptr->values->value, ctmp) != 0) {
										sptr->values->value = strdup(ctmp);
										update = 1;
										break;
									}
								}
								opt = opt->next;
							}
							/* Check if we need to update the state */
							if(strcmp(sptr->name, "state") == 0 && strcmp(sptr->values->value, state) != 0) {
								sptr->values->value = strdup(state);
								update = 1;
								break;
							}
							sptr = sptr->next;
						}
					}
				}
				dptr = dptr->next;
			}
			lptr = lptr->next;
		}
	}
	/* Only update the config file, if a state change occured */
	if(update == 1) {
		config_write(json_stringify(config2json(), "\t"));
	}
	json_delete(json);
}

int config_get_location(char *id, struct conf_locations_t **loc) {
	struct conf_locations_t *lptr = locations;
	while(lptr) {
		if(strcmp(lptr->id, id) == 0) {
			*loc = lptr;
			return 0;
		}
		lptr = lptr->next;
	}
	return 1;
}

int config_get_device(char *lid, char *sid, struct conf_devices_t **dev) {
	struct conf_locations_t *lptr = NULL;
	struct conf_devices_t *dptr = NULL;

	if(config_get_location(lid, &lptr) == 0) {
		while(lptr) {
			dptr = lptr->devices;
			while(dptr) {
				if(strcmp(dptr->id, sid) == 0) {
					*dev = dptr;
					return 0;
				}
				dptr = dptr->next;
			}
			lptr = lptr->next;
		}
	}
	return 1;
}

/* http://stackoverflow.com/a/13654646 */
void config_reverse_struct(struct conf_locations_t **loc) {
    if(!loc || !*loc)
        return;

    struct conf_locations_t *lptr = *loc, *lnext = NULL, *lprev = NULL;
    struct conf_devices_t *dptr = NULL, *dnext = NULL, *dprev = NULL;
    struct conf_settings_t *sptr = NULL, *snext = NULL, *sprev = NULL;
    struct conf_values_t *vptr = NULL, *vnext = NULL, *vprev = NULL;

    while(lptr) {

		dptr = lptr->devices;
		while(dptr != NULL) {

			sptr = dptr->settings;
			while(sptr != NULL) {

				vptr = sptr->values;
				while(vptr != NULL) {
					vnext = vptr->next;
					vptr->next = vprev;
					vprev = vptr;
					vptr = vnext;
				}
				sptr->values = vprev;
				vptr = NULL;
				vprev = NULL;
				vnext = NULL;

				snext = sptr->next;
				sptr->next = sprev;
				sprev = sptr;
				sptr = snext;
			}
			dptr->settings = sprev;
			sptr = NULL;
			sprev = NULL;
			snext = NULL;

			dnext = dptr->next;
			dptr->next = dprev;
			dprev = dptr;
			dptr = dnext;
		}
		lptr->devices = dprev;
		dptr = NULL;
		dprev = NULL;
		dnext = NULL;

        lnext = lptr->next;
        lptr->next = lprev;
        lprev = lptr;
        lptr = lnext;
    }

    *loc = lprev;
}

JsonNode *config2json(void) {
	/* Temporary pointer to the different structure */
	struct conf_locations_t *tmp_locations = NULL;
	struct conf_devices_t *tmp_devices;
	struct conf_settings_t *tmp_settings;
	struct conf_values_t *tmp_values;

	/* Pointers to the newly created JSON object */
	struct JsonNode *root = json_mkobject();
	struct JsonNode *location = json_mkobject();
	struct JsonNode *device = json_mkobject();
	struct JsonNode *setting = json_mkobject();

	/* Make sure we preserve the order of the original file */
	tmp_locations = locations;

	/* Show the parsed log file */
	while(tmp_locations != NULL) {
		location = json_mkobject();
		json_append_member(location, "name", json_mkstring(tmp_locations->name));

		tmp_devices = tmp_locations->devices;
		while(tmp_devices != NULL) {
			device = json_mkobject();
			json_append_member(device, "name", json_mkstring(tmp_devices->name));
			json_append_member(device, "protocol", json_mkstring(tmp_devices->protoname));
			json_append_member(device, "type", json_mknumber(tmp_devices->protopt->type));

			tmp_settings = tmp_devices->settings;
			while(tmp_settings != NULL) {

				tmp_values = tmp_settings->values;
				if(tmp_values->next == NULL) {
					if(tmp_values->type == CONFIG_TYPE_NUMBER) {
						json_append_member(device, tmp_settings->name, json_mknumber(atoi(tmp_values->value)));
					} else if(tmp_values->type == CONFIG_TYPE_STRING) {
						json_append_member(device, tmp_settings->name, json_mkstring(tmp_values->value));
					}
				} else {
					setting = json_mkarray();
					while(tmp_values != NULL) {
						if(tmp_values->type == CONFIG_TYPE_NUMBER) {
							json_append_element(setting, json_mknumber(atoi(tmp_values->value)));
						} else if(tmp_values->type == CONFIG_TYPE_STRING) {
							json_append_element(setting, json_mkstring(tmp_values->value));
						}
						tmp_values = tmp_values->next;
					}
					json_append_member(device, tmp_settings->name, setting);
				}
				tmp_settings = tmp_settings->next;
			}
			json_append_member(location, tmp_devices->id, device);
			tmp_devices = tmp_devices->next;
		}
		json_append_member(root, tmp_locations->id, location);
		tmp_locations = tmp_locations->next;
	}

	return root;
}

void config_print(void) {
	logprintf(LOG_DEBUG, "-- start parsed config file --");
	printf("%s\n", json_stringify(config2json(), "\t"));
	logprintf(LOG_DEBUG, "-- end parsed config file --");
}

/* If a fault was found in the config file, clear everything */
void config_clear(void) {
	values = NULL;
	settings = NULL;
	devices = NULL;
	locations = NULL;
}

/* Save the device settings to the device struct */
void config_save_setting(int i, JsonNode *jsetting, struct conf_settings_t *snode) {
	/* Struct to store the values */
	struct conf_values_t *vnode = NULL;
	/* Temporary JSON pointer */
	struct JsonNode *jtmp;

	/* Variable holder for casting settings */
	char *stmp = NULL;
	char ctmp[256];
	int itmp = 0;

	/* New device settings node */
	snode = malloc(sizeof(struct conf_settings_t));
	snode->name = strdup(jsetting->key);

	/* If the JSON tag is an array, then i should be a values array */
	if(jsetting->tag == JSON_ARRAY) {
		/* Loop through the values of this values array */
		jtmp = json_first_child(jsetting);
		while(jtmp != NULL) {
			/* New values node */
			vnode = malloc(sizeof(struct conf_values_t));
			/* Cast and store the new value */
			if(jtmp->tag == JSON_STRING) {
				vnode->value = strdup(jtmp->string_);
				vnode->type = CONFIG_TYPE_STRING;
			} else if(jtmp->tag == JSON_NUMBER) {
				sprintf(ctmp, "%d", (int)jtmp->number_);
				vnode->value = strdup(ctmp);
				vnode->type = CONFIG_TYPE_NUMBER;
			}
			jtmp = jtmp->next;
			vnode->next = values;
			values = vnode;
		}
	} else {
		vnode = malloc(sizeof(struct conf_values_t));

		/* Cast and store the new value */
		if(jsetting->tag == JSON_STRING && json_find_string(jsetting->parent, jsetting->key, &stmp) == 0) {
			vnode->value = strdup(stmp);
			vnode->type = CONFIG_TYPE_STRING;
		} else if(jsetting->tag == JSON_NUMBER && json_find_number(jsetting->parent, jsetting->key, &itmp) == 0) {
			sprintf(ctmp, "%d", itmp);
			vnode->value = strdup(ctmp);
			vnode->type = CONFIG_TYPE_NUMBER;
		}
		vnode->next = values;
		values = vnode;
	}

	snode->values = malloc(MAX_VALUES*sizeof(struct conf_values_t));
	/* Only store values if they are present */
	if(values != NULL) {
		memcpy(snode->values, values, (MAX_VALUES*sizeof(struct conf_values_t)));
	} else {
		snode->values = NULL;
	}
	snode->next = settings;
	settings = snode;

	/* Make sure to clean all pointer so values don't end up in subsequent structure */
	values = NULL;
	if(vnode != NULL && vnode->next != NULL)
		vnode->next = NULL;
}

int config_check_state(int i, JsonNode *jsetting, struct conf_devices_t *device) {
	/* Temporary options pointer */
	struct options_t *tmp_options;

	/* Temporary JSON pointer */
	struct JsonNode *jtmp;
	/* JSON pointer to the values array */
	struct JsonNode *jvalues = NULL;

	int valid_state = 0, valid_value = 0, have_error = 0, has_values = 0;

	/* Variable holders for casting */
	int itmp = 0;
	char ctmp[256], ctmp1[256];
	char *stmp = NULL;

	/* Regex variables */
	regex_t regex;
	int reti;

	/* Cast the different values */
	if(jsetting->tag == JSON_NUMBER && json_find_number(jsetting->parent, jsetting->key, &itmp) == 0) {
		sprintf(ctmp, "%d", itmp);
	} else if(jsetting->tag == JSON_STRING && json_find_string(jsetting->parent, jsetting->key, &stmp) == 0) {
		strcpy(ctmp, stmp);
	}

	/* Search for the values setting */
	jtmp = json_first_child(jsetting->parent);
	while(jtmp != NULL) {
		if(strcmp(jtmp->key, "values") == 0 && jtmp->tag == JSON_ARRAY) {
			jvalues = jtmp;
			has_values = 1;
			break;
		}
		jtmp = jtmp->next;
	}

	/* Check if the specific device contains any options */
	if(device->protopt->options != NULL) {

		/* If we have a state setting, we also must have a values setting */
		if(has_values == 0) {
			logprintf(LOG_ERR, "setting \"%s\" of \"%s\", missing", "values", device->id);
			have_error = 1;
			goto clear;
		}

		tmp_options = device->protopt->options;

		while(tmp_options != NULL && tmp_options->name != NULL) {
			/* We are only interested in the config_* options */
			if(tmp_options->conftype == config_state) {
				/* If an option requires an argument, then check if the
				   argument state values and values array are of the right
				   type. This is done by checking the regex mask */
				if(tmp_options->argtype == has_value) {
					if(strlen(tmp_options->mask) > 0) {
						reti = regcomp(&regex, tmp_options->mask, REG_EXTENDED);
						if(reti) {
							logprintf(LOG_ERR, "could not compile regex");
							have_error = 1;
							goto clear;
						}
						reti = regexec(&regex, ctmp, 0, NULL, 0);

						if(reti == REG_NOMATCH || reti != 0) {
							logprintf(LOG_ERR, "setting #%d \"%s\" of \"%s\", invalid", i, jsetting->key, device->id);
							have_error = 1;
							goto clear;
						} else {
							/* Also check if the values in the values array
							   and the state value matches */
							jtmp = json_first_child(jvalues);
							while(jtmp != NULL) {
								if(jtmp->tag == JSON_NUMBER) {
									sprintf(ctmp1, "%d", (int)jtmp->number_);
								} else if(jtmp->tag == JSON_STRING) {
									strcpy(ctmp1, jtmp->string_);
								}
								reti = regexec(&regex, ctmp1, 0, NULL, 0);
								if(reti == REG_NOMATCH || reti != 0) {
									logprintf(LOG_ERR, "setting #%d \"%s\" of \"%s\", invalid", i, jsetting->key, device->id);
									have_error = 1;
									goto clear;
								}
								if(strcmp(ctmp, ctmp1) == 0) {
									valid_state = 1;
								}
								jtmp = jtmp->next;
							}
						}
						if(valid_state == 0) {
							logprintf(LOG_ERR, "setting #%d \"%s\" of \"%s\", invalid", i, jsetting->key, device->id);
							have_error = 1;
							goto clear;
						}
					}
				} else {
					/* If a protocol has config_state arguments, than these define
					   the states a protocol can take. Check if the state value
					   and the values in the values array match, these protocol states */
					valid_value = 0;
					jtmp = json_first_child(jvalues);
					while(jtmp != NULL) {
						if(jtmp->tag == JSON_NUMBER) {
							sprintf(ctmp1, "%d", (int)jtmp->number_);
						} else if(jtmp->tag == JSON_STRING) {
							strcpy(ctmp1, jtmp->string_);
						}
						if(strcmp(tmp_options->name, ctmp1) == 0) {
							valid_value = 1;
						}
						jtmp = jtmp->next;
					}
					if(valid_value == 0) {
						logprintf(LOG_ERR, "setting #%d \"%s\" of \"%s\", invalid", i, jsetting->key, device->id);
						have_error = 1;
						goto clear;
					}
					if(strcmp(tmp_options->name, ctmp) == 0 && valid_state == 0) {
						valid_state = 1;
					}
				}
			}
			tmp_options = tmp_options->next;
		}
	}

	if(valid_state == 0) {
		logprintf(LOG_ERR, "setting #%d \"%s\" of \"%s\", invalid", i, jsetting->key, device->id);
		have_error = 1;
		goto clear;
	}

clear:
	if(have_error == 1) {
		return EXIT_FAILURE;
	} else {
		return EXIT_SUCCESS;
	}
}

int config_parse_devices(JsonNode *jdevices, struct conf_devices_t *device) {
	/* Struct to store the settings */
	struct conf_settings_t *snode = NULL;
	/* Temporary settings holder */
	struct conf_settings_t *tmp_settings = NULL;
	/* JSON devices iterator */
	JsonNode *jsettings = json_mkobject();
	/* Temporarily options pointer */
	struct options_t *tmp_options;

	int i = 0, have_error = 0, valid_setting = 0, match = 0, has_state = 0;
	/* Check for any duplicate fields */
	int nrname = 0, nrprotocol = 0, nrstate = 0, nrvalues = 0;

	jsettings = json_first_child(jdevices);
	while(jsettings != NULL) {
		i++;
		/* Check for any duplicate fields */
		if(strcmp(jsettings->key, "name") == 0) {
			nrname++;
		}
		if(strcmp(jsettings->key, "protocol") == 0) {
			nrprotocol++;
		}
		if(strcmp(jsettings->key, "state") == 0) {
			nrstate++;
		}
		if(strcmp(jsettings->key, "values") == 0) {
			nrvalues++;
		}
		if(nrstate > 1 || nrvalues > 1 || nrprotocol > 1 || nrname > 1) {
			logprintf(LOG_ERR, "settting #%d \"%s\" of \"%s\", duplicate", i, jsettings->key, device->id);
			have_error = 1;
			goto clear;
		}
		/* Parse the state setting separately from the other settings.
		   Any errors in the values array will be found in the state check */
		if(strcmp(jsettings->key, "state") == 0 && (jsettings->tag == JSON_STRING || jsettings->tag == JSON_NUMBER)) {
			if(config_check_state(i, jsettings, device) != 0) {
				have_error = 1;
				goto clear;
			}
			config_save_setting(i, jsettings, snode);
		} else if(strcmp(jsettings->key, "values") == 0 && jsettings->tag == JSON_ARRAY) {
			config_save_setting(i, jsettings, snode);
		/* The protocol and name settings are already saved in the device struct */
		} else if(!((strcmp(jsettings->key, "name") == 0 && jsettings->tag == JSON_STRING)
			|| (strcmp(jsettings->key, "protocol") == 0 && jsettings->tag == JSON_STRING)
			|| (strcmp(jsettings->key, "type") == 0 && jsettings->tag == JSON_NUMBER))) {

			/* Check for duplicate settings */
			tmp_settings = settings;
			while(tmp_settings != NULL) {
				if(strcmp(tmp_settings->name, jsettings->key) == 0) {
					logprintf(LOG_ERR, "settting #%d \"%s\" of \"%s\", duplicate", i, jsettings->key, device->id);
					have_error = 1;
					goto clear;
				}
				tmp_settings = tmp_settings->next;
			}

			/* Check if the settings are required by the protocol */
			valid_setting = 0;
			if(device->protopt->options != NULL) {
				tmp_options = device->protopt->options;
				while(tmp_options != NULL && tmp_options->name != NULL) {
					if(strcmp(jsettings->key, tmp_options->name) == 0 && (tmp_options->conftype == config_id || tmp_options->conftype == config_value)) {
						valid_setting = 1;
						break;
					}
					tmp_options = tmp_options->next;
				}
			}

			/* If the settings are valid, store them */
			if(valid_setting == 1) {
				config_save_setting(i, jsettings, snode);
			} else {
				logprintf(LOG_ERR, "setting #%d \"%s\" of \"%s\", invalid", i, jsettings->key, device->id);
				have_error = 1;
				goto clear;
			}
		}
		jsettings = jsettings->next;
	}

	/* Check if required options by this protocol are missing
	   in the config file */
	if(device->protopt->options != NULL) {
		tmp_options = device->protopt->options;
		while(tmp_options != NULL && tmp_options->name != NULL) {
			match = 0;
			jsettings = json_first_child(jdevices);
			while(jsettings != NULL) {
				if(strcmp(jsettings->key, tmp_options->name) == 0) {
					match = 1;
					break;
				}
			jsettings = jsettings->next;
			}
			if(match == 0 && (tmp_options->conftype == config_id || tmp_options->conftype == config_value)) {
				logprintf(LOG_ERR, "setting \"%s\" of \"%s\", missing", tmp_options->name, device->id);
				have_error = 1;
				goto clear;
			}
		tmp_options = tmp_options->next;
		}
	}
	/* Check if this protocol requires a state and values setting
	   to be specified and if they are missing when it is. */
	if(nrstate == 0 || nrvalues == 0) {
		if(device->protopt->options != NULL) {
			tmp_options = device->protopt->options;
			while(tmp_options != NULL && tmp_options->name != NULL) {
				if(tmp_options->conftype == config_state) {
					has_state = 1;
					break;
				}
				tmp_options = tmp_options->next;
			}
		}
		if(has_state == 1) {
			if(nrstate == 0) {
				logprintf(LOG_ERR, "setting \"%s\" of \"%s\", missing", "state", device->id);
				have_error = 1;
				goto clear;
			}

		}
	}

	device->settings = malloc(MAX_SETTINGS*sizeof(struct conf_settings_t));
	/* Only store devices if they are present */
	if(settings != NULL) {
		memcpy(device->settings, settings, (MAX_SETTINGS*sizeof(struct conf_settings_t)));
	} else {
		device->settings = NULL;
	}

	/* Clear the locations struct for the next location */
	settings = NULL;
	if(snode != NULL && snode->next != NULL)
		snode->next = NULL;

clear:
	if(have_error == 1) {
		return EXIT_FAILURE;
	} else {
		return EXIT_SUCCESS;
	}
}

int config_parse_locations(JsonNode *jlocations, struct conf_locations_t *location) {
	/* Struct to store the devices */
	struct conf_devices_t *dnode = NULL;
	/* Temporary JSON devices  */
	struct conf_devices_t *tmp_devices = NULL;
	/* JSON devices iterator */
	JsonNode *jdevices = json_mkobject();
	/* Device name */
	char *name = NULL;
	/* Protocol name */
	char *pname = NULL;
	/* Pointer to the match protocol */
	protocol_t *protocol = malloc(sizeof(protocol_t));

	int i = 0, have_error = 0, match = 0;
	/* Check for duplicate name setting */
	int nrname = 0;

	jdevices = json_first_child(jlocations);
	while(jdevices != NULL) {
		i++;

		/* Check for duplicate name setting */
		if(strcmp(jdevices->key, "name") == 0) {
			nrname++;
			if(nrname > 1) {
				logprintf(LOG_ERR, "device #%d \"%s\" of \"%s\", duplicate", i, jdevices->key, location->id);
				have_error = 1;
				goto clear;
			}
		}
		/* Check if all fields of the devices are of the right type */
		if(!((strcmp(jdevices->key, "name") == 0 && jdevices->tag == JSON_STRING) || (jdevices->tag == JSON_OBJECT))) {
			logprintf(LOG_ERR, "device #%d \"%s\" of \"%s\", invalid field(s)", i, jdevices->key, location->id);
			have_error = 1;
			goto clear;
		} else if(jdevices->tag == JSON_OBJECT) {
			/* Save the name of the device */
			if(json_find_string(jdevices, "name", &name) != 0) {
				logprintf(LOG_ERR, "device #%d \"%s\" of \"%s\", missing name", i, jdevices->key, location->id);
				have_error = 1;
				goto clear;
			/* Save the protocol of the device */
			} else if(json_find_string(jdevices, "protocol", &pname) != 0) {
				logprintf(LOG_ERR, "device #%d \"%s\" of \"%s\", missing protocol", i, jdevices->key, location->id);
				have_error = 1;
				goto clear;
			} else {

				/* Check for duplicate fields */
				tmp_devices = devices;
				while(tmp_devices != NULL) {
					if(strcmp(tmp_devices->id, jdevices->key) == 0) {
						logprintf(LOG_ERR, "device #%d \"%s\" of \"%s\", duplicate", i, jdevices->key, location->id);
						have_error = 1;
						goto clear;
					}
					tmp_devices = tmp_devices->next;
				}

				match = 0;
				for(i=0;i<protocols.nr; ++i) {
					protocol = protocols.listeners[i];

					/* Check if the protocol exists */
					if(providesDevice(&protocol, pname) == 0 && match == 0) {
						match = 1;
						break;
					}
				}

				if(match == 0) {
					logprintf(LOG_ERR, "device #%d \"%s\" of \"%s\", invalid protocol", i, jdevices->key, location->id);
					have_error = 1;
					goto clear;
				}

				dnode = malloc(sizeof(struct conf_devices_t));
				dnode->id = strdup(jdevices->key);
				dnode->name = strdup(name);
				/* Save both the protocol pointer and the protocol name */
				dnode->protoname = strdup(pname);
				dnode->protopt = protocol;
				dnode->next = devices;

				if(config_parse_devices(jdevices, dnode) != 0) {
					have_error = 1;
					goto clear;
				}

				devices = dnode;
			}
		}

		jdevices = jdevices->next;
	}

	location->devices = malloc(MAX_DEVICES*sizeof(struct conf_devices_t));
	/* Only store devices if they are present */
	if(devices != NULL) {
		memcpy(location->devices, devices, (MAX_DEVICES*sizeof(struct conf_devices_t)));
	} else {
		location->devices = NULL;
	}
	/* Clear the locations struct for the next location */
	devices = NULL;
	if(dnode != NULL && dnode->next != NULL)
		dnode->next = NULL;

clear:
	if(have_error == 1) {
		return EXIT_FAILURE;
	} else {
		return EXIT_SUCCESS;
	}
}

int config_parse(JsonNode *root) {
	/* Struct to store the locations */
	struct conf_locations_t *lnode = NULL;
	struct conf_locations_t *tmp_locations = NULL;
	/* JSON locations iterator */
	JsonNode *jlocations = json_mkobject();
	/* Location name */
	char *name = NULL;

	int i = 0, have_error = 0;

	jlocations = json_first_child(root);

	while(jlocations != NULL) {
		i++;
		/* An location can only be a JSON object */
		if(jlocations->tag != 5) {
			logprintf(LOG_ERR, "location #%d \"%s\", invalid format", i, jlocations->key);
			have_error = 1;
			goto clear;
		/* Check if the location has a name */
		} else if(json_find_string(jlocations, "name", &name) != 0) {
			logprintf(LOG_ERR, "location #%d \"%s\", missing name", i, jlocations->key);
			have_error = 1;
			goto clear;
		} else {
			/* Check for duplicate locations */
			tmp_locations = locations;
			while(tmp_locations != NULL) {
				if(strcmp(tmp_locations->id, jlocations->key) == 0) {
					logprintf(LOG_ERR, "location #%d \"%s\", duplicate", i, jlocations->key);
					have_error = 1;
					goto clear;
				}
				tmp_locations = tmp_locations->next;
			}

			lnode = malloc(sizeof(struct conf_locations_t));
			lnode->id = strdup(jlocations->key);
			lnode->name = strdup(name);
			lnode->next = locations;

			if(config_parse_locations(jlocations, lnode) != 0) {
				have_error = 1;
				goto clear;
			}

			locations = lnode;

			jlocations = jlocations->next;
		}
	}
	/* Preverse the original order inside the structs as in the config file */
	config_reverse_struct(&locations);
clear:
	if(have_error == 1) {
		config_clear();
		return EXIT_FAILURE;
	} else {
		return EXIT_SUCCESS;
	}
}

int config_write(char *content) {
	FILE *fp;

	/* Overwrite config file with proper format */
	if((fp = fopen(configfile, "w+")) == NULL) {
		logprintf(LOG_ERR, "cannot write config file: %s", configfile);
		return EXIT_FAILURE;
	}
	fseek(fp, 0L, SEEK_SET);
 	fwrite(content, sizeof(char), strlen(content), fp);
	fclose(fp);

	return EXIT_SUCCESS;
}

int config_read() {
	FILE *fp;
	char *content;
	size_t bytes;
	JsonNode *root;
	struct stat st;

	/* Read JSON config file */
	if((fp = fopen(configfile, "rb")) == NULL) {
		logprintf(LOG_ERR, "cannot read config file: %s", configfile);
		return EXIT_FAILURE;
	}

	fstat(fileno(fp), &st);
	bytes = (size_t)st.st_size;

	if((content = calloc(bytes+1, sizeof(char))) == NULL) {
		logprintf(LOG_ERR, "out of memory", configfile);
		return EXIT_FAILURE;
	}

	fread(content, sizeof(char), bytes, fp);
	fclose(fp);

	/* Validate JSON and turn into JSON object */
	if(json_validate(content) == false) {
		logprintf(LOG_ERR, "config is not in a valid json format", content);
		free(content);
		return EXIT_FAILURE;
	}
	root = json_decode(content);

	free(content);

	config_parse(root);
	config_write(json_stringify(root, "\t"));

	return EXIT_SUCCESS;
}

int config_set_file(char *cfgfile) {
	if(access(cfgfile, F_OK) != -1) {
		strcpy(configfile, cfgfile);
	} else {
		fprintf(stderr, "%s: the config file %s does not exists\n", progname, cfgfile);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
