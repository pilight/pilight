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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <regex.h>
#include <sys/stat.h>
#include <ctype.h>

#include "../../pilight.h"
#include "config.h"
#include "common.h"
#include "log.h"
#include "options.h"
#include "protocol.h"
#include "ssdp.h"
#include "firmware.h"

#ifdef UPDATE
	#include "update.h"
#endif

/* Struct to store the locations */
struct conf_locations_t *conf_locations = NULL;

int config_update(char *protoname, JsonNode *json, JsonNode **out) {
	/* The pointer to the config locations */
	struct conf_locations_t *lptr = conf_locations;
	/* The pointer to the config devices */
	struct conf_devices_t *dptr = NULL;
	/* The pointer to the device settings */
	struct conf_settings_t *sptr = NULL;
	/* The pointer to the device settings */
	struct conf_values_t *vptr = NULL;	
	/* The pointer to the registered protocols */
	struct protocol_t *protocol = NULL;
	/* The pointer to the protocol options */
	struct options_t *opt = NULL;
	/* Get the message part of the sended code */
	JsonNode *message = json_find_member(json, "message");
	/* Get the settings part of the sended code */
	JsonNode *settings = json_find_member(json, "settings");
	/* The return JSON object will all updated devices */
	JsonNode *rroot = json_mkobject();
	JsonNode *rdev = json_mkobject();
	JsonNode *rval = json_mkobject();
	
	/* Temporarily char pointer */
	char *stmp = NULL;
	/* Temporarily char array */
	char ctmp[255];
	/* Temporarily int */
	int itmp;
	/* Do we need to update the config file */
	unsigned short update = 0;
	/* The new state value */
	char state[255];
	/* The UUID of this device */
	char *uuid = NULL;
	json_find_string(json, "uuid", &uuid);

	/* Make sure the character poinrter are empty */
	memset(state, '\0', sizeof(state));
	memset(ctmp, '\0', sizeof(ctmp));

	/* Check if the found settings matches the send code */
	int match = 0;
	int match1 = 0;
	int match2 = 0;
	
	/* Was this device added to the return struct */
	int have_device = 0;
	
	/* Is is a valid new state / value */
	int is_valid = 1;

	/* Retrieve the used protocol */
	struct protocols_t *pnode = protocols;
	while(pnode) {
		protocol = pnode->listener;
		if(strcmp(protocol->id, protoname) == 0) {
			break;
		}
		pnode = pnode->next;
	}

	/* Only loop through all locations if the protocol has options */
	if((opt = protocol->options)) {

		/* Loop through all locations */
		while(lptr) {
			dptr = lptr->devices;
			/* Loop through all devices of this location */
			have_device = 0;

			JsonNode *rloc = NULL;
			while(dptr) {
				if((uuid && dptr->dev_uuid && strcmp(dptr->dev_uuid, uuid) == 0) || !uuid) {
					struct protocols_t *tmp_protocols = dptr->protocols;
					match = 0;
					while(tmp_protocols) {
						if(protocol_device_exists(protocol, tmp_protocols->name) == 0) {
							match = 1;
							break;
						}
						tmp_protocols = tmp_protocols->next;
					}

					if(match) {
						sptr = dptr->settings;
						/* Loop through all settings */
						while(sptr) {
							match1 = 0; match2 = 0;
							
							/* Check how many id's we need to match */
							opt = protocol->options;
							while(opt) {
								if(opt->conftype == CONFIG_ID) {
									JsonNode *jtmp = json_first_child(message);
									while(jtmp) {
										if(strcmp(jtmp->key, opt->name) == 0) {
											match1++;
										}
										jtmp = jtmp->next;
									}
								}
								opt = opt->next;
							}

							/* Loop through all protocol options */
							opt = protocol->options;
							while(opt) {
								if(opt->conftype == CONFIG_ID && strcmp(sptr->name, "id") == 0) {
									/* Check the config id's to match a device */								
									vptr = sptr->values;
									while(vptr) {
										if(strcmp(vptr->name, opt->name) == 0) {
											if(json_find_string(message, opt->name, &stmp) == 0) {
												strcpy(ctmp, stmp);
											}
											if(json_find_number(message, opt->name, &itmp) == 0) {
												sprintf(ctmp, "%d", itmp);
											}
											if(strcmp(ctmp, vptr->value) == 0) {
												match2++;
											}
										}
										vptr = vptr->next;
									}
								}
								/* Retrieve the new device state */
								if(opt->conftype == CONFIG_STATE && strlen(state) == 0) {
									if(opt->argtype == OPTION_NO_VALUE) {
										if(json_find_string(message, "state", &stmp) == 0) {
											strcpy(state, stmp);
										}
										if(json_find_number(message, "state", &itmp) == 0) {
											sprintf(state, "%d", itmp);
										}
									} else if(opt->argtype == OPTION_HAS_VALUE) {
										if(json_find_string(message, opt->name, &stmp) == 0) {
											strcpy(state, stmp);
										}
										if(json_find_number(message, opt->name, &itmp) == 0) {
											sprintf(state, "%d", itmp);
										}
									}
								}
								opt = opt->next;
							}
							if(match1 > 0 && match2 > 0 && match1 == match2) {
								break;
							}
							sptr = sptr->next;
						}
						is_valid = 1;
						/* If we matched a config device, update it's state */
						if(match1 > 0 && match2 > 0 && match1 == match2) {
							sptr = dptr->settings;
							while(sptr) {
								opt = protocol->options;
								/* Loop through all protocol options */
								while(opt) {
									/* Check if there are values that can be updated */
									if(strcmp(sptr->name, opt->name) == 0 
									   && (opt->conftype == CONFIG_VALUE)
									   && opt->argtype == OPTION_HAS_VALUE) {

										memset(ctmp, '\0', sizeof(ctmp));
										int type = 0;
										if(json_find_string(message, opt->name, &stmp) == 0) {
											strcpy(ctmp, stmp);
										}
										if(json_find_number(message, opt->name, &itmp) == 0) {
											sprintf(ctmp, "%d", itmp);
											type = 1;
										}										

										/* Check if the protocol settings of this device are valid to 
										   make sure no errors occur in the config.json. */
										if(protocol->checkValues) {
											JsonNode *jcode = json_mkobject();
											JsonNode *jsettings = json_first_child(settings);
											while(jsettings) {
												if(jsettings->tag == JSON_NUMBER) {
													json_append_member(jcode, jsettings->key, json_mknumber(jsettings->number_));
												} else if(jsettings->tag == JSON_STRING) {
													json_append_member(jcode, jsettings->key, json_mkstring(jsettings->string_));
												}
												jsettings = jsettings->next;
											}

											if(type == 0) { 
												json_append_member(jcode, opt->name, json_mkstring(ctmp));
											} else {
												json_append_member(jcode, opt->name, json_mknumber(itmp));
											}

											if(protocol->checkValues(jcode) != 0) {
												is_valid = 0;
												json_delete(jcode);
												break;
											} else {
												json_delete(jcode);
											}
										}

										if(strlen(ctmp) > 0 && is_valid) {
											if(strcmp(sptr->values->value, ctmp) != 0) {
												sptr->values->value = realloc(sptr->values->value, strlen(ctmp)+1);
												if(!sptr->values->value) {
													logprintf(LOG_ERR, "out of memory");
													exit(EXIT_FAILURE);
												}
												strcpy(sptr->values->value, ctmp);
											}

											if(json_find_string(rval, sptr->name, &stmp) != 0) {
												if(sptr->values->type == CONFIG_TYPE_STRING) {
													json_append_member(rval, sptr->name, json_mkstring(sptr->values->value));
												} else if(sptr->values->type == CONFIG_TYPE_NUMBER) {
													json_append_member(rval, sptr->name, json_mknumber(atoi(sptr->values->value)));
												}
												update = 1;
											}

											if(have_device == 0) {
												if(rloc == NULL) {
													rloc = json_mkarray();
												}

												json_append_element(rloc, json_mkstring(dptr->id));
												have_device = 1;
											}
										}
										//break;
									}
									opt = opt->next;
								}

								/* Check if we need to update the state */
								if(strcmp(sptr->name, "state") == 0) {
									if(strcmp(sptr->values->value, state) != 0) {
										sptr->values->value = realloc(sptr->values->value, strlen(state)+1);
										if(!sptr->values->value) {
											logprintf(LOG_ERR, "out of memory");
											exit(EXIT_FAILURE);
										}
										strcpy(sptr->values->value, state);
										update = 1;
									}
									
									if(json_find_string(rval, sptr->name, &stmp) != 0) {
										if(sptr->values->type == CONFIG_TYPE_STRING) {
											json_append_member(rval, sptr->name, json_mkstring(sptr->values->value));
										} else if(sptr->values->type == CONFIG_TYPE_NUMBER) {
											json_append_member(rval, sptr->name, json_mknumber(atoi(sptr->values->value)));
										}
									}
									if(rloc == NULL) {
										rloc = json_mkarray();
									}
									json_append_element(rloc, json_mkstring(dptr->id));
									have_device = 1;
									//break;
								}
								sptr = sptr->next;
							}
						}
					}
				}
				dptr = dptr->next;
			}
			if(have_device == 1) {
				json_append_member(rdev, lptr->id, rloc);
			}
			lptr = lptr->next;
		}
	}
	if(update == 1) {	
		json_append_member(rroot, "origin", json_mkstring("config"));
		json_append_member(rroot, "type",  json_mknumber((int)protocol->devtype));
		if(strlen(pilight_uuid) > 0 && (protocol->hwtype == SENSOR || protocol->hwtype == HWRELAY)) {
			json_append_member(rroot, "uuid",  json_mkstring(pilight_uuid));
		}
		json_append_member(rroot, "devices", rdev);
		json_append_member(rroot, "values", rval);

		*out = rroot;
	} else {
		json_delete(rroot);
	}

	return (update == 1) ? 0 : -1;
}

int config_get_location(char *id, struct conf_locations_t **loc) {
	struct conf_locations_t *lptr = conf_locations;
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

int config_valid_state(char *lid, char *sid, char *state) {
	struct conf_devices_t *dptr = NULL;
	struct protocol_t *protocol = NULL;
	struct options_t *options = NULL;
	struct protocols_t *tmp_protocol = NULL;
	
	if(config_get_device(lid, sid, &dptr) == 0) {
		tmp_protocol = dptr->protocols;
		while(tmp_protocol) {
			protocol = tmp_protocol->listener;
			if(protocol->options) {
				options = protocol->options;
				while(options) {
					if(strcmp(options->name, state) == 0 && options->conftype == CONFIG_STATE) {
						return 0;
						break;
					}
					options = options->next;
				}
			}
			tmp_protocol = tmp_protocol->next;
		}
	}
	return 1;
}

int config_valid_value(char *lid, char *sid, char *name, char *value) {
	struct conf_devices_t *dptr = NULL;
	struct options_t *opt = NULL;
	struct protocols_t *tmp_protocol = NULL;
#ifndef __FreeBSD__	
	regex_t regex;
	int reti;
#endif
	
	if(config_get_device(lid, sid, &dptr) == 0) {
		tmp_protocol = dptr->protocols;
		while(tmp_protocol) {
			opt = tmp_protocol->listener->options;
			while(opt) {
				if(opt->conftype == CONFIG_VALUE && strcmp(name, opt->name) == 0) {
#ifndef __FreeBSD__				
					reti = regcomp(&regex, opt->mask, REG_EXTENDED);
					if(reti) {
						logprintf(LOG_ERR, "could not compile regex");
						exit(EXIT_FAILURE);
					}
					reti = regexec(&regex, value, 0, NULL, 0);
					if(reti == REG_NOMATCH || reti != 0) {
						regfree(&regex);
						return 1;
					}
					regfree(&regex);
#endif
					return 0;
				}
				opt = opt->next;
			}
			tmp_protocol = tmp_protocol->next;
		}
	}
	return 1;
}

JsonNode *config2json(short internal) {
	/* Temporary pointer to the different structure */
	struct conf_locations_t *tmp_locations = NULL;
	struct conf_devices_t *tmp_devices = NULL;
	struct conf_settings_t *tmp_settings = NULL;
	struct conf_values_t *tmp_values = NULL;

	/* Pointers to the newly created JSON object */
	struct JsonNode *jroot = json_mkobject();
	struct JsonNode *jlocation = NULL;
	struct JsonNode *jdevice = NULL;
	struct JsonNode *joptions = NULL;
	struct JsonNode *jid = NULL;
	struct options_t *tmp_options = NULL;

	int lorder = 0;
	int dorder = 0;

	/* Make sure we preserve the order of the original file */
	tmp_locations = conf_locations;

	/* Show the parsed config file */
	while(tmp_locations) {
		lorder++;
		jlocation = json_mkobject();
		json_append_member(jlocation, "name", json_mkstring(tmp_locations->name));
		if(internal > 0) {
			json_append_member(jlocation, "order", json_mknumber(lorder));
		}

		dorder = 0;
		tmp_devices = tmp_locations->devices;

		while(tmp_devices) {
			dorder++;
			jdevice = json_mkobject();
			json_append_member(jdevice, "name", json_mkstring(tmp_devices->name));

			if((strlen(pilight_uuid) > 0 && ((strcmp(tmp_devices->ori_uuid, pilight_uuid) == 0) || 
			   (tmp_devices->dev_uuid && strcmp(tmp_devices->dev_uuid, pilight_uuid) == 0)))
			   || internal > 0) {

				if(internal > 0) {
					json_append_member(jdevice, "order", json_mknumber(dorder));
				}

				struct protocols_t *tmp_protocols = tmp_devices->protocols;
				struct JsonNode *jprotocols = json_mkarray();

				if(internal > 0) {
					json_append_member(jdevice, "type", json_mknumber(tmp_protocols->listener->devtype));
				}
				if(internal > 0 || (strlen(pilight_uuid) > 0 && 
					(strcmp(tmp_devices->ori_uuid, pilight_uuid) == 0) &&
					(strcmp(tmp_devices->dev_uuid, pilight_uuid) != 0)) 
					|| tmp_devices->cst_uuid == 1) {
					json_append_member(jdevice, "uuid", json_mkstring(tmp_devices->dev_uuid));
				}
				if(internal > 0) {
					json_append_member(jdevice, "origin", json_mkstring(tmp_devices->ori_uuid));
				}
				while(tmp_protocols) {
					json_append_element(jprotocols, json_mkstring(tmp_protocols->name));
					tmp_protocols = tmp_protocols->next;
				}
				json_append_member(jdevice, "protocol", jprotocols);
				json_append_member(jdevice, "id", json_mkarray());

				tmp_settings = tmp_devices->settings;
				while(tmp_settings) {
					tmp_values = tmp_settings->values;
					if(strcmp(tmp_settings->name, "id") == 0) {
						jid = json_find_member(jdevice, "id");
						JsonNode *jnid = json_mkobject();
						while(tmp_values) {
							if(tmp_values->type == CONFIG_TYPE_NUMBER) {
								json_append_member(jnid, tmp_values->name, json_mknumber(atoi(tmp_values->value)));
							} else if(tmp_values->type == CONFIG_TYPE_STRING) {
								json_append_member(jnid, tmp_values->name, json_mkstring(tmp_values->value));
							}
							tmp_values = tmp_values->next;
						}
						json_append_element(jid, jnid);
					} else if(!tmp_values->next) {
						if(tmp_values->type == CONFIG_TYPE_NUMBER) {
							json_append_member(jdevice, tmp_settings->name, json_mknumber(atoi(tmp_values->value)));
						} else if(tmp_values->type == CONFIG_TYPE_STRING) {
							json_append_member(jdevice, tmp_settings->name, json_mkstring(tmp_values->value));
						}
					} else {
						joptions = json_mkarray();
						while(tmp_values) {
							if(tmp_values->type == CONFIG_TYPE_NUMBER) {
								json_append_element(joptions, json_mknumber(atoi(tmp_values->value)));
							} else if(tmp_values->type == CONFIG_TYPE_STRING) {
								json_append_element(joptions, json_mkstring(tmp_values->value));
							}
							tmp_values = tmp_values->next;
						}
						json_append_member(jdevice, tmp_settings->name, joptions);
					}
					tmp_settings = tmp_settings->next;
				}

				tmp_protocols = tmp_devices->protocols;
				while(tmp_protocols) {
					tmp_options = tmp_protocols->listener->options;
					if(tmp_options) {
						while(tmp_options) {
							if(internal > 0 && tmp_options->conftype == CONFIG_SETTING && json_find_member(jdevice, tmp_options->name) == NULL) {
								if(tmp_options->vartype == JSON_NUMBER) {
									json_append_member(jdevice, tmp_options->name, json_mknumber((int)tmp_options->def));
								} else if(tmp_options->vartype == JSON_STRING) {
									json_append_member(jdevice, tmp_options->name, json_mkstring((char *)tmp_options->def));
								}
							}
							tmp_options = tmp_options->next;
						}
					}
					tmp_protocols = tmp_protocols->next;
				}
				json_append_member(jlocation, tmp_devices->id, jdevice);
			}
			tmp_devices = tmp_devices->next;
		}
		json_append_member(jroot, tmp_locations->id, jlocation);

		tmp_locations = tmp_locations->next;
	}

	return jroot;
}

JsonNode *config_broadcast_create(void) {
	struct JsonNode *jsend = json_mkobject();
	struct JsonNode *joutput = config2json(1);
	json_append_member(jsend, "config", joutput);
	
#ifdef UPDATE
	struct JsonNode *jversion = json_mkarray();
	json_append_element(jversion, json_mkstring(VERSION));
	if(update_latests_version()) {
		json_append_element(jversion, json_mkstring(update_latests_version()));
	} else {
		json_append_element(jversion, json_mkstring(VERSION));
	}
	json_append_element(jversion, json_mkstring(HASH));
	json_append_member(jsend, "version", jversion);	
#endif
	struct JsonNode *jfirmware = json_mkobject();
	json_append_member(jfirmware, "version", json_mknumber(firmware.version));	
	json_append_member(jfirmware, "hpf", json_mknumber(firmware.hpf));	
	json_append_member(jfirmware, "lpf", json_mknumber(firmware.lpf));	
	json_append_member(jsend, "firmware", jfirmware);

	return jsend;
}

void config_print(void) {
	logprintf(LOG_DEBUG, "-- start parsed config file --");
	JsonNode *joutput = config2json(2);
	char *output = json_stringify(joutput, "\t");
	printf("%s\n", output);
	json_delete(joutput);
	sfree((void *)&output);
	joutput = NULL;
	logprintf(LOG_DEBUG, "-- end parsed config file --");
}

/* Save the device settings to the device struct */
void config_save_setting(int i, JsonNode *jsetting, struct conf_devices_t *device) {
	/* Struct to store the values */
	struct conf_values_t *vnode = NULL;
	struct conf_settings_t *snode = NULL;
	struct conf_settings_t *tmp_settings = NULL;
	struct conf_values_t *tmp_values = NULL;
	/* Temporary JSON pointer */
	struct JsonNode *jtmp;

	/* Variable holder for casting settings */
	char *stmp = NULL;
	char ctmp[256];
	int itmp = 0;

	/* If the JSON tag is an array, then it should be a values or id array */
	if(jsetting->tag == JSON_ARRAY) {
		if(strcmp(jsetting->key, "id") == 0) {			
			/* Loop through the values of this values array */
			jtmp = json_first_child(jsetting);
			while(jtmp) {
				snode = malloc(sizeof(struct conf_settings_t));
				if(!snode) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				snode->name = malloc(strlen(jsetting->key)+1);
				if(!snode->name) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				strcpy(snode->name, jsetting->key);			
				snode->values = NULL;
				snode->next = NULL;
				if(jtmp->tag == JSON_OBJECT) {
					JsonNode *jtmp1 = json_first_child(jtmp);	
					while(jtmp1) {
						vnode = malloc(sizeof(struct conf_values_t));
						if(!vnode) {
							logprintf(LOG_ERR, "out of memory");
							exit(EXIT_FAILURE);
						}
						vnode->name = malloc(strlen(jtmp1->key)+1);
						if(!vnode->name) {
							logprintf(LOG_ERR, "out of memory");
							exit(EXIT_FAILURE);
						}
						strcpy(vnode->name, jtmp1->key);
						vnode->next = NULL;
						if(jtmp1->tag == JSON_STRING) {
							vnode->value = malloc(strlen(jtmp1->string_)+1);
							if(!vnode->value) {
								logprintf(LOG_ERR, "out of memory");
								exit(EXIT_FAILURE);
							}
							strcpy(vnode->value, jtmp1->string_);
							vnode->type = CONFIG_TYPE_STRING;
						} else if(jtmp1->tag == JSON_NUMBER) {
							sprintf(ctmp, "%d", (int)jtmp1->number_);
							vnode->value = malloc(strlen(ctmp)+1);
							if(!vnode->value) {
								logprintf(LOG_ERR, "out of memory");
								exit(EXIT_FAILURE);
							}
							strcpy(vnode->value, ctmp);
							vnode->type = CONFIG_TYPE_NUMBER;	
						}
						if(jtmp1->tag == JSON_NUMBER || jtmp1->tag == JSON_STRING) {
							tmp_values = snode->values;
							if(tmp_values) {
								while(tmp_values->next != NULL) {
									tmp_values = tmp_values->next;
								}
								tmp_values->next = vnode;
							} else {
								vnode->next = snode->values;
								snode->values = vnode;
							}			
						}
						jtmp1 = jtmp1->next;	
					}
					json_delete(jtmp1);			
				}
				jtmp = jtmp->next;
				
				tmp_settings = device->settings;
				if(tmp_settings) {
					while(tmp_settings->next != NULL) {
						tmp_settings = tmp_settings->next;
					}
					tmp_settings->next = snode;
				} else {
					snode->next = device->settings;
					device->settings = snode;
				}
			}
		}
	} else if(jsetting->tag == JSON_OBJECT) {
		snode = malloc(sizeof(struct conf_settings_t));
		if(!snode) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		snode->name = malloc(strlen(jsetting->key)+1);
		if(!snode->name) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		strcpy(snode->name, jsetting->key);
		snode->values = NULL;
		snode->next = NULL;
 
		jtmp = json_first_child(jsetting);
		while(jtmp) {
			if(jtmp->tag == JSON_STRING) {		
				vnode = malloc(sizeof(struct conf_values_t));
				if(!vnode) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				vnode->name = malloc(strlen(jtmp->key)+1);
				if(!vnode->name) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				strcpy(vnode->name, jtmp->key);			
				vnode->value = malloc(strlen(jtmp->string_)+1);
				if(!vnode->value) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				strcpy(vnode->value, jtmp->string_);
				vnode->type = CONFIG_TYPE_STRING;
				vnode->next = NULL;
			} else if(jtmp->tag == JSON_NUMBER) {
				vnode = malloc(sizeof(struct conf_values_t));			
				if(!vnode) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				vnode->name = malloc(strlen(jtmp->key)+1);
				if(!vnode->name) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				strcpy(vnode->name, jtmp->key);			
				sprintf(ctmp, "%d", (int)jtmp->number_);
				vnode->value = malloc(strlen(ctmp)+1);
				if(!vnode->value) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				strcpy(vnode->value, ctmp);
				vnode->type = CONFIG_TYPE_NUMBER;
				vnode->next = NULL;
			}
			if(jtmp->tag == JSON_NUMBER || jtmp->tag == JSON_STRING) {
				tmp_values = snode->values;
				if(tmp_values) {
					while(tmp_values->next != NULL) {
						tmp_values = tmp_values->next;
					}
					tmp_values->next = vnode;
				} else {
					vnode->next = snode->values;
					snode->values = vnode;
				}			
			}
			jtmp = jtmp->next;
		}

		tmp_settings = device->settings;
		if(tmp_settings) {
			while(tmp_settings->next != NULL) {
				tmp_settings = tmp_settings->next;
			}
			tmp_settings->next = snode;
		} else {
			snode->next = device->settings;
			device->settings = snode;
		}
		
	} else {
		/* New device settings node */
		snode = malloc(sizeof(struct conf_settings_t));
		if(!snode) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		snode->name = malloc(strlen(jsetting->key)+1);
		if(!snode->name) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		strcpy(snode->name, jsetting->key);
		snode->values = NULL;
		snode->next = NULL;
	
		vnode = malloc(sizeof(struct conf_values_t));
		if(!vnode) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		int valid = 0;
		/* Cast and store the new value */
		if(jsetting->tag == JSON_STRING && json_find_string(jsetting->parent, jsetting->key, &stmp) == 0) {
			vnode->value = malloc(strlen(stmp)+1);
			if(!vnode->value) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			vnode->name = malloc(4);
			if(!vnode->name) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			strcpy(vnode->value, stmp);
			vnode->type = CONFIG_TYPE_STRING;
			valid = 1;
		} else if(jsetting->tag == JSON_NUMBER && json_find_number(jsetting->parent, jsetting->key, &itmp) == 0) {
			sprintf(ctmp, "%d", itmp);
			vnode->value = malloc(strlen(ctmp)+1);
			if(!vnode->value) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			vnode->name = malloc(4);
			if(!vnode->name) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			strcpy(vnode->value, ctmp);
			vnode->type = CONFIG_TYPE_NUMBER;
			valid = 1;
		}
		if(valid) {
			tmp_values = snode->values;
			if(tmp_values) {
				while(tmp_values->next != NULL) {
					tmp_values = tmp_values->next;
				}
				tmp_values->next = vnode;
			} else {
				vnode->next = snode->values;
				snode->values = vnode;
			}			
			jtmp = jtmp->next;
		}

		tmp_settings = device->settings;
		if(tmp_settings) {
			while(tmp_settings->next != NULL) {
				tmp_settings = tmp_settings->next;
			}
			tmp_settings->next = snode;
		} else {
			snode->next = device->settings;
			device->settings = snode;
		}
	}
}

int config_check_id(int i, JsonNode *jsetting, struct conf_devices_t *device) {
	/* Temporary options pointer */
	struct options_t *tmp_options = NULL;
	/* Temporary ID array pointer */
	struct JsonNode *jid = NULL;
	/* Temporary ID values pointer */	
	struct JsonNode *jvalues = NULL;
	/* Temporary protocols pointer */
	struct protocols_t *tmp_protocols = NULL;

	int match1 = 0, match2 = 0, match3 = 0, has_id = 0;
	int valid_values = 0, nrprotocols = 0, nrids1 = 0, nrids2 = 0, have_error = 0;

	/* Variable holders for casting */
	char ctmp[256];

	tmp_protocols = device->protocols;
	while(tmp_protocols) {
		jid = json_first_child(jsetting);
		has_id = 0;
		match3 = 0;
		nrprotocols++;
		while(jid) {
			match2 = 0; match1 = 0; nrids1 = 0; nrids2 = 0;
			jvalues = json_first_child(jid);
			while(jvalues) {
				nrids1++;
				jvalues = jvalues->next;
			}
			if((tmp_options = tmp_protocols->listener->options)) {
				while(tmp_options) {
					if(tmp_options->conftype == CONFIG_ID) {
						nrids2++;
					}
					tmp_options = tmp_options->next;
				}
			}
			if(nrids1 == nrids2) {
				has_id = 1;
				jvalues = json_first_child(jid);
				while(jvalues) {
					match1++;
					if((tmp_options = tmp_protocols->listener->options)) {
						while(tmp_options) {
							if(tmp_options->conftype == CONFIG_ID && tmp_options->vartype == jvalues->tag) {
								if(strcmp(tmp_options->name, jvalues->key) == 0) {
									match2++;
									if(jvalues->tag == JSON_NUMBER) {
										sprintf(ctmp, "%d", (int)jvalues->number_);
									} else if(jvalues->tag == JSON_STRING) {
										strcpy(ctmp, jvalues->string_);
									}
									
									if(strlen(tmp_options->mask) > 0) {

										regex_t regex;
										int reti = regcomp(&regex, tmp_options->mask, REG_EXTENDED);
										if(reti) {
											logprintf(LOG_ERR, "could not compile regex");
										} else {
											reti = regexec(&regex, ctmp, 0, NULL, 0);
											if(reti == REG_NOMATCH || reti != 0) {
												match2--;
											}
											regfree(&regex);
										}
									}
								}
							}
							tmp_options = tmp_options->next;
						}
					}
					jvalues = jvalues->next;
				}
			}
			json_delete(jvalues);
			jid = jid->next;
			if(match2 > 0 && match1 == match2) {
				match3 = 1;
			}
		}
		if(!has_id) {
			valid_values--;
		} else if(match3) {
			valid_values++;
		} else {
			valid_values--;
		}
		json_delete(jid);
		tmp_protocols = tmp_protocols->next;
	}
	if(nrprotocols != valid_values) {
		logprintf(LOG_ERR, "setting #%d \"%s\" of \"%s\", invalid", i, "id", device->id);
		have_error = 1;
	}	
	return have_error;
}

int config_validate_settings(void) {
	/* Temporary pointer to the different structure */
	struct conf_locations_t *tmp_locations = NULL;
	struct conf_devices_t *tmp_devices = NULL;
	struct conf_settings_t *tmp_settings = NULL;
	struct conf_values_t *tmp_values = NULL;

	/* Pointers to the newly created JSON object */
	struct JsonNode *jdevice = NULL;
	struct JsonNode *joptions = NULL;

	int have_error = 0;
	int dorder = 0;
	/* Make sure we preserve the order of the original file */
	tmp_locations = conf_locations;

	/* Show the parsed config file */
	while(tmp_locations) {

		tmp_devices = tmp_locations->devices;
		dorder = 0;
		while(tmp_devices) {

			jdevice = json_mkobject();
			struct protocols_t *tmp_protocols = tmp_devices->protocols;
			while(tmp_protocols) {
				/* Only continue if protocol specific settings can be validated */
				if(tmp_protocols->listener->checkValues) {
					tmp_settings = tmp_devices->settings;
					dorder++;
					while(tmp_settings) {
						tmp_values = tmp_settings->values;
						/* Retrieve all protocol specific settings for this device. Also add all
						   device values and states so it can be validated by the protocol */
						if(strcmp(tmp_settings->name, "id") != 0) {
							if(!tmp_values->next) {
								if(tmp_values->type == CONFIG_TYPE_STRING) {
									json_append_member(jdevice, tmp_settings->name, json_mkstring(tmp_values->value));
								} else if(tmp_values->type == CONFIG_TYPE_NUMBER) {
									json_append_member(jdevice, tmp_settings->name, json_mknumber(atoi(tmp_values->value)));
								}
							} else {
								joptions = json_mkarray();
								while(tmp_values) {
									if(tmp_values->type == CONFIG_TYPE_STRING) {
										json_append_element(joptions, json_mkstring(tmp_values->value));
									} else if(tmp_values->type == CONFIG_TYPE_NUMBER) {
										json_append_element(joptions, json_mknumber(atoi(tmp_values->value)));
									}
									tmp_values = tmp_values->next;
								}
								json_append_member(jdevice, tmp_settings->name, joptions);
							}
						}
						tmp_settings = tmp_settings->next;
					}

					/* Let the settings and values be validated against each other */
					if(tmp_protocols->listener->checkValues(jdevice) != 0) {
						logprintf(LOG_ERR, "device #%d \"%s\" of \"%s\", invalid", dorder, tmp_devices->name, tmp_locations->name);
						have_error = 1;
						goto clear;
					}
				}
				tmp_protocols = tmp_protocols->next;
			}
			tmp_devices = tmp_devices->next;
			json_delete(jdevice);
			jdevice=NULL;
		}
		tmp_locations = tmp_locations->next;
	}
	
clear:
	if(jdevice) {
		json_delete(jdevice);
	}
	return have_error;
}

int config_check_state(int i, JsonNode *jsetting, struct conf_devices_t *device) {
	/* Temporary options pointer */
	struct options_t *tmp_options;

	int valid_state = 0, have_error = 0;

	/* Variable holders for casting */
	int itmp = 0;
	char ctmp[256];
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

	struct protocols_t *tmp_protocols = device->protocols;
	while(tmp_protocols) {
		/* Check if the specific device contains any options */
		if(tmp_protocols->listener->options) {

			tmp_options = tmp_protocols->listener->options;

			while(tmp_options) {
				/* We are only interested in the CONFIG_STATE options */
				if(tmp_options->conftype == CONFIG_STATE && tmp_options->vartype == jsetting->tag) {
					/* If an option requires an argument, then check if the
					   argument state values and values array are of the right
					   type. This is done by checking the regex mask */
					if(tmp_options->argtype == OPTION_HAS_VALUE) {
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
								regfree(&regex);
								goto clear;
							}
							regfree(&regex);
						}
					} else {
						/* If a protocol has CONFIG_STATE arguments, than these define
						   the states a protocol can take. Check if the state value
						   match, these protocol states */
						
						if(strcmp(tmp_options->name, ctmp) == 0 && valid_state == 0) {
							valid_state = 1;
						}
					}
				}
				tmp_options = tmp_options->next;
			}
		}
		tmp_protocols = tmp_protocols->next;
	}

	if(valid_state == 0) {
		logprintf(LOG_ERR, "setting #%d \"%s\" of \"%s\", invalid", i, jsetting->key, device->id);
		have_error = 1;
		goto clear;
	}

clear:
	return have_error;
}

int config_parse_devices(JsonNode *jdevices, struct conf_devices_t *device) {
	/* Temporary settings holder */
	struct conf_settings_t *tmp_settings = NULL;
	/* JSON devices iterator */
	JsonNode *jsettings = NULL;
	/* Temporarily options pointer */
	struct options_t *tmp_options = NULL;
	/* Temporarily protocols pointer */
	struct protocols_t *tmp_protocols = NULL;

	int i = 0, have_error = 0, valid_setting = 0;
	int	match = 0, has_state = 0;
	/* Check for any duplicate fields */
	int nrname = 0, nrprotocol = 0, nrstate = 0, nrorder = 0;
	int nrsettings = 0, nruuid = 0, nrorigin = 0;

	jsettings = json_first_child(jdevices);
	while(jsettings) {
		i++;
		/* Check for any duplicate fields */
		if(strcmp(jsettings->key, "name") == 0) {
			nrname++;
		}
		if(strcmp(jsettings->key, "uuid") == 0) {
			nruuid++;
		}		
		if(strcmp(jsettings->key, "origin") == 0) {
			nrorigin++;
		}		
		if(strcmp(jsettings->key, "order") == 0) {
			nrorder++;
		}		
		if(strcmp(jsettings->key, "protocol") == 0) {
			nrprotocol++;
		}
		if(strcmp(jsettings->key, "state") == 0) {
			nrstate++;
		}
		if(strcmp(jsettings->key, "settings") == 0) {
			nrsettings++;
		}
		if(nrstate > 1 || nrprotocol > 1 || nrname > 1 || nrorder > 1 
		   || nrsettings > 1 || nruuid > 1) {
			logprintf(LOG_ERR, "settting #%d \"%s\" of \"%s\", duplicate", i, jsettings->key, device->id);
			have_error = 1;
			goto clear;
		}
		
		/* Parse the state setting separately from the other settings. */
		if(strcmp(jsettings->key, "state") == 0 && (jsettings->tag == JSON_STRING || jsettings->tag == JSON_NUMBER)) {
			if(config_check_state(i, jsettings, device) != 0) {
				have_error = 1;
				goto clear;
			}
			config_save_setting(i, jsettings, device);
		} else if(strcmp(jsettings->key, "id") == 0 && jsettings->tag == JSON_ARRAY) {
			if(config_check_id(i, jsettings, device) == EXIT_FAILURE) {
				have_error = 1;
				goto clear;
			}	
			config_save_setting(i, jsettings, device);
		} else if(strcmp(jsettings->key, "uuid") == 0 && jsettings->tag == JSON_STRING) {
			strcpy(device->dev_uuid, jsettings->string_);
			device->cst_uuid = 1;
		} else if(strcmp(jsettings->key, "origin") == 0 && jsettings->tag == JSON_STRING) {
			strcpy(device->ori_uuid, jsettings->string_);
		/* The protocol and name settings are already saved in the device struct */				
		}  else if(!((strcmp(jsettings->key, "name") == 0 && jsettings->tag == JSON_STRING)
			|| (strcmp(jsettings->key, "protocol") == 0 && jsettings->tag == JSON_ARRAY)
			|| (strcmp(jsettings->key, "type") == 0 && jsettings->tag == JSON_NUMBER)
			|| (strcmp(jsettings->key, "order") == 0 && jsettings->tag == JSON_NUMBER)
			|| (strcmp(jsettings->key, "uuid") == 0 && jsettings->tag == JSON_STRING))) {

			/* Check for duplicate settings */
			tmp_settings = device->settings;
			while(tmp_settings) {
				if(strcmp(tmp_settings->name, jsettings->key) == 0) {
					logprintf(LOG_ERR, "setting #%d \"%s\" of \"%s\", duplicate", i, jsettings->key, device->id);
					have_error = 1;
					goto clear;
				}
				tmp_settings = tmp_settings->next;
			}

			tmp_protocols = device->protocols;
			valid_setting = 0;			
			while(tmp_protocols) {
				/* Check if the settings are required by the protocol */			
				if(tmp_protocols->listener->options) {
					tmp_options = tmp_protocols->listener->options;
					while(tmp_options) {
						if(strcmp(jsettings->key, tmp_options->name) == 0 
						   && (tmp_options->conftype == CONFIG_ID 
						       || tmp_options->conftype == CONFIG_VALUE
							   || tmp_options->conftype == CONFIG_SETTING)
						   && (tmp_options->vartype == jsettings->tag)) {
							valid_setting = 1;
							break;
						}
						tmp_options = tmp_options->next;
					}
				}
				tmp_protocols = tmp_protocols->next;
			}
			/* If the settings are valid, store them */
			if(valid_setting == 1) {
				config_save_setting(i, jsettings, device);
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
	tmp_protocols = device->protocols;
	while(tmp_protocols) {
		if(tmp_protocols->listener->options) {
			tmp_options = tmp_protocols->listener->options;
			while(tmp_options) {
				match = 0;
				jsettings = json_first_child(jdevices);
				while(jsettings) {
					if(strcmp(jsettings->key, tmp_options->name) == 0) {
						match = 1;
						break;
					}
					jsettings = jsettings->next;
				}
				if(match == 0 && tmp_options->conftype == CONFIG_VALUE) {
					logprintf(LOG_ERR, "setting \"%s\" of \"%s\", missing", tmp_options->name, device->id);
					have_error = 1;
					goto clear;
				}
			tmp_options = tmp_options->next;
			}
			
		}
		tmp_protocols = tmp_protocols->next;
	}
	match = 0;
	jsettings = json_first_child(jdevices);
	while(jsettings) {
		if(strcmp(jsettings->key, "id") == 0) {
			match = 1;
			break;
		}
		jsettings = jsettings->next;
	}
	if(match == 0) {
		logprintf(LOG_ERR, "setting \"id\" of \"%s\", missing", device->id);
		have_error = 1;
		goto clear;
	}

	/* Check if this protocol requires a state setting */
	if(nrstate == 0) {
		tmp_protocols = device->protocols;
		while(tmp_protocols) {
			if(tmp_protocols->listener->options) {
				tmp_options = tmp_protocols->listener->options;
				while(tmp_options) {
					if(tmp_options->conftype == CONFIG_STATE) {
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
			tmp_protocols = tmp_protocols->next;
		}
	}

	if(strlen(pilight_uuid) > 0 && strcmp(device->dev_uuid, pilight_uuid) == 0) {
		tmp_protocols = device->protocols;
		while(tmp_protocols) {
			if(tmp_protocols->listener->initDev) {
				tmp_protocols->listener->initDev(jdevices);
			}
			tmp_protocols = tmp_protocols->next;
		}
	}

clear:
	return have_error;
}

int config_parse_locations(JsonNode *jlocations, struct conf_locations_t *location) {
	/* Struct to store the devices */
	struct conf_devices_t *dnode = NULL;
	/* Temporary JSON devices  */
	struct conf_devices_t *tmp_devices = NULL;
	/* Temporary protocol JSON array */
	JsonNode *jprotocol = NULL;
	JsonNode *jprotocols = NULL;
	/* JSON devices iterator */
	JsonNode *jdevices = NULL;
	/* Device name */
	char *name = NULL;

	int i = 0, have_error = 0, match = 0, x = 0;
	/* Check for duplicate name setting */
	int nrname = 0;
	/* Check for duplicate order setting */
	int nrorder = 0;

	jdevices = json_first_child(jlocations);
	while(jdevices) {
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
		/* Check for duplicate name setting */
		if(strcmp(jdevices->key, "order") == 0) {
			nrorder++;
			if(nrorder > 1) {
				logprintf(LOG_ERR, "device #%d \"%s\" of \"%s\", duplicate", i, jdevices->key, location->id);
				have_error = 1;
				goto clear;
			}
		}
		/* Check if all fields of the devices are of the right type */
		if(!((strcmp(jdevices->key, "name") == 0 && jdevices->tag == JSON_STRING) || (strcmp(jdevices->key, "order") == 0 && jdevices->tag == JSON_NUMBER) || (jdevices->tag == JSON_OBJECT))) {
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
			} else if((jprotocols = json_find_member(jdevices, "protocol")) == NULL && jprotocols->tag == JSON_ARRAY) {
				logprintf(LOG_ERR, "device #%d \"%s\" of \"%s\", missing protocol", i, jdevices->key, location->id);
				have_error = 1;
				goto clear;
			} else {
				for(x=0;x<strlen(jdevices->key);x++) {
					if(!isalnum(jdevices->key[x])) {
						logprintf(LOG_ERR, "device #%d \"%s\" of \"%s\", not alphanumeric", i, jdevices->key, location->id);
						have_error = 1;
						goto clear;
					}
				}
				/* Check for duplicate fields */
				tmp_devices = location->devices;
				while(tmp_devices) {
					if(strcmp(tmp_devices->id, jdevices->key) == 0) {
						logprintf(LOG_ERR, "device #%d \"%s\" of \"%s\", duplicate", i, jdevices->key, location->id);
						have_error = 1;
					}
					tmp_devices = tmp_devices->next;
				}

				dnode = malloc(sizeof(struct conf_devices_t));
				if(!dnode) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				if(strlen(pilight_uuid) > 0) {				
					strcpy(dnode->dev_uuid, pilight_uuid);
					strcpy(dnode->ori_uuid, pilight_uuid);
				}
				dnode->cst_uuid = 0;
				dnode->id = malloc(strlen(jdevices->key)+1);
				if(!dnode->id) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				strcpy(dnode->id, jdevices->key);
				dnode->name = malloc(strlen(name)+1);
				if(!dnode->name) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				strcpy(dnode->name, name);
				dnode->settings = NULL;
				dnode->next = NULL;
				dnode->protocols = NULL;

				int ptype = -1;
				/* Save both the protocol pointer and the protocol name */
				jprotocol = json_first_child(jprotocols);
				while(jprotocol) {
					match = 0;
					struct protocols_t *tmp_protocols = protocols;
					/* Pointer to the match protocol */				
					protocol_t *protocol = NULL;	
					while(tmp_protocols) {
						protocol = tmp_protocols->listener;
						if(protocol_device_exists(protocol, jprotocol->string_) == 0 && match == 0) {
							if(ptype == -1) {
								ptype = protocol->hwtype;
								match = 1;
							}
							if(ptype > -1 && protocol->hwtype == ptype) {
								match = 1;
							}			
							match = 1;
							break;
						}
						tmp_protocols = tmp_protocols->next;
					}
					if(match == 1 && ptype != protocol->hwtype) {
						logprintf(LOG_ERR, "device #%d \"%s\" of \"%s\", cannot combine protocols of different hardware types", i, jdevices->key, location->id);
						have_error = 1;
					}		
					if(match == 0) {
						logprintf(LOG_ERR, "device #%d \"%s\" of \"%s\", invalid protocol", i, jdevices->key, location->id);
						have_error = 1;
					} else {
						struct protocols_t *pnode = malloc(sizeof(struct protocols_t));
						if(!pnode) {
							logprintf(LOG_ERR, "out of memory");
							exit(EXIT_FAILURE);
						}
						pnode->listener = malloc(sizeof(struct protocol_t));
						if(!pnode->listener) {
							logprintf(LOG_ERR, "out of memory");
							exit(EXIT_FAILURE);
						}
						memcpy(pnode->listener, protocol, sizeof(struct protocol_t));
						pnode->name = malloc(strlen(jprotocol->string_)+1);
						if(!pnode->name) {
							logprintf(LOG_ERR, "out of memory");
							exit(EXIT_FAILURE);
						}
						strcpy(pnode->name, jprotocol->string_);
						pnode->next = NULL;
						tmp_protocols = dnode->protocols;
						if(tmp_protocols) {
							while(tmp_protocols->next != NULL) {
								tmp_protocols = tmp_protocols->next;
							}
							tmp_protocols->next = pnode;
						} else {
							pnode->next = dnode->protocols;
							dnode->protocols = pnode;
						}
					}
					jprotocol = jprotocol->next;
				}
				json_delete(jprotocol);

				if(!have_error && config_parse_devices(jdevices, dnode) != 0) {
					have_error = 1;
				}

				tmp_devices = location->devices;
				if(tmp_devices) {
					while(tmp_devices->next != NULL) {
						tmp_devices = tmp_devices->next;
					}
					tmp_devices->next = dnode;
				} else {
					dnode->next = location->devices;
					location->devices = dnode;
				}

				if(have_error) {
					goto clear;
				}
			}
		}

		jdevices = jdevices->next;
	}

clear:
	return have_error;
}

int config_parse(JsonNode *root) {
	/* Struct to store the locations */
	struct conf_locations_t *lnode = NULL;
	struct conf_locations_t *tmp_locations = NULL;
	/* JSON locations iterator */
	JsonNode *jlocations = NULL;
	/* Location name */
	char *name;

	int i = 0, have_error = 0, x = 0;

	jlocations = json_first_child(root);

	while(jlocations) {
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
			for(x=0;x<strlen(jlocations->key);x++) {
				if(!isalnum(jlocations->key[x])) {
					logprintf(LOG_ERR, "location #%d \"%s\", not alphanumeric", i, jlocations->key);
					have_error = 1;
					goto clear;
				}
			}		
		
			/* Check for duplicate locations */
			tmp_locations = conf_locations;
			while(tmp_locations) {
				if(strcmp(tmp_locations->id, jlocations->key) == 0) {
					logprintf(LOG_ERR, "location #%d \"%s\", duplicate", i, jlocations->key);
					have_error = 1;
					goto clear;
				}
				tmp_locations = tmp_locations->next;
			}

			lnode = malloc(sizeof(struct conf_locations_t));
			if(!lnode) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			lnode->id = malloc(strlen(jlocations->key)+1);
			if(!lnode->id) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			strcpy(lnode->id, jlocations->key);
			lnode->name = malloc(strlen(name)+1);
			if(!lnode->name) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			strcpy(lnode->name, name);
			lnode->devices = NULL;
			lnode->next = NULL;
			
			if(config_parse_locations(jlocations, lnode) != 0) {
				have_error = 1;
			}			

			tmp_locations = conf_locations;
			if(tmp_locations) {
				while(tmp_locations->next != NULL) {
					tmp_locations = tmp_locations->next;
				}
				tmp_locations->next = lnode;
			} else {
				lnode->next = conf_locations;
				conf_locations = lnode;
			}

			if(have_error) {
				goto clear;
			}
			
			jlocations = jlocations->next;
		}
	}

clear:
	return have_error;
}

// int config_merge(char *config) {
	// /* Struct to store the locations */
	// struct conf_locations_t *lnode = NULL;
	// struct conf_locations_t *tmp_locations = NULL;
	// struct JsonNode *root = NULL;
	// /* JSON locations iterator */
	// JsonNode *jlocations = NULL;
	// /* Location name */
	// char *name = NULL;

	// int i = 0, have_error = 0, match = 0;

	// if(json_validate(config) == false) {
		// logprintf(LOG_ERR, "config is not in a valid json format");
		// have_error = 1;
		// goto clear;
	// }
	
	// root = json_decode(config);
	// jlocations = json_first_child(root);

	// while(jlocations) {
		// i++;
		// /* An location can only be a JSON object */
		// if(jlocations->tag != 5) {
			// logprintf(LOG_ERR, "location #%d \"%s\", invalid format", i, jlocations->key);
			// have_error = 1;
			// goto clear;
		// /* Check if the location has a name */
		// } else if(json_find_string(jlocations, "name", &name) != 0) {
			// logprintf(LOG_ERR, "location #%d \"%s\", missing name", i, jlocations->key);
			// have_error = 1;
			// goto clear;
		// } else {
			// match = 0;
			// /* Check for duplicate locations */
			// tmp_locations = conf_locations;
			// while(tmp_locations) {
				// if(strcmp(tmp_locations->id, jlocations->key) == 0) {
					// match = 1;
					// break;
				// }
				// tmp_locations = tmp_locations->next;
			// }

			// if(match == 0) {
				// lnode = malloc(sizeof(struct conf_locations_t));
				// lnode->id = malloc(strlen(jlocations->key)+1);
				// strcpy(lnode->id, jlocations->key);
				// lnode->name = malloc(strlen(name)+1);
				// strcpy(lnode->name, name);
				// lnode->devices = NULL;
				// lnode->next = NULL;
				
				// if(config_parse_locations(jlocations, lnode) != 0) {
					// have_error = 1;
					// goto clear;
				// }

				// tmp_locations = conf_locations;
				// if(tmp_locations) {
					// while(tmp_locations->next != NULL) {
						// tmp_locations = tmp_locations->next;
					// }
					// tmp_locations->next = lnode;
				// } else {
					// lnode->next = conf_locations;
					// conf_locations = lnode;
				// }
			// } else {
				// if(config_parse_locations(jlocations, tmp_locations) != 0) {
					// have_error = 1;
					// goto clear;
				// }
			// }
			
			// jlocations = jlocations->next;
		// }
	// }

	// json_delete(jlocations);
// clear:
	// return have_error;
// }

int config_write(char *content) {
	FILE *fp;

	if(access(configfile, F_OK) != -1) {
		/* Overwrite config file with proper format */
		if(!(fp = fopen(configfile, "w+"))) {
			logprintf(LOG_ERR, "cannot write config file: %s", configfile);
			return EXIT_FAILURE;
		}
		fseek(fp, 0L, SEEK_SET);
		fwrite(content, sizeof(char), strlen(content), fp);
		fclose(fp);
		if(strcmp(content, "{}") == 0) {
			return EXIT_SUCCESS;
		}		
	} else {
		logprintf(LOG_ERR, "the config file %s does not exists\n", configfile);
	}

	return EXIT_SUCCESS;
}

int config_gc(void) {
	sfree((void *)&configfile);

	struct conf_locations_t *ltmp;
	struct conf_devices_t *dtmp;
	struct conf_settings_t *stmp;
	struct conf_values_t *vtmp;
	struct protocols_t *ptmp;

	/* Free config structure */
	while(conf_locations) {
		ltmp = conf_locations;
		while(ltmp->devices) {
			dtmp = ltmp->devices;
			while(dtmp->settings) {
				stmp = dtmp->settings;
				while(stmp->values) {
					vtmp = stmp->values;
					sfree((void *)&vtmp->value);
					sfree((void *)&vtmp->name);
					stmp->values = stmp->values->next;
					sfree((void *)&vtmp);
				}
				sfree((void *)&stmp->values);
				sfree((void *)&stmp->name);
				dtmp->settings = dtmp->settings->next;
				sfree((void *)&stmp);
			}
			while(dtmp->protocols) {
				ptmp = dtmp->protocols;
				sfree((void *)&ptmp->name);
				sfree((void *)&ptmp->listener);
				dtmp->protocols = dtmp->protocols->next;
				sfree((void *)&ptmp);
			}
			sfree((void *)&dtmp->protocols);
			sfree((void *)&dtmp->settings);
			sfree((void *)&dtmp->id);
			sfree((void *)&dtmp->name);
			ltmp->devices = ltmp->devices->next;
			sfree((void *)&dtmp);
		}
		sfree((void *)&ltmp->devices);		
		sfree((void *)&ltmp->id);
		sfree((void *)&ltmp->name);
		conf_locations = conf_locations->next;
		sfree((void *)&ltmp);
	}
	sfree((void *)&conf_locations);
	
	logprintf(LOG_DEBUG, "garbage collected config library");
	
	return EXIT_SUCCESS;
}

int config_read() {
	char *content = NULL;
	JsonNode *root = NULL;
	FILE *fp;
	size_t bytes;
	struct stat st;

	/* Read JSON config file */
	if(!(fp = fopen(configfile, "rb"))) {
		logprintf(LOG_ERR, "cannot read config file: %s", configfile);
		return EXIT_FAILURE;
	}

	fstat(fileno(fp), &st);
	bytes = (size_t)st.st_size;

	if(!(content = calloc(bytes+1, sizeof(char)))) {
		logprintf(LOG_ERR, "out of memory");
		return EXIT_FAILURE;
	}

	if(fread(content, sizeof(char), bytes, fp) == -1) {
		logprintf(LOG_ERR, "cannot read config file: %s", configfile);
	}
	fclose(fp);

	/* Validate JSON and turn into JSON object */
	if(json_validate(content) == false) {
		logprintf(LOG_ERR, "config is not in a valid json format");
		sfree((void *)&content);
		return EXIT_FAILURE;
	}
	root = json_decode(content);

	sfree((void *)&content);

	if(config_parse(root) == 0 && config_validate_settings() == 0) {
		JsonNode *joutput = config2json(-1);
		char *output = json_stringify(joutput, "\t");
		config_write(output);
		json_delete(joutput);
		sfree((void *)&output);			
		joutput = NULL;
		json_delete(root);
		root = NULL;
		return EXIT_SUCCESS;
	} else {
		json_delete(root);
		root = NULL;
		return EXIT_FAILURE;
	}
}

int config_set_file(char *cfgfile) {
	if(access(cfgfile, F_OK) != -1) {
		configfile = realloc(configfile, strlen(cfgfile)+1);
		if(!configfile) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		strcpy(configfile, cfgfile);
	} else {
		logprintf(LOG_ERR, "the config file %s does not exists\n", cfgfile);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
