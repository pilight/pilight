/*
	Copyright (C) 2015 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#ifndef _WIN32
	#include <unistd.h>
	#include <regex.h>
	#include <sys/ioctl.h>
	#include <dlfcn.h>
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
	#include <pthread.h>
#endif
#include <sys/stat.h>
#include <time.h>
#include <math.h>
#include <wiringx.h>

#include "../hardware/hardware.h"
#include "../protocols/protocol.h"
#include "../core/common.h"
#include "../core/datetime.h"
#include "../core/json.h"
#include "../core/log.h"
#include "../events/events.h"
#include "../events/action.h"
#include "../datatypes/stack.h"
#include "storage.h"

#include "json.h"

static void *storage_import_thread(int, void *);

static void *reason_config_update_free(void *param) {
	struct reason_config_update_t *data = param;
	FREE(data);
	return NULL;
}

static void *reason_device_added_free(void *param) {
	json_delete(param);
	return NULL;
}

#ifdef EVENTS
static struct rules_t *rules = NULL;
#endif

struct device_t *device = NULL;
struct storage_t *storage = NULL;
static struct JsonNode *jdevices_cache = NULL;
#ifdef EVENTS
	static struct JsonNode *jrules_cache = NULL;
#endif
static struct JsonNode *jgui_cache = NULL;
static struct JsonNode *jsettings_cache = NULL;
static struct JsonNode *jhardware_cache = NULL;
static struct JsonNode *jregistry_cache = NULL;

static void *devices_update_cache(int reason, void *param) {
	struct reason_config_update_t *data = param;
	struct reason_config_updated_t *data1 = MALLOC(sizeof(struct reason_config_updated_t));

	struct JsonNode *jcdev_childs = json_first_child(jdevices_cache);
	struct JsonNode *jvalue = NULL;
	struct device_t *dev = NULL;
	int i = 0, x = 0;

	/*
	 * FIXME: clone reason_config_update_t
	 */
	if(data1 == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}	

	data1->type = data->type;
	data1->nrdev = data->nrdev;
	data1->timestamp = data->timestamp;
	if(data->uuid != NULL) {
		data1->uuid = data->uuid;
	}
	strcpy(data1->origin, data->origin);

	for(i=0;i<data->nrdev;i++) {
		strcpy(data1->devices[i], data->devices[i]);
		if(devices_select_struct(ORIGIN_CONFIG, data->devices[i], &dev) == 0) {
			dev->timestamp = (time_t)data->timestamp;
		}
		while(jcdev_childs) {
			if(strcmp(jcdev_childs->key, "timestamp") != 0 &&
			   strcmp(jcdev_childs->key, data->devices[i]) == 0) {
				data1->nrval = data->nrval;
				for(x=0;x<data->nrval;x++) {
					strcpy(data1->values[x].name, data->values[x].name);
					data1->values[x].type = data->values[x].type;
					if(data->values[x].type == JSON_NUMBER) {
						data1->values[x].number_ = data->values[x].number_;
						data1->values[x].decimals = data->values[x].decimals;
					} else if(data->values[x].type == JSON_STRING) {
						strcpy(data1->values[x].string_, data->values[x].string_);
					}
					if((jvalue = json_find_member(jcdev_childs, data->values[x].name)) != NULL) {
						if(data->values[x].type == JSON_NUMBER) {
							if(jvalue->tag == JSON_STRING) {
								json_free(jvalue->string_);
							}
							jvalue->number_ = data->values[x].number_;
							jvalue->decimals_ = data->values[x].decimals;
							jvalue->tag = JSON_NUMBER;
						}
						if(data->values[x].type == JSON_STRING) {
							if(jvalue->tag == JSON_NUMBER) {
								jvalue->string_ = NULL;
							}
							if((jvalue->string_ = REALLOC(jvalue->string_, strlen(data->values[x].string_)+1)) == NULL) {
								OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
							}
							strcpy(jvalue->string_, data->values[x].string_);
							jvalue->tag = JSON_STRING;
						}
					}
				}
			}
			jcdev_childs = jcdev_childs->next;
		}
	}

	eventpool_trigger(REASON_CONFIG_UPDATED, reason_config_update_free, data1);
	return NULL;
}

void *config_values_update(int reason, void *param) {
	char *protoname = NULL, *origin = NULL, *message = NULL, *uuid = NULL/*, *settings = NULL*/;

	switch(reason) {
		case REASON_CODE_RECEIVED: {
			struct reason_code_received_t *data = param;
			protoname = data->protocol;
			origin = data->origin;
			message = data->message;
			uuid = data->uuid;
		} break;
		case REASON_CODE_SENT: {
			struct reason_code_sent_t *data = param;
			if(strlen(data->protocol) > 0) {
				protoname = data->protocol;
			}
			message = data->message;
			if(strlen(data->uuid) > 0) {
				uuid = data->uuid;
			}
			/*if(strlen(data->settings) > 0) {
				settings = data->settings;
			}*/
			origin = data->origin;
		} break;
		default: {
			return NULL;
		} break;
	}

	if(origin != NULL && strcmp(origin, "core") == 0) {
		return NULL;
	}

	struct JsonNode *jmessage = json_decode(message);
	//struct JsonNode *jsettings = NULL;
	struct JsonNode *jdevices = json_first_child(jdevices_cache);
	struct JsonNode *jchilds = NULL;
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jids = NULL;
	struct JsonNode *jtmp = NULL;

	if((jtmp = json_first_child(jmessage)) != NULL && strcmp(jtmp->key, "message") == 0) {
		struct JsonNode *jclone = NULL;
		json_clone(jtmp, &jclone);
		json_delete(jmessage);
		jmessage = jclone;
	}

	/*if(settings != NULL) {
		jsettings = json_decode(settings);
	}*/

	struct protocol_t *protocol = NULL;
	struct options_t *opt = NULL;
	char *dev_uuid = NULL, *ori_uuid = NULL;
	char _dev_uuid[UUID_LENGTH+1], _ori_uuid[UUID_LENGTH+1], *stmp = NULL, *stmp1 = NULL;
	int custom_uuid = 0, match = 0, match1 = 0, match2 = 0, is_valid = 0;
	int update = 0, final_update = 0;
	double itmp = 0.0, itmp1 = 0.0;

	if(strlen(protoname) == 0) {
		logprintf(LOG_ERR, "config values update message misses a protocol field");
		json_delete(jmessage);
		/*if(jsettings != NULL) {
			json_delete(jsettings);
		}*/
		return NULL;
	}

	if(uuid == NULL || strlen(uuid) == 0) {
		logprintf(LOG_ERR, "config values update message misses a uuid field");
		json_delete(jmessage);
		/*if(jsettings != NULL) {
			json_delete(jsettings);
		}*/
		return NULL;
	}

	struct reason_config_update_t *data = MALLOC(sizeof(struct reason_config_update_t));
	if(data == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(data, 0, sizeof(struct reason_config_update_t));

	/* Retrieve the used protocol */
	struct protocols_t *pnode = protocols;
	while(pnode) {
		protocol = pnode->listener;
		if(strcmp(protocol->id, protoname) == 0) {
			break;
		}
		pnode = pnode->next;
	}

	time_t timenow = time(NULL);
	struct tm gmt;
	memset(&gmt, '\0', sizeof(struct tm));
#ifdef _WIN32
	struct tm *tm;
	tm = gmtime(&timenow);
	memcpy(&gmt, tm, sizeof(struct tm));
#else
	gmtime_r(&timenow, &gmt);
#endif
	time_t utct = datetime2ts(gmt.tm_year+1900, gmt.tm_mon+1, gmt.tm_mday, gmt.tm_hour, gmt.tm_min, gmt.tm_sec);
	data->timestamp = utct;

	if((opt = protocol->options)) {
		/* Loop through all devices */
		while(jdevices) {
			custom_uuid = 0;
			if(json_find_string(jdevices, "uuid", &dev_uuid) != 0) {
				if(strlen(pilight_uuid) > 0) {
					strcpy(_dev_uuid, pilight_uuid);
					dev_uuid = _dev_uuid;
				}
			} else {
				custom_uuid = 1;
			}
			if(json_find_string(jdevices, "origin", &ori_uuid) != 0) {
				if(strlen(pilight_uuid) > 0) {
					strcpy(_ori_uuid, pilight_uuid);
					ori_uuid = _ori_uuid;
				}
			}
			int uuidmatch = 0;
			if(uuid != NULL && dev_uuid != NULL && ori_uuid != NULL && strlen(pilight_uuid) > 0) {
				if(custom_uuid == 1) {
					/* If the user forced the device UUID and it matches the UUID of the received code */
					if(strcmp(dev_uuid, uuid) == 0) {
						uuidmatch = 1;
					}
				} else {
					uuidmatch = 1;
				}
			} else if(uuid == NULL || strlen(pilight_uuid) == 0) {
				uuidmatch = 1;
			}
			if(uuidmatch == 1) {
				/*
				 * Check if the received protocol is configured.
				 */
				jprotocols = json_find_member(jdevices, "protocol");
				jchilds = json_first_child(jprotocols);
				while(jchilds) {
					if(jchilds->tag == JSON_STRING) {
						if(protocol_device_exists(protocol, jchilds->string_) == 0) {
							match = 1;
							break;
						}
					}
					jchilds = jchilds->next;
				}
				if(match == 1) {
					match1 = 0; match2 = 0;
					opt = protocol->options;
					while(opt) {
						if(opt->conftype == DEVICES_ID) {
							struct JsonNode *jtmp = json_first_child(jmessage);
							while(jtmp) {
								if(strcmp(jtmp->key, opt->name) == 0) {
									match1++;
								}
								jtmp = jtmp->next;
							}
						}
						opt = opt->next;
					}
					jids = json_find_member(jdevices, "id");
					jchilds = json_first_child(jids);
					while(jchilds) {
						/* Loop through all protocol options */
						opt = protocol->options;
						while(opt) {
							if(opt->conftype == DEVICES_ID) {
								/* Check the devices id's to match a device */
								if(json_find_string(jchilds, opt->name, &stmp) == 0 &&
									 json_find_string(jmessage, opt->name, &stmp1) == 0 &&
									 opt->vartype == JSON_STRING &&
									 strcmp(stmp, stmp1) == 0) {
									match2++;
								} else if(json_find_number(jchilds, opt->name, &itmp) == 0 &&
													json_find_number(jmessage, opt->name, &itmp1) == 0 &&
													fabs(itmp-itmp1) < EPSILON &&
													opt->vartype == JSON_NUMBER) {
									match2++;
								}
							}

							opt = opt->next;
						}
						if(match1 > 0 && match2 > 0 && match1 == match2) {
							break;
						}
						jchilds = jchilds->next;
					}

					is_valid = 1;

					if(match1 > 0 && match2 > 0 && match1 == match2) {
						struct JsonNode *jcode = json_mkobject();
						if(json_find_string(jdevices, "state", &stmp) == 0 &&
							 json_find_string(jmessage, "state", &stmp1) == 0 &&
							 strcmp(stmp1, stmp) != 0 /*&&
							 json_find_string(rval, "state", &stmp) != 0*/) {
								strcpy(data->values[data->nrval].string_, stmp1);
								strcpy(data->values[data->nrval].name, "state");
								data->values[data->nrval].type = JSON_STRING;
								data->nrval++;
								json_append_member(jcode, "state", json_mkstring(stmp1));
								update = 1;
						}
						if(json_find_number(jdevices, "state", &itmp) == 0 &&
							 (jtmp = json_find_member(jmessage, "state")) != NULL &&
								jtmp->tag == JSON_NUMBER &&
							 fabs(itmp-jtmp->number_) >= EPSILON /*&&
							 json_find_number(rval, "state", &itmp) != 0*/) {
								data->values[data->nrval].number_ = jtmp->number_;
								strcpy(data->values[data->nrval].name, "state");
								data->values[data->nrval].type = JSON_NUMBER;
								data->values[data->nrval].decimals = jtmp->decimals_;
								data->nrval++;
								update = 1;
						}

						opt = protocol->options;
						/* Loop through all protocol options */
						while(opt) {
							if(opt->conftype == DEVICES_VALUE &&
								opt->argtype == OPTION_HAS_VALUE) {
								if(opt->vartype == JSON_STRING &&
										(jtmp = json_find_member(jmessage, opt->name)) != NULL &&
										jtmp->tag == JSON_STRING) {
									if(is_valid == 1 /*&& json_find_string(rval, opt->name, &stmp) != 0*/) {
										strcpy(data->values[data->nrval].string_, jtmp->string_);
										strcpy(data->values[data->nrval].name, opt->name);
										data->values[data->nrval].type = JSON_STRING;
										data->nrval++;
										json_append_member(jcode, opt->name, json_mkstring(jtmp->string_));
										update = 1;
									}
								} else if(opt->vartype == JSON_NUMBER &&
										(jtmp = json_find_member(jmessage, opt->name)) != NULL &&
										jtmp->tag == JSON_NUMBER) {
									if(is_valid == 1 /*&& json_find_number(rval, opt->name, &itmp) != 0*/) {
										data->values[data->nrval].number_ = jtmp->number_;
										strcpy(data->values[data->nrval].name, opt->name);
										data->values[data->nrval].type = JSON_NUMBER;
										data->values[data->nrval].decimals = jtmp->decimals_;
										data->nrval++;
										json_append_member(jcode, opt->name, json_mknumber(jtmp->number_, jtmp->decimals_));
										update = 1;
									}
								}
							}
							opt = opt->next;
						}
						if(protocol->checkValues != NULL) {
							/*struct JsonNode *jchilds = json_first_child(jsettings);
							while(jchilds) {
								if(jchilds->tag == JSON_NUMBER) {
									json_append_member(jcode, jchilds->key, json_mknumber(jchilds->number_, jchilds->decimals_));
								} else if(jchilds->tag == JSON_STRING) {
									json_append_member(jcode, jchilds->key, json_mkstring(jchilds->string_));
								}
								jchilds = jchilds->next;
							}*/
							if(protocol->checkValues(jcode) != 0) {
								update = 0;
							}
						}
						json_delete(jcode);
					}
				}
			}

			if(update == 1) {
				final_update = 1;
				strcpy(data->devices[data->nrdev++], jdevices->key);
			}
			update = 0;
			jdevices = jdevices->next;
		}
	}

	if(final_update == 1) {
		strncpy(data->origin, "update", 255);
		data->type = (int)protocol->devtype;
		if(strlen(pilight_uuid) > 0 && (protocol->hwtype == SENSOR || protocol->hwtype == HWRELAY)) {
			data->uuid = pilight_uuid;
		}
		eventpool_trigger(REASON_CONFIG_UPDATE, reason_config_update_free, data);
	} else {
		FREE(data);
	}
	json_delete(jmessage);
	/*if(jsettings != NULL) {
		json_delete(jsettings);
	}*/

	return NULL;
}

void storage_register(struct storage_t **s, const char *id) {
	if((*s = MALLOC(sizeof(struct storage_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	if(((*s)->id = MALLOC(strlen(id)+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy((*s)->id, id);
	(*s)->sync = NULL;
	(*s)->read = NULL;
	(*s)->gc = NULL;

	(*s)->devices_select = NULL;
	(*s)->hardware_select = NULL;
	(*s)->gui_select = NULL;

	(*s)->next = storage;
	storage = (*s);
}

/*
 * FIXME
 */
void storage_init(void) {
	eventpool_callback(REASON_CONFIG_UPDATE, devices_update_cache);
	eventpool_callback(REASON_CODE_RECEIVED, config_values_update);
	eventpool_callback(REASON_CODE_SENT, config_values_update);

	eventpool_callback(REASON_ADHOC_CONFIG_RECEIVED, storage_import_thread);
	jsonInit();
}

void devices_struct_parse(struct JsonNode *jdevices, int i) {
	struct JsonNode *jprotocols = NULL, *jchilds = NULL;
	struct device_t *node = MALLOC(sizeof(struct device_t));
	memset(node, 0, sizeof(struct device_t));
	if(node == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	if((node->id = MALLOC(strlen(jdevices->key)+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(node->id, jdevices->key);
	node->timestamp = 0;
	node->nrthreads = 0;
	node->nrprotocols = 0;
	node->nrprotocols1 = 0;
	node->protocols = NULL;
	node->protocols1 = NULL;
#ifdef EVENTS
	node->action_thread = NULL;
#endif
	node->next = NULL;

	if((jprotocols = json_find_member(jdevices, "protocol")) != NULL && jprotocols->tag == JSON_ARRAY) {
		jchilds = json_first_child(jprotocols);
		while(jchilds) {
			struct protocols_t *tmp_protocols = protocols;
			struct protocol_t *protocol = NULL;
			while(tmp_protocols) {
				protocol = tmp_protocols->listener;
				if(protocol_device_exists(protocol, jchilds->string_) == 0) {
					if((node->protocols = REALLOC(node->protocols, (sizeof(struct protocol_t *)*(size_t)(node->nrprotocols+1)))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}
					node->protocols[node->nrprotocols] = protocol;
					node->nrprotocols++;

					if((node->protocols1 = REALLOC(node->protocols1, (sizeof(char *)*(size_t)(node->nrprotocols1+1)))) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}
					if((node->protocols1[node->nrprotocols1] = STRDUP(jchilds->string_)) == NULL) {
						OUT_OF_MEMORY
					}
					node->nrprotocols1++;
				}
				tmp_protocols = tmp_protocols->next;
			}
			jchilds = jchilds->next;
		}
	}

	node->next = device;
	device = node;
}

int devices_validate_settings(struct JsonNode *jdevices, int i) {
	struct JsonNode *jsettings = NULL;
	struct JsonNode *jsettings1 = NULL;
	int y = 0, x = 0;
	int nrprotocol = 0, nrstate = 0, nrid = 0, match = 0, nrcomment = 0;
	int nruuid = 0, nrorigin = 0, nrtimestamps = 0, has_state = 0;

	jsettings = json_first_child(jdevices);
	x = 0;

	while(jsettings) {
		x++;
		y = 0;
		if(strcmp(jsettings->key, "comment") == 0) {
			nrcomment++;
		}
		if(strcmp(jsettings->key, "id") == 0) {
			nrid++;
		}
		if(strcmp(jsettings->key, "uuid") == 0) {
			nruuid++;
		}
		if(strcmp(jsettings->key, "origin") == 0) {
			nrorigin++;
		}
		if(strcmp(jsettings->key, "protocol") == 0) {
			nrprotocol++;
		}
		if(strcmp(jsettings->key, "state") == 0) {
			nrstate++;
		}
		if(strcmp(jsettings->key, "timestamp") == 0) {
			nrtimestamps++;
		}
		if(nrid > 1 || nrstate > 1 || nrprotocol > 1 || nruuid > 1 || nrtimestamps > 1 || nrcomment > 1) {
			if(i > 0) {
				logprintf(LOG_ERR, "config device setting #%d \"%s\" of \"%s\", duplicate", i, jsettings->key, jdevices->key);
			}
			return -1;
		}

		jsettings1 = json_first_child(jdevices);
		while(jsettings1) {
			y++;
			if(y > x) {
				if(strcmp(jsettings->key, jsettings1->key) == 0) {
					if(i > 0) {
						logprintf(LOG_ERR, "config device setting #%d \"%s\" of \"%s\", duplicate", i, jsettings->key, jdevices->key);
					}
					return -1;
				}
			}
			jsettings1 = jsettings1->next;
		}

		if(strcmp(jsettings->key, "state") == 0) {
			if(!(jsettings->tag == JSON_STRING || jsettings->tag == JSON_NUMBER)) {
				if(i > 0) {
					logprintf(LOG_ERR, "config device setting #%d \"%s\" of \"%s\", invalid", i, jsettings->key, jdevices->key);
				}
				return -1;
			}
		} else if(strcmp(jsettings->key, "id") == 0) {
			if(jsettings->tag != JSON_ARRAY) {
				if(i > 0) {
					logprintf(LOG_ERR, "config device setting #%d \"%s\" of \"%s\", invalid", i, jsettings->key, jdevices->key);
				}
				return -1;
			}
		} else if(strcmp(jsettings->key, "uuid") == 0) {
			if(jsettings->tag != JSON_STRING) {
				if(i > 0) {
					logprintf(LOG_ERR, "config device setting #%d \"%s\" of \"%s\", invalid", i, jsettings->key, jdevices->key);
				}
				return -1;
			}
		} else if(strcmp(jsettings->key, "timestamp") == 0) {
			if(jsettings->tag != JSON_NUMBER) {
				if(i > 0) {
					logprintf(LOG_ERR, "config device setting #%d \"%s\" of \"%s\", invalid", i, jsettings->key, jdevices->key);
				}
				return -1;
			}
		} else if(strcmp(jsettings->key, "origin") == 0) {
			if(jsettings->tag != JSON_STRING) {
				if(i > 0) {
					logprintf(LOG_ERR, "config device setting #%d \"%s\" of \"%s\", invalid", i, jsettings->key, jdevices->key);
				}
				return -1;
			}
		}
		jsettings = jsettings->next;
	}

	/* Check if required options by this protocol are missing
		 in the devices file */
	struct protocol_t *protocol = NULL;
	x = 0;
	while(devices_select_protocol(ORIGIN_CONFIG, jdevices->key, x++, &protocol) == 0) {
		if(protocol->options != NULL) {
			struct options_t *tmp_options = protocol->options;
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
				if(match == 0 && tmp_options->conftype == DEVICES_VALUE) {
					if(i > 0) {
						logprintf(LOG_ERR, "config device #%d setting \"%s\" of \"%s\", missing", i, tmp_options->name, jdevices->key);
					}
					return -1;
				}
				if(tmp_options->conftype == DEVICES_STATE) {
					has_state = 1;
				}
				tmp_options = tmp_options->next;
			}
		}
		jsettings = json_first_child(jdevices);
		while(jsettings) {
			match = 0;
			if(strcmp(jsettings->key, "protocol") != 0 &&
				 strcmp(jsettings->key, "uuid") != 0 &&
				 strcmp(jsettings->key, "origin") != 0 &&
				 strcmp(jsettings->key, "state") != 0 &&
				 strcmp(jsettings->key, "timestamp") != 0 &&
				 strcmp(jsettings->key, "id") != 0 &&
				 strcmp(jsettings->key, "comment") != 0) {
				struct options_t *tmp_options = protocol->options;
				while(tmp_options) {
					if(strcmp(jsettings->key, tmp_options->name) == 0) {
						if(jsettings->tag == tmp_options->vartype) {
							match = 1;
							break;
						}
					}
					tmp_options = tmp_options->next;
				}
				if(match == 0) {
					if(i > 0) {
						logprintf(LOG_ERR, "config device #%d setting \"%s\" of \"%s\", invalid", i, jsettings->key, jdevices->key);
					}
					return -1;
				}
			}
			jsettings = jsettings->next;
		}
	}
	if(has_state == 1 && nrstate == 0) {
		if(i > 0) {
			logprintf(LOG_ERR, "config device #%d setting \"%s\" of \"%s\", missing", i, "state", jdevices->key);
		}
		return -1;
	}
	return 0;
}

int devices_validate_protocols(struct JsonNode *jdevices, int i) {
	struct JsonNode *jprotocols = NULL;
	struct JsonNode *jchilds = NULL;
	int match = 0, ptype = -1;

	if((jprotocols = json_find_member(jdevices, "protocol")) != NULL && jprotocols->tag == JSON_ARRAY) {
		jchilds = json_first_child(jprotocols);
		while(jchilds) {
			match = 0;
			struct protocols_t *tmp_protocols = protocols;
			struct protocol_t *protocol = NULL;
			while(tmp_protocols) {
				protocol = tmp_protocols->listener;
				if(protocol_device_exists(protocol, jchilds->string_) == 0 && match == 0 && protocol->config == 1) {
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
				if(i > 0) {
					logprintf(LOG_ERR, "config device #%d \"%s\", cannot combine protocols of different hardware types", i, jdevices->key);
				}
				return -1;
			}
			if(match == 0) {
				if(i > 0) {
					logprintf(LOG_ERR, "config device #%d \"%s\", invalid protocol", i, jdevices->key);
				}
				return -1;
			}
			jchilds = jchilds->next;
		}
	} else {
		if(i > 0) {
			logprintf(LOG_ERR, "config device #%d \"%s\", missing protocol", i, jdevices->key);
		}
		return -1;
	}
	return 0;
}

int devices_validate_duplicates(struct JsonNode *jdevices, int i) {
	int x = 0;
	struct JsonNode *jdevices1 = json_first_child(jdevices->parent);
	while(jdevices1) {
		x++;
		if(x > i) {
			if(strcmp(jdevices1->key, jdevices->key) == 0) {
				if(i > 0) {
					logprintf(LOG_ERR, "config device #%d \"%s\", duplicate", i, jdevices->key);
				}
				return -1;
			}
		}
		jdevices1 = jdevices1->next;
	}
	return 0;
}

int devices_validate_name(struct JsonNode *jdevices, int i) {
	struct protocols_t *tmp_protocols = protocols;
	int x = 0;

	for(x=0;x<strlen(jdevices->key);x++) {
		if(!isalnum(jdevices->key[x]) && jdevices->key[x] != '-' && jdevices->key[x] != '_') {
			if(i > 0) {
				logprintf(LOG_ERR, "config device #%d \"%s\", not alphanumeric", i, jdevices->key);
			}
			return -1;
		}
	}
	while(tmp_protocols) {
		struct protocol_t *protocol = tmp_protocols->listener;
		if(strcmp(protocol->id, jdevices->key) == 0) {
			logprintf(LOG_ERR, "config device #%d \"%s\", protocol names are reserved words", i, jdevices->key);
			return -1;
		}
		tmp_protocols = tmp_protocols->next;
	}

	return 0;
}

int devices_validate_state(struct JsonNode *jdevices, int i) {
	struct JsonNode *jstate = NULL;
	struct options_t *tmp_options = NULL;
	char ctmp[256], *stmp = NULL;
	int valid_state = 0, x = 0;
	double itmp = 0;

#if !defined(__FreeBSD__) && !defined(_WIN32)
	/* Regex variables */
	regex_t regex;
	int reti = 0;
	memset(&regex, '\0', sizeof(regex));
#endif

	valid_state = 0;
	if((jstate = json_find_member(jdevices, "state")) != NULL) {

		/* Cast the different values */
		if(jstate->tag == JSON_NUMBER && json_find_number(jstate->parent, jstate->key, &itmp) == 0) {
			sprintf(ctmp, "%.0f", itmp);
		} else if(jstate->tag == JSON_STRING && json_find_string(jstate->parent, jstate->key, &stmp) == 0) {
			strcpy(ctmp, stmp);
		}

		struct protocol_t *protocol = NULL;
		x = 0;
		while(devices_select_protocol(ORIGIN_CONFIG, jdevices->key, x++, &protocol) == 0) {
			if(protocol->options != NULL) {
				tmp_options = protocol->options;
				while(tmp_options) {
					/* We are only interested in the DEVICES_STATE options */
					if(tmp_options->conftype == DEVICES_STATE && tmp_options->vartype == jstate->tag) {
						/* If an option requires an argument, then check if the
							 argument state values and values array are of the right
							 type. This is done by checking the regex mask */
						if(tmp_options->argtype == OPTION_HAS_VALUE) {
							if(tmp_options->mask != NULL && strlen(tmp_options->mask) > 0) {
#if !defined(__FreeBSD__) && !defined(_WIN32)
								reti = regcomp(&regex, tmp_options->mask, REG_EXTENDED);
								if(reti) {
									if(i > 0) {
										logprintf(LOG_ERR, "%s: could not compile %s regex", protocol->id, tmp_options->name);
									}
									return -1;
								}
								reti = regexec(&regex, ctmp, 0, NULL, 0);

								if(reti == REG_NOMATCH || reti != 0) {
									if(i > 0) {
										logprintf(LOG_ERR, "config device setting #%d \"%s\" of \"%s\", invalid", i, jstate->key, jdevices->key);
									}
									regfree(&regex);
									return -1;
								}
								regfree(&regex);
#endif
							}
						} else {
							/* If a protocol has DEVICES_STATE arguments, than these define
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
		}
		if(valid_state == 0) {
			if(i > 0) {
				logprintf(LOG_ERR, "config device setting #%d \"%s\" of \"%s\", invalid", i, jstate->key, jdevices->key);
			}
			return -1;
		}
	}

	return 0;
}

int devices_validate_values(struct JsonNode *jdevices, int i) {
	struct protocol_t *protocol = NULL;
	int x = 0;

	while(devices_select_protocol(ORIGIN_CONFIG, jdevices->key, x++, &protocol) == 0) {
		if(protocol->options != NULL) {
			if(protocol->checkValues != NULL) {
				if(protocol->checkValues(jdevices) != 0) {
					if(i > 0) {
						logprintf(LOG_ERR, "config device #%d \"%s\", invalid", i, jdevices->key);
					}
					return -1;
				}
			}
		}
	}
	return 0;
}

int devices_validate_id(struct JsonNode *jdevices, int i) {
	struct JsonNode *jid = NULL;
	struct JsonNode *jvalues = NULL;
	struct JsonNode *jsetting = NULL;
	struct options_t *tmp_options = NULL;
	struct protocol_t *protocol = NULL;

	int match1 = 0, match2 = 0, match3 = 0, has_id = 0;
	int valid_values = 0, nrids1 = 0, x = 0;
	int nrids2 = 0, etype = 0;
	unsigned short nrid = 0;
	char ctmp[256];

	if((jsetting = json_find_member(jdevices, "id")) != NULL && jsetting->tag == JSON_ARRAY) {
		while(devices_select_protocol(ORIGIN_CONFIG, jdevices->key, x++, &protocol) == 0) {
			if(protocol->options != NULL) {
				jid = json_first_child(jsetting);
				has_id = 0;
				nrid = 0;
				match3 = 0;
				while(jid) {
					nrid++;
					match2 = 0; match1 = 0; nrids1 = 0; nrids2 = 0;
					jvalues = json_first_child(jid);
					while(jvalues) {
						nrids1++;
						jvalues = jvalues->next;
					}
					if((tmp_options = protocol->options)) {
						while(tmp_options) {
							if(tmp_options->conftype == DEVICES_ID) {
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
							if((tmp_options = protocol->options)) {
								while(tmp_options) {
									if(tmp_options->conftype == DEVICES_ID && tmp_options->vartype == jvalues->tag) {
										if(strcmp(tmp_options->name, jvalues->key) == 0) {
											match2++;
											if(jvalues->tag == JSON_NUMBER) {
												sprintf(ctmp, "%.*f", jvalues->decimals_, jvalues->number_);
											} else if(jvalues->tag == JSON_STRING) {
												strcpy(ctmp, jvalues->string_);
											}

											if(tmp_options->mask != NULL && strlen(tmp_options->mask) > 0) {
#if !defined(__FreeBSD__) && !defined(_WIN32)
												regex_t regex;
												memset(&regex, '\0', sizeof(regex));
												int reti = regcomp(&regex, tmp_options->mask, REG_EXTENDED);
												if(reti) {
													if(i > 0) {
														logprintf(LOG_ERR, "%s: could not compile %s regex", protocol->id, tmp_options->name);
													}
												} else {
													reti = regexec(&regex, ctmp, 0, NULL, 0);
													if(reti == REG_NOMATCH || reti != 0) {
														match2--;
													}
													regfree(&regex);
												}
#endif
											}
										}
									}
									tmp_options = tmp_options->next;
								}
							}
							jvalues = jvalues->next;
						}
					}
					jid = jid->next;
					if(match2 > 0 && match1 == match2) {
						match3 = 1;
					}
				}
				if(has_id == 0) {
					valid_values--;
				} else if(protocol->multipleId == 0 && nrid > 1) {
					valid_values--;
					etype = 1;
					break;
				} else if(match3 > 0) {
					valid_values++;
				} else {
					valid_values--;
				}
			}
		}
		if((x-1) != valid_values) {
			if(etype == 1) {
				if(i > 0) {
					logprintf(LOG_ERR, "protocol \"%s\" used in \"%s\" does not support multiple id's", protocol->id, jdevices->key);
				}
			} else {
				if(i > 0) {
					logprintf(LOG_ERR, "config device setting #%d \"%s\" of \"%s\", invalid", i, "id", jdevices->key);
				}
			}
			return -1;
		}
	}
	return 0;
}

void devices_init_event_threads(struct JsonNode *jdevices, int i) {
	struct device_t *nodes = device;

	while(nodes) {
		if(strcmp(jdevices->key, nodes->id) == 0) {
			break;
		}
		nodes = nodes->next;
	}
	if(nodes == NULL) {
		return;
	}

	// event_action_thread_init(nodes);
}

void devices_init_protocol_threads(struct JsonNode *jdevices, int i) {
	struct device_t *nodes = device;
	char *dev_uuid = NULL, _dev_uuid[UUID_LENGTH];

	while(nodes) {
		if(strcmp(jdevices->key, nodes->id) == 0) {
			break;
		}
		nodes = nodes->next;
	}
	if(nodes == NULL) {
		return;
	}

	if(json_find_string(jdevices, "uuid", &dev_uuid) != 0) {
		if(strlen(pilight_uuid) > 0) {
			strcpy(_dev_uuid, pilight_uuid);
			dev_uuid = _dev_uuid;
		}
	}

	if(strlen(pilight_uuid) > 0 && strcmp(dev_uuid, pilight_uuid) == 0) {
		char *cpy = json_stringify(jdevices, NULL);
		int l = strlen(cpy)+6+strlen(jdevices->key);
		char *output = MALLOC(l);
		if(output == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		snprintf(output, l, "{\"%s\":%s}", jdevices->key, cpy);
		eventpool_trigger(REASON_DEVICE_ADDED, reason_device_added_free, json_decode(output));
		json_free(cpy);
		FREE(output);
	}
}

#ifdef EVENTS
int rules_validate_name(struct JsonNode *jrules, int i) {
	int x = 0;

	for(x=0;x<strlen(jrules->key);x++) {
		if(!isalnum(jrules->key[x]) && jrules->key[x] != '-' && jrules->key[x] != '_') {
			if(i > 0) {
				logprintf(LOG_ERR, "config rule #%d \"%s\", not alphanumeric", i, jrules->key);
			}
			return -1;
		}
	}
	return 0;
}

int rules_validate_settings(struct JsonNode *jrules, int i) {
	struct JsonNode *jsettings = NULL;
	int x = 0;
	int nrrule = 0, nractive = 0;

	jsettings = json_first_child(jrules);
	x = 0;

	while(jsettings) {
		x++;
		if(strcmp(jsettings->key, "rule") == 0) {
			nrrule++;
		}
		if(strcmp(jsettings->key, "active") == 0) {
			nractive++;
		}
		if(nrrule > 1 || nractive > 1) {
			if(i > 0) {
				logprintf(LOG_ERR, "config rule setting #%d \"%s\" of \"%s\", duplicate", i, jsettings->key, jrules->key);
			}
			return -1;
		}

		if(strcmp(jsettings->key, "rule") == 0) {
			if(jsettings->tag != JSON_STRING) {
				if(i > 0) {
					logprintf(LOG_ERR, "config rule setting #%d \"%s\" of \"%s\", invalid", i, jsettings->key, jrules->key);
				}
				return -1;
			}
		} else if(strcmp(jsettings->key, "active") == 0) {
			if(jsettings->tag != JSON_NUMBER || (jsettings->number_ != 0 && jsettings->number_ != 1)) {
				if(i > 0) {
					logprintf(LOG_ERR, "config rule setting #%d \"%s\" of \"%s\", invalid", i, jsettings->key, jrules->key);
				}
				return -1;
			}
		}
		jsettings = jsettings->next;
	}
	if(nrrule == 0) {
		if(i > 0) {
			logprintf(LOG_ERR, "config rule setting \"%s\" of \"%s\", missing", "rule", jrules->key);
		}
		return -1;
	}
	if(nractive == 0) {
		if(i > 0) {
			logprintf(LOG_ERR, "config rule setting \"%s\" of \"%s\", missing", "active", jrules->key);
		}
		return -1;
	}
	return 0;
}

int rules_validate_duplicates(struct JsonNode *jrules, int i) {
	int x = 0;
	struct JsonNode *jrules1 = json_first_child(jrules->parent);
	while(jrules1) {
		x++;
		if(x > i) {
			if(strcmp(jrules1->key, jrules->key) == 0) {
				if(i > 0) {
					logprintf(LOG_ERR, "config rules #%d \"%s\", duplicate", i, jrules->key);
				}
				return -1;
			}
		}
		jrules1 = jrules1->next;
	}
	return 0;
}

int rules_struct_parse(struct JsonNode *jrules, int i) {
	struct rules_t *tmp = NULL;
	char *rule = NULL;
	int have_error = 0;
	double active = 1.0;

	json_find_number(jrules, "active", &active);
	json_find_string(jrules, "rule", &rule);

	struct rules_t *node = MALLOC(sizeof(struct rules_t));
	if(node == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(node, 0, sizeof(struct rules_t));
	node->nr = i;
	if((node->name = MALLOC(strlen(jrules->key)+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(node->name, jrules->key);

#ifndef _WIN32
	clock_gettime(CLOCK_MONOTONIC, &node->timestamp.first);
#endif
	if(event_parse_rule(rule, node, 0, 1) == -1) {
		have_error = -1;
	}
#ifndef _WIN32
	clock_gettime(CLOCK_MONOTONIC, &node->timestamp.second);
	logprintf(LOG_INFO, "rule #%d %s was parsed in %.6f seconds", node->nr, node->name,
		((double)node->timestamp.second.tv_sec + 1.0e-9*node->timestamp.second.tv_nsec) -
		((double)node->timestamp.first.tv_sec + 1.0e-9*node->timestamp.first.tv_nsec));
#endif

	node->status = 0;
	if((node->rule = MALLOC(strlen(rule)+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(node->rule, rule);
	node->active = (unsigned short)active;

	tmp = rules;
	if(tmp) {
		while(tmp->next != NULL) {
			tmp = tmp->next;
		}
		tmp->next = node;
	} else {
		node->next = rules;
		rules = node;
	}
	/*
	 * In case of an error, we do want to
	 * save a pointer to our faulty rule
	 * so it can be properly garbage collected.
	 */

	return have_error;
}
#endif

int gui_validate_duplicates(struct JsonNode *jgui, int i) {
	int x = 0;
	struct JsonNode *jgui1 = json_first_child(jgui->parent);
	while(jgui1) {
		x++;
		if(x > i) {
			if(strcmp(jgui1->key, jgui->key) == 0) {
				if(i > 0) {
					logprintf(LOG_ERR, "config gui #%d \"%s\", duplicate", i, jgui->key);
				}
				return -1;
			}
		}
		jgui1 = jgui1->next;
	}
	return 0;
}

int gui_validate_id(struct JsonNode *jgui, int i) {
	struct JsonNode *jdevices = NULL;
	if(storage->devices_select(ORIGIN_CONFIG, jgui->key, &jdevices) != 0) {
		if(i > 0) {
			logprintf(LOG_ERR, "config gui element #%d \"%s\", device not configured", i, jgui->key);
		}
		return -1;
	}
	return 0;
}

int gui_validate_settings(struct JsonNode *jgui, int i) {
	struct JsonNode *jsettings = NULL, *jsettings1 = NULL;
	struct JsonNode *jdevices = NULL, *jprotocols = NULL;
	struct JsonNode *jchilds = NULL;
	unsigned int nrgroup = 0, nrmedia = 0, nrname = 0, nrorder = 0;
	int x = 0, y = 0, match = 0;

	jsettings = json_first_child(jgui);
	x = 0;
	storage->devices_select(ORIGIN_CONFIG, jgui->key, &jdevices);

	while(jsettings) {
		x++;
		y = 0;
		if(strcmp(jsettings->key, "group") == 0) {
			nrgroup++;
		} else if(strcmp(jsettings->key, "media") == 0) {
			nrmedia++;
		} else if(strcmp(jsettings->key, "name") == 0) {
			nrname++;
		} else if(strcmp(jsettings->key, "order") == 0) {
			nrorder++;
		}
		if(nrmedia > 1 || nrgroup > 1 || nrname > 1 || nrorder > 1) {
			if(i > 0) {
				logprintf(LOG_ERR, "config gui element #%d \"%s\" of \"%s\", duplicate", i, jsettings->key, jgui->key);
			}
			return -1;
		}

		jsettings1 = json_first_child(jgui);
		while(jsettings1) {
			y++;
			if(y > x) {
				if(strcmp(jsettings->key, jsettings1->key) == 0) {
					if(i > 0) {
						logprintf(LOG_ERR, "config gui setting #%d \"%s\" of \"%s\", duplicate", i, jsettings->key, jgui->key);
					}
					return -1;
				}
			}
			jsettings1 = jsettings1->next;
		}

		if(strcmp(jsettings->key, "group") == 0) {
			if(jsettings->tag != JSON_ARRAY) {
				if(i > 0) {
					logprintf(LOG_ERR, "config device setting #%d \"%s\" of \"%s\", invalid", i, jsettings->key, jgui->key);
				}
				return -1;
			}
		} else if(strcmp(jsettings->key, "media") == 0) {
			if(jsettings->tag != JSON_ARRAY) {
				if(i > 0) {
					logprintf(LOG_ERR, "config device setting #%d \"%s\" of \"%s\", invalid", i, jsettings->key, jgui->key);
				}
				return -1;
			}
		} else if(strcmp(jsettings->key, "name") == 0) {
			if(jsettings->tag != JSON_STRING) {
				if(i > 0) {
					logprintf(LOG_ERR, "config device setting #%d \"%s\" of \"%s\", invalid", i, jsettings->key, jgui->key);
				}
				return -1;
			}
		} else if(strcmp(jsettings->key, "order") == 0) {
			if(jsettings->tag != JSON_NUMBER) {
				if(i > 0) {
					logprintf(LOG_ERR, "config device setting #%d \"%s\" of \"%s\", invalid", i, jsettings->key, jgui->key);
				}
				return -1;
			}
		} else {
			match = 0;
			if((jprotocols = json_find_member(jdevices, "protocol")) != NULL && jprotocols->tag == JSON_ARRAY) {
				jchilds = json_first_child(jprotocols);
				while(jchilds) {
					struct protocols_t *tmp_protocols = protocols;
					struct protocol_t *protocol = NULL;
					while(tmp_protocols) {
						protocol = tmp_protocols->listener;
						if(protocol_device_exists(protocol, jchilds->string_) == 0) {
							if(protocol->options != NULL) {
								struct options_t *tmp_options = protocol->options;
								while(tmp_options) {
									if(tmp_options->conftype == GUI_SETTING &&
										 tmp_options->vartype == jsettings->tag &&
										 strcmp(jsettings->key, tmp_options->name) == 0) {
										match = 1;
										break;
									}
									tmp_options = tmp_options->next;
								}
							}
						}
						if(match == 1) {
							break;
						}
						tmp_protocols = tmp_protocols->next;
					}
					if(match == 1) {
						break;
					}
					jchilds = jchilds->next;
				}
			}
			if(match == 0) {
				if(i > 0) {
					logprintf(LOG_ERR, "config gui element #%d \"%s\" of \"%s\", invalid", i, jsettings->key, jgui->key);
				}
				return -1;
			}
		}

		jsettings = jsettings->next;
	}
	if(nrgroup == 0) {
		if(i > 0) {
			logprintf(LOG_ERR, "config gui element #%d \"%s\", missing \"group\"", i, jgui->key);
		}
		return -1;
	}
	if(nrname == 0) {
		if(i > 0) {
			logprintf(LOG_ERR, "config gui element #%d \"%s\", missing \"name\"", i, jgui->key);
		}
		return -1;
	}

	return 0;
}

int gui_validate_media(struct JsonNode *jgui, int i) {
	struct JsonNode *jmedia = NULL;
	struct JsonNode *jchilds = NULL;
	int hasall = 0, nrvalues = 0;

	if(jgui != NULL) {
		if((jmedia = json_find_member(jgui, "media")) != NULL && jmedia->tag == JSON_ARRAY) {
			jchilds = json_first_child(jmedia);
			while(jchilds) {
				if(jchilds->tag == JSON_STRING) {
					if(strcmp(jchilds->string_, "web") != 0 &&
					strcmp(jchilds->string_, "mobile") != 0 &&
					strcmp(jchilds->string_, "desktop") != 0 &&
					strcmp(jchilds->string_, "all") != 0) {
						if(i > 0) {
							logprintf(LOG_ERR, "config gui element #%d \"media\" of \"%s\" can only contain \"web\", \"desktop\", \"mobile\", or \"all\"", i, jgui->key);
						}
						return -1;
					} else {
						nrvalues++;
					}
					if(strcmp(jchilds->string_, "all") == 0) {
						hasall = 1;
					}
				} else {
					if(i > 0) {
						logprintf(LOG_ERR, "config gui element #%d \"media\" of \"%s\", invalid", i, jgui->key);
					}
					return -1;
				}
				if(hasall == 1 && nrvalues > 1) {
					if(i > 0) {
						logprintf(LOG_ERR, "config gui element #%d \"media\" of \"%s\" value \"all\" cannot be combined with other values", i, jgui->key);
					}
					return -1;
				}
				jchilds = jchilds->next;
			}
		}
	}
	return 0;
}

int settings_validate_duplicates(struct JsonNode *jsettings, int i) {
	struct JsonNode *jsettings1 = json_first_child(jsettings->parent);
	int x = 0;

	while(jsettings1) {
		x++;
		if(x > i) {
			if(strcmp(jsettings1->key, jsettings->key) == 0) {
				if(i > 0) {
					logprintf(LOG_ERR, "config settings #%d \"%s\", duplicate", i, jsettings->key);
				}
				return -1;
			}
		}
		jsettings1 = jsettings1->next;
	}
	return 0;
}

int settings_validate_settings(struct JsonNode *jsettings, int i) {
#if !defined(__FreeBSD__) && !defined(_WIN32)
	/* Regex variables */
	regex_t regex;
	int reti = 0;
	memset(&regex, '\0', sizeof(regex));
#endif

	if(strcmp(jsettings->key, "port") == 0 ||
	   strcmp(jsettings->key, "arp-interval") == 0 ||
		 strcmp(jsettings->key, "arp-timeout") == 0) {
		if(jsettings->tag != JSON_NUMBER) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain a number larger than 0", i, jsettings->key);
			}
			return -1;
		} else if((int)jsettings->number_ == 0) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain a number larger than 0", i, jsettings->key);
			}
			return -1;
		}
	} else if(strcmp(jsettings->key, "firmware-gpio-reset") == 0
		|| strcmp(jsettings->key, "firmware-gpio-sck") == 0
		|| strcmp(jsettings->key, "firmware-gpio-mosi") == 0
		|| strcmp(jsettings->key, "firmware-gpio-miso") == 0) {
#if !defined(__arm__) || !defined(__mips__)				
		return -1;
#endif
		if(jsettings->tag != JSON_NUMBER) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain a number larger than 0", i, jsettings->key);
			}
			return -1;
		} else if(wiringXValidGPIO((int)jsettings->number_) != 0) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain a valid GPIO number", i, jsettings->key);
			}
			return -1;
		}
	} else if(strcmp(jsettings->key, "gpio-platform") == 0) {
		if(jsettings->tag != JSON_STRING) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain a supported gpio platform", i, jsettings->key);
			}
			return -1;
		} else if(jsettings->string_ == NULL) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain a supported gpio platform", i, jsettings->key);
			}
			return -1;
		} else {
			if(strcmp(jsettings->string_, "none") != 0) { 				
				if(wiringXSetup(jsettings->string_, _logprintf) != 0 && i > 0) {
					logprintf(LOG_ERR, "config setting #%d \"%s\" must contain a supported gpio platform", i, jsettings->key);
				}
			}
		}		
	} else if(strcmp(jsettings->key, "standalone") == 0 ||
						strcmp(jsettings->key, "watchdog-enable") == 0 ||
						strcmp(jsettings->key, "stats-enable") == 0) {
		if(jsettings->tag != JSON_NUMBER) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must be either 0 or 1", i, jsettings->key);
			}
			return -1;
		} else if(jsettings->number_ < 0 || jsettings->number_ > 1) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must be either 0 or 1", i, jsettings->key);
			}
			return -1;
		}
	} else if(strcmp(jsettings->key, "log-level") == 0) {
		if(jsettings->tag != JSON_NUMBER) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain a number from 0 till 6", i, jsettings->key);
			}
			return -1;
		} else if((int)jsettings->number_ < 0 || (int)jsettings->number_ > 6) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain a number from 0 till 6", i, jsettings->key);
			}
			return -1;
		}
	} else if(strcmp(jsettings->key, "log-file") == 0
#ifndef _WIN32
			|| strcmp(jsettings->key, "pid-file") == 0
#endif
			|| strcmp(jsettings->key, "pem-file") == 0
#ifdef WEBSERVER
			|| strcmp(jsettings->key, "webserver-root") == 0
#endif
		) {
		if(jsettings->tag != JSON_STRING) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain an existing file", i, jsettings->key);
			}
			return -1;
		} else if(jsettings->string_ == NULL) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain an existing file", i, jsettings->key);
			}
			return -1;
		} else {
			char *dir = NULL;
			char *cpy = STRDUP(jsettings->string_);
			if(cpy == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}


			if((dir = _dirname(cpy)) == NULL) {
				logprintf(LOG_ERR, "could not open logfile %s", log);
				atomicunlock();
				return -1;
			}

			if(path_exists(dir) != EXIT_SUCCESS) {
				if(i > 0) {
					logprintf(LOG_ERR, "config setting #%d \"%s\" must point to an existing folder", i, jsettings->key);
				}
				FREE(cpy);
				return -1;
			}
			FREE(cpy);
		}
	} else if(strcmp(jsettings->key, "whitelist") == 0) {
		if(jsettings->tag != JSON_STRING) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain a valid ip address", i, jsettings->key);
			}
			return -1;
		} else if(!jsettings->string_) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain a valid ip addresses", i, jsettings->key);
			}
			return -1;
		} else if(strlen(jsettings->string_) > 0) {
#if !defined(__FreeBSD__) && !defined(_WIN32)
			char validate[] = "^((\\*|[0-9]|[1-9][0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))\\.(\\*|[0-9]|[1-9][0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))\\.(\\*|[0-9]|[1-9][0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))\\.(\\*|[0-9]|[1-9][0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))(,[\\ ]|,|$))+$";
			reti = regcomp(&regex, validate, REG_EXTENDED);
			if(reti) {
				if(i > 0) {
					logprintf(LOG_ERR, "could not compile regex");
				}
				return -1;
			}
			reti = regexec(&regex, jsettings->string_, 0, NULL, 0);
			if(reti == REG_NOMATCH || reti != 0) {
				if(i > 0) {
					logprintf(LOG_ERR, "config setting #%d \"%s\" must contain valid ip addresses", i, jsettings->key);
				}
				regfree(&regex);
				return -1;
			}
			regfree(&regex);
#endif
			int l = (int)strlen(jsettings->string_)-1;
			if(jsettings->string_[l] == ' ' || jsettings->string_[l] == ',') {
				if(i > 0) {
					logprintf(LOG_ERR, "config setting #%d \"%s\" must contain valid ip addresses", i, jsettings->key);
				}
				return -1;
			}
		}
#ifdef WEBSERVER
	#ifdef WEBSERVER_HTTPS
	} else if(strcmp(jsettings->key, "webserver-https-port") == 0) {
		if(jsettings->tag != JSON_NUMBER) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain a number larget than 0", i, jsettings->key);
			}
			return -1;
		} else if(jsettings->number_ < -1) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain a number larger than 0", i, jsettings->key);
			}
			return -1;
		}
	#endif
	} else if(strcmp(jsettings->key, "webserver-http-port") == 0) {
		if(jsettings->tag != JSON_NUMBER) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain a number larget than 0", i, jsettings->key);
			}
			return -1;
		} else if(jsettings->number_ < -1) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain a number larger than 0", i, jsettings->key);
			}
			return -1;
		}
	} else if(strcmp(jsettings->key, "webserver-enable") == 0) {
		if(jsettings->tag != JSON_NUMBER) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must be either 0 or 1", i, jsettings->key);
			}
			return -1;
		} else if(jsettings->number_ < 0 || jsettings->number_ > 1) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must be either 0 or 1", i, jsettings->key);
			}
			return -1;
		}
	} else if(strcmp(jsettings->key, "webserver-cache") == 0 ||
						strcmp(jsettings->key, "webgui-websockets") == 0) {
		if(jsettings->tag != JSON_NUMBER) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must be either 0 or 1", i, jsettings->key);
			}
			return -1;
		} else if(jsettings->number_ < 0 || jsettings->number_ > 1) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must be either 0 or 1", i, jsettings->key);
			}
			return -1;
		}
	} else if(strcmp(jsettings->key, "webserver-authentication") == 0 && jsettings->tag == JSON_ARRAY) {
		struct JsonNode *jtmp = json_first_child(jsettings);
		unsigned short x = 0;
		while(jtmp) {
			x++;
			if(jtmp->tag != JSON_STRING || x > 2) {
				if(i > 0) {
					logprintf(LOG_ERR, "config setting #%d \"%s\" must be in the format of [ \"username\", \"password\" ]", i, jsettings->key);
				}
				return -1;
			}
			jtmp = jtmp->next;
		}
#endif // WEBSERVER
	} else if(strcmp(jsettings->key, "ntp-servers") == 0 && jsettings->tag == JSON_ARRAY) {
		struct JsonNode *jtmp = json_first_child(jsettings);
		int nrntp = 0;

		while(jtmp) {
			nrntp++;
			if(jtmp->tag != JSON_STRING) {
				if(i > 0) {
					logprintf(LOG_ERR, "config setting #%d \"%s\" must be in the format of [ \"0.eu.pool.ntp.org\", ... ]", i, jsettings->key);
				}
				return -1;
			}
			jtmp = jtmp->next;
		}
		if(nrntp > 5) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" can only take a maximum of 5 servers", i, jsettings->key);
			}
			return -1;
		}
	} else if(strcmp(jsettings->key, "protocol-root") == 0 ||
						strcmp(jsettings->key, "hardware-root") == 0 ||
						strcmp(jsettings->key, "actions-root") == 0 ||
						strcmp(jsettings->key, "functions-root") == 0 ||
						strcmp(jsettings->key, "operators-root") == 0) {
		if(jsettings->tag != JSON_STRING) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain a valid path", i, jsettings->key);
			}
			return -1;
		} else if(jsettings->string_ == NULL || path_exists(jsettings->string_) != 0) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain a valid path", i, jsettings->key);
			}
			return -1;
		}
#ifdef EVENTS
	} else if(strcmp(jsettings->key, "smtp-sender") == 0) {
		if(jsettings->tag != JSON_STRING) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain an e-mail address", i, jsettings->key);
			}
			return -1;
		} else if(jsettings->string_ == NULL) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting \"%s\" must contain an e-mail address", i, jsettings->key);
			}
			return -1;
		} else if(strlen(jsettings->string_) > 0) {
#if !defined(__FreeBSD__) && !defined(_WIN32)
			char validate[] = "^[a-zA-Z0-9_.]+@([a-zA-Z0-9]+\\.)+([a-zA-Z0-9]{2,3}){1,2}$";
			reti = regcomp(&regex, validate, REG_EXTENDED);
			if(reti) {
				if(i > 0) {
					logprintf(LOG_ERR, "could not compile regex for %s", jsettings->key);
				}
				return -1;
			}
			reti = regexec(&regex, jsettings->string_, 0, NULL, 0);
			if(reti == REG_NOMATCH || reti != 0) {
				if(i > 0) {
					logprintf(LOG_ERR, "config setting #%d \"%s\" must contain an e-mail address", i, jsettings->key);
				}
				regfree(&regex);
				return -1;
			}
			regfree(&regex);
#endif
		}
	} else if(strcmp(jsettings->key, "smtp-user") == 0) {
		if(jsettings->tag != JSON_STRING) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting \"%s\" must contain a user id", jsettings->key);
			}
			return -1;
		} else if(jsettings->string_ == NULL) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting \"%s\" must contain a user id", jsettings->key);
			}
			return -1;
		}
	} else if(strcmp(jsettings->key, "smtp-password") == 0) {
		if(jsettings->tag != JSON_STRING) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain a password string", i, jsettings->key);
			}
			return -1;
		} else if(jsettings->string_ == NULL) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain a password string", i, jsettings->key);
			}
			return -1;
		}
	} else if(strcmp(jsettings->key, "smtp-ssl") == 0) {
		if(jsettings->tag != JSON_NUMBER) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must be either 0 or 1", i, jsettings->key);
			}
			return -1;
		} else if(jsettings->number_ < 0 || jsettings->number_ > 1) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must be either 0 or 1", i, jsettings->key);
			}
			return -1;
		}
	} else if(strcmp(jsettings->key, "smtp-host") == 0) {
		if(jsettings->tag != JSON_STRING) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain an smtp host address", i, jsettings->key);
			}
			return -1;
		} else if(jsettings->string_ == NULL) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain an smtp host address", i, jsettings->key);
			}
			return -1;
		} /*else if(strlen(jsettings->string_) > 0) {
#if !defined(__FreeBSD__) && !defined(_WIN32)
			char validate[] = "^([a-zA-Z0-9\\_\\-]){2,20}(\\.([a-zA-Z0-9\\_\\-]){2,20}){2,3}$";
			reti = regcomp(&regex, validate, REG_EXTENDED);
			if(reti) {
				if(i > 0) {
					logprintf(LOG_ERR, "could not compile regex for %s", jsettings->key);
				}
				return -1;
			}
			reti = regexec(&regex, jsettings->string_, 0, NULL, 0);
			if(reti == REG_NOMATCH || reti != 0) {
				if(i > 0) {
					logprintf(LOG_ERR, "config setting #%d \"%s\" must contain an smtp host address", i, jsettings->key);
				}
				regfree(&regex);
				return -1;
			}
			regfree(&regex);
#endif
		}*/
	} else if(strcmp(jsettings->key, "smtp-port") == 0) {
		if(jsettings->tag != JSON_NUMBER) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must be 25, 465 or 587", i, jsettings->key);
			}
			return -1;
		}
#endif //EVENTS
	} else if(strcmp(jsettings->key, "name") == 0 ||
					  strcmp(jsettings->key, "adhoc-master") == 0) {
		if(jsettings->tag != JSON_STRING) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain a string", i, jsettings->key);
			}
			return -1;
		} else if(strlen(jsettings->string_) > 16) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" can not be more than 16 characters", i, jsettings->key);
			}
			return -1;
		}
	} else if(strcmp(jsettings->key, "adhoc-mode") == 0) {
		if(jsettings->tag != JSON_STRING) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must contain a string", i, jsettings->key);
			}
			return -1;
		} else if(!(strcmp(jsettings->string_, "server") == 0 || strcmp(jsettings->string_, "client") == 0)) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting #%d \"%s\" must be either \"server\" or \"client\"", i, jsettings->key);
			}
			return -1;
		}
	} else {
		if(i > 0) {
			logprintf(LOG_ERR, "config setting #%d \"%s\" is invalid", i, jsettings->key);
		}
		return -1;
	}
	return 0;
}

int settings_validate_values(struct JsonNode *jsettings, int i) {
	struct JsonNode *jparent = jsettings->parent;
	double itmp = 0.0;
	char *adhoc_mode = NULL, *adhoc_master = NULL;

#ifdef WEBSERVER
	int web_http_port = WEBSERVER_HTTP_PORT;

	#ifdef WEBSERVER_HTTPS
		int web_https_port = WEBSERVER_HTTPS_PORT;
	#endif

	int pilight_daemon_port = -1;
	if(strcmp(jsettings->key, "port") == 0 ||
#ifdef WEBSERVER_HTTPS
	   strcmp(jsettings->key, "webserver-https-port") == 0 ||
#endif
	   strcmp(jsettings->key, "webserver-http-port") == 0) {
		if(json_find_number(jparent, "webserver-http-port", &itmp) == 0) {
			web_http_port = (int)itmp;
		}
#ifdef WEBSERVER_HTTPS
		if(json_find_number(jparent, "webserver-https-port", &itmp) == 0) {
			web_https_port = (int)itmp;
		}
#endif
		if(json_find_number(jparent, "port", &itmp) == 0) {
			pilight_daemon_port = (int)itmp;
		}
		if(web_http_port == pilight_daemon_port) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting \"port\" and \"webserver-http-port\" cannot be the same");
			}
			return -1;
		}
#ifdef WEBSERVER_HTTPS
		if(web_https_port == pilight_daemon_port) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting \"port\" and \"webserver-https-port\" cannot be the same");
			}
			return -1;
		}
		if(web_https_port == web_http_port) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting \"webserver-http-port\" and \"webserver-https-port\" cannot be the same");
			}
			return -1;
		}
#endif
	}
#endif
	if(strcmp(jsettings->key, "adhoc-mode") == 0 ||
		 strcmp(jsettings->key, "name") == 0) {
		json_find_string(jparent, "adhoc-mode", &adhoc_mode);
		json_find_string(jparent, "adhoc-master", &adhoc_master);
		if(adhoc_mode != NULL && adhoc_master != NULL && strcmp(adhoc_mode, "server") == 0) {
			if(i > 0) {
				logprintf(LOG_ERR, "config setting \"adhoc-master\" can not be used when \"adhoc-mode\" is \"server\"");
			}

			return -1;
		}
	}
	return 0;
}

int hardware_validate_id(struct JsonNode *jhardware, int i) {
	struct hardware_t *tmp_hardware = hardware;
	int match = 0;
	while(tmp_hardware) {
		if(strcmp(jhardware->key, tmp_hardware->id) == 0) {
			match = 1;
			break;
		}
		tmp_hardware = tmp_hardware->next;
	}
	if(match == 0) {
		if(i > 0) {
			logprintf(LOG_ERR, "config hardware module #%d \"%s\" does not exist", i, jhardware->key);
		}
		return -1;
	}
	return 0;
}

int hardware_validate_duplicates(struct JsonNode *jhardware, int i) {
	struct JsonNode *jhardware1 = json_first_child(jhardware->parent);
	struct hardware_t *tmp_hardware = hardware;
	int x = 0, hwtype = -1;

	while(tmp_hardware) {
		if(strcmp(jhardware->key, tmp_hardware->id) == 0) {
			hwtype = tmp_hardware->hwtype;
			break;
		}
		tmp_hardware = tmp_hardware->next;
	}
	while(jhardware1) {
		x++;
		if(x > i) {
			if(strcmp(jhardware1->key, jhardware->key) == 0) {
				if(i > 0) {
					logprintf(LOG_ERR, "config hardware #%d \"%s\", duplicate", i, jhardware->key);
				}
				return -1;
			}
			tmp_hardware = hardware;
			while(tmp_hardware) {
				if(strcmp(jhardware1->key, tmp_hardware->id) == 0) {
					if(tmp_hardware->hwtype == hwtype) {
						if(i > 0) {
							logprintf(LOG_ERR, "config hardware module #%d \"%s\", duplicate freq.", i, jhardware1->key);
						}
					}
					break;
				}
				tmp_hardware = tmp_hardware->next;
			}
		}
		jhardware1 = jhardware1->next;
	}
	return 0;
}

int hardware_validate_settings(struct JsonNode *jhardware, int i) {
	struct JsonNode *jvalues = NULL;
	struct hardware_t *tmp_hardware = hardware;
	struct options_t *tmp_options = NULL;
	int match = 0;

	while(tmp_hardware) {
		if(strcmp(tmp_hardware->id, jhardware->key) == 0) {
			/* Check if all options required by the hardware module are present */
			tmp_options = tmp_hardware->options;
			while(tmp_options) {
				match = 0;
				jvalues = json_first_child(jhardware);
				while(jvalues) {
					if(jvalues->tag == JSON_NUMBER || jvalues->tag == JSON_STRING) {
						if(strcmp(jvalues->key, tmp_options->name) == 0 && tmp_options->argtype == OPTION_HAS_VALUE) {
							match = 1;
							break;
						}
					}
					jvalues = jvalues->next;
				}
				if(match == 0) {
					if(i > 0) {
						logprintf(LOG_ERR, "config hardware module #%d \"%s\", setting \"%s\" missing", i, jhardware->key, tmp_options->name);
					}
					return -1;
				} else {
					/* Check if setting contains a valid value */
#if !defined(__FreeBSD__) && !defined(_WIN32)
					regex_t regex;
					int reti = 0;
					char *stmp = NULL;

					if(jvalues->tag == JSON_NUMBER) {
						int l = snprintf(NULL, 0, "%d", (int)jvalues->number_);
						if(l < 2) {
							l = 2;
						}
						if((stmp = REALLOC(stmp, l+1)) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}
						snprintf(stmp, l, "%d", (int)jvalues->number_);
					} else if(jvalues->tag == JSON_STRING) {
						if((stmp = REALLOC(stmp, strlen(jvalues->string_)+1)) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}
						strcpy(stmp, jvalues->string_);
					}
					if(tmp_options->mask != NULL) {
						reti = regcomp(&regex, tmp_options->mask, REG_EXTENDED);
						if(reti) {
							if(i > 0) {
								logprintf(LOG_ERR, "could not compile regex");
							}
							return -1;
						}
						reti = regexec(&regex, stmp, 0, NULL, 0);
						if(reti == REG_NOMATCH || reti != 0) {
							if(i > 0) {
								logprintf(LOG_ERR, "config hardware module #%d \"%s\", setting \"%s\" invalid", i, jhardware->key, tmp_options->name);
							}
							regfree(&regex);
							FREE(stmp);
							return -1;
						}
						regfree(&regex);
					}
					FREE(stmp);
#endif
				}
				tmp_options = tmp_options->next;
			}

			/* Check for any settings that are not valid for this hardware module */
			jvalues = json_first_child(jhardware);
			while(jvalues) {
				match = 0;
				if(jvalues->tag == JSON_NUMBER || jvalues->tag == JSON_STRING) {
					tmp_options = tmp_hardware->options;
					while(tmp_options) {
						if(strcmp(jvalues->key, tmp_options->name) == 0 && jvalues->tag == tmp_options->vartype) {
							if(tmp_options->vartype == JSON_NUMBER) {
								options_set_number(&tmp_options, tmp_options->id, jvalues->number_);
							} else if(tmp_options->vartype == JSON_STRING) {
								options_set_string(&tmp_options, tmp_options->id, jvalues->string_);
							}
							match = 1;
							break;
						}
						tmp_options = tmp_options->next;
					}
					if(match == 0) {
						if(i > 0) {
							logprintf(LOG_ERR, "config hardware module #%d \"%s\", setting \"%s\" invalid", i, jhardware->key, jvalues->key);
						}
						return -1;
					}
				}
				jvalues = jvalues->next;
			}

			if(tmp_hardware->settings != NULL) {
				/* Sync all settings with the hardware module */
				jvalues = json_first_child(jhardware);
				while(jvalues) {
					if(tmp_hardware->settings(jvalues) == EXIT_FAILURE) {
						if(i > 0) {
							logprintf(LOG_ERR, "config hardware module #%d \"%s\", setting \"%s\" invalid", i, jhardware->key, jvalues->key);
						}
						return -1;
					}
					jvalues = jvalues->next;
				}
			}
		}
		tmp_hardware = tmp_hardware->next;
	}
	return 0;
}

void devices_import(struct JsonNode *jdevices) {
	int i = 0;
	json_clone(jdevices, &jdevices_cache);

	jdevices = json_first_child(jdevices);
	while(jdevices) {
		i++;
		devices_struct_parse(jdevices, i);

		jdevices = jdevices->next;
	}
}

int storage_devices_validate(struct JsonNode *jdevices) {
	int i = 0;
	jdevices = json_first_child(jdevices);
	while(jdevices) {
		i++;

		if(devices_validate_protocols(jdevices, i) == -1) { return -1; }

		devices_struct_parse(jdevices, i);

		if(devices_validate_name(jdevices, i) == -1) { return -1; }
		if(devices_validate_duplicates(jdevices, i) == -1) { return -1; }
		if(devices_validate_settings(jdevices, i) == -1) { return -1; }
		if(devices_validate_state(jdevices, i) == -1) { return -1; }
		if(devices_validate_values(jdevices, i) == -1) { return -1; }
		if(devices_validate_id(jdevices, i) == -1) { return -1; }

		devices_init_protocol_threads(jdevices, i);
		// devices_init_event_threads(jdevices, i);

		jdevices = jdevices->next;
	}
	return 0;
}

#ifdef EVENTS
int storage_rules_validate(struct JsonNode *jrules) {
	int i = 0;
	jrules = json_first_child(jrules);
	while(jrules) {
		i++;

		if(rules_validate_name(jrules, i) == -1) { return -1; }
		if(rules_validate_settings(jrules, i) == -1) { return -1; }
		if(rules_validate_duplicates(jrules, i) == -1) { return -1; }
		if(rules_struct_parse(jrules, i) == -1) { return -1; }

		jrules = jrules->next;
	}
	return 0;
}
#endif

int storage_gui_validate(struct JsonNode *jgui) {
	int i = 0;
	jgui = json_first_child(jgui);
	while(jgui) {
		i++;

		if(gui_validate_id(jgui, i) == -1) { return -1; }
		if(gui_validate_duplicates(jgui, i) == -1) { return -1; }
		if(gui_validate_settings(jgui, i) == -1) { return -1; }
		if(gui_validate_media(jgui, i) == -1) { return -1; }

		jgui = jgui->next;
	}
	return 0;
}

int storage_hardware_validate(struct JsonNode *jhardware) {
	int i = 0;
	jhardware = json_first_child(jhardware);
	while(jhardware) {
		i++;

		if(hardware_validate_id(jhardware, i) == -1) { return -1; }
		if(hardware_validate_duplicates(jhardware, i) == -1) { return -1; }
		if(hardware_validate_settings(jhardware, i) == -1) { return -1; }

		jhardware = jhardware->next;
	}
	return 0;
}

int storage_settings_validate(struct JsonNode *jsettings) {
	int i = 0;
	jsettings = json_first_child(jsettings);
	while(jsettings) {
		i++;

		if(settings_validate_duplicates(jsettings, i) == -1) { return -1; }
		if(settings_validate_settings(jsettings, i) == -1) { return -1; }
		if(settings_validate_values(jsettings, i) == -1) { return -1; }

		jsettings = jsettings->next;
	}
	return 0;
}

int storage_read(char *file, unsigned long objects) {
	struct JsonNode *json = NULL;
	if(storage == NULL) {
		return -1;
	}
	if(storage->read(file, objects) == -1) {
		logprintf(LOG_ERR, "failed to read configuration from %s data source", storage->id);
		return -1;
	}

	/*
	 * We have to validate the settings first to know the gpio-platform setting.
	 */
	if(((objects & CONFIG_SETTINGS) == CONFIG_SETTINGS || (objects & CONFIG_ALL) == CONFIG_ALL) &&
		storage->settings_select(ORIGIN_CONFIG, NULL, &json) == 0) {
		json_clone(json, &jsettings_cache);
		if(storage_settings_validate(jsettings_cache) == -1) {
			return -1;
		}
	}

	if(((objects & CONFIG_DEVICES) == CONFIG_DEVICES || (objects & CONFIG_ALL) == CONFIG_ALL) &&
		storage->devices_select(ORIGIN_CONFIG, NULL, &json) == 0) {
		json_clone(json, &jdevices_cache);
		if(storage_devices_validate(jdevices_cache) == -1) {
			return -1;
		}
	}

	if(((objects & CONFIG_GUI) == CONFIG_GUI || (objects & CONFIG_ALL) == CONFIG_ALL) &&
		storage->gui_select(ORIGIN_CONFIG, NULL, &json) == 0) {
		json_clone(json, &jgui_cache);
		if(storage_gui_validate(jgui_cache) == -1) {
			return -1;
		}
	}

	if(((objects & CONFIG_HARDWARE) == CONFIG_HARDWARE || (objects & CONFIG_ALL) == CONFIG_ALL) &&
		storage->hardware_select(ORIGIN_CONFIG, NULL, &json) == 0) {
		json_clone(json, &jhardware_cache);
		if(storage_hardware_validate(jhardware_cache) == -1) {
			return -1;
		}
	}

#ifdef EVENTS
	if(((objects & CONFIG_RULES) == CONFIG_RULES || (objects & CONFIG_ALL) == CONFIG_ALL) &&
		storage->rules_select(ORIGIN_CONFIG, NULL, &json) == 0) {
		json_clone(json, &jrules_cache);
		if(storage_rules_validate(jrules_cache) == -1) {
			return -1;
		}
	}
#endif

	if(((objects & CONFIG_REGISTRY) == CONFIG_REGISTRY || (objects & CONFIG_ALL) == CONFIG_ALL)) {
		if(storage->registry_select(ORIGIN_CONFIG, NULL, &json) != 0) {
			return -1;
		}
		json_clone(json, &jregistry_cache);
	}

	storage->sync();

	return 0;
}

static void *storage_import_thread(int reason, void *param) {
	struct JsonNode *jconfig = param;

	storage_import(jconfig);
	return NULL;
}

int storage_import(struct JsonNode *jconfig) {
	struct JsonNode *jdevices = NULL;
	struct JsonNode *jgui = NULL;
	struct JsonNode *jhardware = NULL;
#ifdef EVENTS
	struct JsonNode *jrules = NULL;
#endif
	struct JsonNode *jsettings = NULL;

	if((jdevices = json_find_member(jconfig, "devices")) != NULL) {
		if(storage_devices_validate(jdevices) == -1) {
			logprintf(LOG_DEBUG, "failed to import foreign config");
			return -1;
		}
		json_clone(jdevices, &jdevices_cache);
	}
#ifdef EVENTS
	if((jrules = json_find_member(jconfig, "rules")) != NULL) {
		if(storage_rules_validate(jrules) == -1) {
			logprintf(LOG_DEBUG, "failed to import foreign config");
			return -1;
		}
		json_clone(jrules, &jrules_cache);
	}
#endif
	if((jgui = json_find_member(jconfig, "gui")) != NULL) {
		if(storage_gui_validate(jgui) == -1) {
			logprintf(LOG_DEBUG, "failed to import foreign config");
			return -1;
		}
		json_clone(jgui, &jgui_cache);
	}

	if((jhardware = json_find_member(jconfig, "hardware")) != NULL) {
		if(storage_hardware_validate(jhardware) == -1) {
			logprintf(LOG_DEBUG, "failed to import foreign config");
			return -1;
		}
		json_clone(jhardware, &jhardware_cache);
	}

	if((jsettings = json_find_member(jconfig, "settings")) != NULL) {
		if(storage_settings_validate(jsettings) == -1) {
			logprintf(LOG_DEBUG, "failed to import foreign config");
			return -1;
		}
		json_clone(jsettings, &jsettings_cache);
	}
	logprintf(LOG_DEBUG, "foreign config successfully imported");
	return 0;
}

int storage_export(void) {
	if(storage != NULL) {
		return storage->sync();
	}
	return 0;
}

int devices_select(enum origin_t origin, char *id, struct JsonNode **jrespond) {
	struct JsonNode *tmp = jdevices_cache;
	if(tmp == NULL) {
		return -1;
	}
	if(id != NULL) {
		struct JsonNode *jchilds = json_first_child(tmp);
		while(jchilds) {
			if(strcmp(jchilds->key, id) == 0) {
				if(jrespond != NULL) {
					*jrespond = jchilds;
				}
				return 0;
			}
			jchilds = jchilds->next;
		}
		return -1;
	} else {
		*jrespond = tmp;
	}
	return 0;
}

int devices_select_struct(enum origin_t origin, char *id, struct device_t **dev) {
	struct device_t *nodes = device;
	while(nodes) {
		if(strcmp(id, nodes->id) == 0) {
			*dev = nodes;
			return 0;
		}
		nodes = nodes->next;
	}
	return -1;
}

int devices_select_string_setting(enum origin_t origin, char *id, char *setting, char **out) {
	struct JsonNode *tmp = NULL;
	char *_out = NULL;
	if(devices_select(origin, id, &tmp) == 0) {
		if(json_find_string(tmp, setting, &_out) == 0) {
			if(out != NULL) {
				*out = _out;
			}
			return 0;
		}
	}
	return -1;
}

int devices_select_number_setting(enum origin_t origin, char *id, char *setting, double *out, int *decimals) {
	struct JsonNode *tmp = NULL;
	struct JsonNode *tmp1 = NULL;
	if(devices_select(origin, id, &tmp) == 0) {
		if((tmp1 = json_find_member(tmp, setting)) != NULL) {
			if(out != NULL && decimals != NULL) {
				*out = tmp1->number_;
				if(decimals != NULL) {
					*decimals = tmp1->decimals_;
				}
			}
			return 0;
		}
	}
	return -1;
}

int devices_select_settings(enum origin_t origin, char *id, int i, char **setting, struct varcont_t *out) {
	struct JsonNode *tmp = NULL;
	struct JsonNode *tmp1 = NULL;
	struct protocol_t *protocol = NULL;
	/*
	 * FIXME: Only valid in local scope
	 */
	char *state = "state";
	int x = 0;
	int y = 0;

	if(devices_select(origin, id, &tmp) == 0) {
		while(devices_select_protocol(origin, id, x++, &protocol) == 0) {
			struct options_t *options = protocol->options;
			while(options) {
				if(options->conftype == DEVICES_SETTING || options->conftype == DEVICES_VALUE) {
					if((tmp1 = json_find_member(tmp, options->name)) != NULL) {
						if(y == i) {
							*setting = options->name;
							if(tmp1->tag == JSON_STRING) {
								out->string_ = tmp1->string_;
								out->type_ = JSON_STRING;
								return 0;
							} else if(tmp1->tag == JSON_NUMBER) {
								out->number_ = tmp1->number_;
								out->decimals_ = tmp1->decimals_;
								out->type_ = JSON_NUMBER;
								return 0;
							}
						}
						y++;
					}
				}
				options = options->next;
			}
			if((tmp1 = json_find_member(tmp, state)) != NULL) {
				if(y == i) {
					*setting = state;
					if(tmp1->tag == JSON_STRING) {
						out->string_ = tmp1->string_;
						out->type_ = JSON_STRING;
						return 0;
					} else if(tmp1->tag == JSON_NUMBER) {
						out->number_ = tmp1->number_;
						out->decimals_ = tmp1->decimals_;
						out->type_ = JSON_NUMBER;
						return 0;
					}
				}
				y++;
			}
		}
	}
	return -1;
}

int devices_select_protocol(enum origin_t origin, char *id, int element, struct protocol_t **out) {
	struct device_t *nodes = device;
	while(nodes) {
		if(strcmp(id, nodes->id) == 0) {
			if(nodes->nrprotocols > element) {
				*out = nodes->protocols[element];
				return 0;
			}
		}
		nodes = nodes->next;
	}
	return -1;
}

int devices_select_id(enum origin_t origin, char *id, int element, char **name, struct varcont_t *out) {
	struct JsonNode *jrespond = NULL;
	struct JsonNode *jsetting = NULL;
	struct JsonNode *jvalues = NULL;
	struct JsonNode *jid = NULL;
	int i = 0;
	if(devices_select(origin, id, &jrespond) == 0) {
		if((jsetting = json_find_member(jrespond, "id")) != NULL && jsetting->tag == JSON_ARRAY) {
			jid = json_first_child(jsetting);
			while(jid) {
				jvalues = json_first_child(jid);
				while(jvalues) {
					if(i == element) {
						if(jvalues->tag == JSON_STRING) {
							*name = jvalues->key;
							out->type_ = jvalues->tag;
							out->string_ = jvalues->string_;
							return 0;
						} else if(jvalues->tag == JSON_NUMBER) {
							*name = jvalues->key;
							out->type_ = jvalues->tag;
							out->number_ = jvalues->number_;
							out->decimals_ = jvalues->decimals_;
							return 0;
						}
					}
					i++;
					jvalues = jvalues->next;
				}
				jid = jid->next;
			}
		}
	}
	return -1;
}

#ifdef EVENTS
int rules_select(enum origin_t origin, char *id, struct JsonNode **jrespond) {
	struct JsonNode *tmp = jrules_cache;
	if(id != NULL) {
		struct JsonNode *jchilds = json_first_child(tmp);
		while(jchilds) {
			if(strcmp(jchilds->key, id) == 0) {
				if(jrespond != NULL) {
					*jrespond = jchilds;
				}
				return 0;
			}
			jchilds = jchilds->next;
		}
		return -1;
	} else {
		*jrespond = tmp;
	}
	return 0;
}

int rules_select_struct(enum origin_t origin, char *id, struct rules_t **out) {
	struct rules_t *tmp = rules;
	if(id != NULL) {
		while(tmp) {
			if(strcmp(tmp->name, id) == 0) {
				*out = tmp;
				return 0;
			}
			tmp = tmp->next;
		}
		return -1;
	} else {
		*out = tmp;
	}
	return 0;
}
#endif

int gui_select(enum origin_t origin, char *id, struct JsonNode **jrespond) {
	struct JsonNode *tmp = jgui_cache;

	if(id != NULL) {
		struct JsonNode *jchilds = json_first_child(tmp);
		while(jchilds) {
			if(strcmp(jchilds->key, id) == 0) {
				if(jrespond != NULL) {
					*jrespond = jchilds;
				}
				return 0;
			}
			jchilds = jchilds->next;
		}
		return -1;
	} else {
		*jrespond = tmp;
	}
	return 0;
}

int gui_check_media(enum origin_t origin, char *id, const char *media) {
	if(media == NULL || strcmp(media, "all") == 0) {
		return 0;
	}
	struct JsonNode *jrespond = NULL;
	struct JsonNode *jmedia = NULL;
	struct JsonNode *jchilds = NULL;
	if(gui_select(origin, id, &jrespond) == 0) {
		if((jmedia = json_find_member(jrespond, "media")) != NULL) {
			if(jmedia->tag == JSON_ARRAY) {
				jchilds = json_first_child(jmedia);
				while(jchilds) {
					if(jchilds->tag == JSON_STRING) {
						if(strcmp(media, jchilds->string_) == 0) {
							return 0;
						}
					}
					jchilds = jchilds->next;
				}
			}
		} else {
			return 0;
		}
	}
	return -1;
}

int hardware_select(enum origin_t origin, char *id, struct JsonNode **jrespond) {
	struct JsonNode *tmp = jhardware_cache;
	if(id != NULL) {
		struct JsonNode *jchilds = json_first_child(tmp);
		while(jchilds) {
			if(strcmp(jchilds->key, id) == 0) {
				if(jrespond != NULL) {
					*jrespond = jchilds;
				}
				return 0;
			}
			jchilds = jchilds->next;
		}
		return -1;
	} else {
		*jrespond = tmp;
	}
	return 0;
}

int hardware_select_struct(enum origin_t origin, char *id, struct hardware_t **out) {
	struct hardware_t *tmp = hardware;
	while(tmp) {
		if(strcmp(tmp->id, id) == 0) {
			*out = tmp;
			return 0;
		}
		tmp = tmp->next;
	}
	return -1;
}

int settings_select(enum origin_t origin, char *id, struct JsonNode **jrespond) {
	if(jsettings_cache == NULL) {
		return -1;
	}
	*jrespond = jsettings_cache;
	if(id != NULL) {
		struct JsonNode *jchilds = json_first_child(*jrespond);
		while(jchilds) {
			if(strcmp(jchilds->key, id) == 0) {
				*jrespond = jchilds;
				return 0;
			}
			jchilds = jchilds->next;
		}
		return -1;
	}
	return 0;
}

int settings_select_string(enum origin_t origin, char *id, char **out) {
	struct JsonNode *jrespond = NULL;
	if(settings_select(origin, id, &jrespond) == 0) {
		if(jrespond->tag == JSON_STRING) {
			*out = jrespond->string_;
			return 0;
		}
	}
	return -1;
}

int settings_select_number(enum origin_t origin, char *id, double *out) {
	struct JsonNode *jrespond = NULL;
	if(settings_select(origin, id, &jrespond) == 0) {
		if(jrespond->tag == JSON_NUMBER) {
			*out = jrespond->number_;
			return 0;
		}
	}
	return -1;
}

int settings_select_string_element(enum origin_t origin, char *id, int element, char **out) {
	struct JsonNode *jrespond = NULL;
	struct JsonNode *jelement = NULL;
	if(settings_select(origin, id, &jrespond) == 0) {
		if(jrespond->tag == JSON_ARRAY) {
			if((jelement = json_find_element(jrespond, element)) != NULL) {
				if(jelement->tag == JSON_STRING) {
					*out = jelement->string_;
					return 0;
				}
			}
		}
	}
	return -1;
}

int settings_select_number_element(enum origin_t origin, char *id, int element, double *out) {
	struct JsonNode *jrespond = NULL;
	struct JsonNode *jelement = NULL;
	if(settings_select(origin, id, &jrespond) == 0) {
		if(jrespond->tag == JSON_ARRAY) {
			if((jelement = json_find_element(jrespond, element)) != NULL) {
				if(jelement->tag == JSON_NUMBER) {
					*out = jelement->number_;
					return 0;
				}
			}
		}
	}
	return -1;
}

static int registry_get_value_recursive(struct JsonNode *root, const char *key, void **value, void **decimals, int type) {
	char *sub = strstr(key, ".");
	char *buff = MALLOC(strlen(key)+1);
	strcpy(buff, key);
	if(sub != NULL) {
		int pos = sub-key;
		buff[pos] = '\0';
	}
	struct JsonNode *member = json_find_member(root, buff);
	if(member != NULL) {
		if(member->tag == type) {
			if(type == JSON_NUMBER) {
				*value = (void *)&member->number_;
				*decimals = (void *)&member->decimals_;
			} else if(type == JSON_STRING) {
				*value = (void *)member->string_;
			}
			FREE(buff);
			return 0;
		} else if(member->tag == JSON_OBJECT) {
			if(sub != NULL) {
				int pos = sub-key;
				strcpy(buff, &key[pos+1]);
			}
			int ret = registry_get_value_recursive(member, buff, value, decimals, type);
			FREE(buff);
			return ret;
		}
	}
	FREE(buff);
	return -1;
}

int registry_select(enum origin_t origin, char *id, struct JsonNode **jrespond) {
	if(jregistry_cache == NULL) {
		return -1;
	}
	*jrespond = jregistry_cache;
	if(id != NULL) {
		struct JsonNode *jchilds = json_first_child(*jrespond);
		while(jchilds) {
			if(strcmp(jchilds->key, id) == 0) {
				*jrespond = jchilds;
				return 0;
			}
			jchilds = jchilds->next;
		}
		return -1;
	}
	return 0;
}

int registry_select_number(enum origin_t origin, char *id, double *out, int *decimals) {
	struct JsonNode *jregistry = NULL;
	if(registry_select(origin, NULL, &jregistry) == 0) {
		if(jregistry == NULL) {
			return -1;
		}
		void *p = NULL;
		void *q = NULL;
		int ret = registry_get_value_recursive(jregistry, id, &p, &q, JSON_NUMBER);
		if(ret == 0) {
			*out = *(double *)p;
			*decimals = *(int *)q;
		}
		return ret;
	}
	return -1;
}

int registry_select_string(enum origin_t origin, char *id, char **out) {
	struct JsonNode *jregistry = NULL;
	if(registry_select(origin, NULL, &jregistry) == 0) {
		if(jregistry == NULL) {
			return -1;
		}
		return registry_get_value_recursive(jregistry, id, (void *)out, NULL, JSON_STRING);
	}
	return -1;
}

static int registry_set_value_recursive(struct JsonNode *root, const char *key, char *svalue, double dvalue, int decimals, int type) {
	char *sub = strstr(key, ".");
	char *buff = MALLOC(strlen(key)+1);
	strcpy(buff, key);
	if(sub != NULL) {
		int pos = sub-key;
		buff[pos] = '\0';
	}
	struct JsonNode *member = json_find_member(root, buff);
	if(member != NULL) {
		if(member->tag == type) {
			if(type == JSON_NUMBER) {
				member->number_ = dvalue;
				member->decimals_ = decimals;
			} else if(type == JSON_STRING) {
				if((member->string_ = REALLOC(member->string_, strlen(svalue)+1)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				strcpy(member->string_, svalue);
			}
			FREE(buff);
			return 0;
		} else if(member->tag == JSON_OBJECT) {
			if(sub != NULL) {
				int pos = sub-key;
				strcpy(buff, &key[pos+1]);
			}
			int ret = registry_set_value_recursive(member, buff, svalue, dvalue, decimals, type);
			FREE(buff);
			return ret;
		}
	} else if(sub != NULL) {
		member = json_mkobject();
		json_append_member(root, buff, member);
		int pos = sub-key;
		strcpy(buff, &key[pos+1]);
		int ret = registry_set_value_recursive(member, buff, svalue, dvalue, decimals, type);
		FREE(buff);
		return ret;
	} else {
		if(type == JSON_NUMBER) {
			json_append_member(root, buff, json_mknumber(dvalue, decimals));
		} else if(type == JSON_STRING) {
			json_append_member(root, buff, json_mkstring(svalue));
		}
		FREE(buff);
		return 0;
	}
	FREE(buff);
	return -1;
}

static void registry_remove_empty_parent(struct JsonNode *root) {
	struct JsonNode *parent = root->parent;
	if(json_first_child(root) == NULL) {
		if(parent != NULL) {
			json_remove_from_parent(root);
			registry_remove_empty_parent(parent);
			json_delete(root);
		}
	}
}

static int registry_remove_value_recursive(struct JsonNode *root, const char *key) {
	char *sub = strstr(key, ".");
	char *buff = MALLOC(strlen(key)+1);
	strcpy(buff, key);
	if(sub != NULL) {
		int pos = sub-key;
		buff[pos] = '\0';
	}
	struct JsonNode *member = json_find_member(root, buff);
	if(member != NULL) {
		if(sub == NULL) {
			json_remove_from_parent(member);
			json_delete(member);
			registry_remove_empty_parent(root);
			FREE(buff);
			return 0;
		}
		if(member->tag == JSON_OBJECT) {
			if(sub != NULL) {
				int pos = sub-key;
				strcpy(buff, &key[pos+1]);
			}
			int ret = registry_remove_value_recursive(member, buff);
			FREE(buff);
			return ret;
		}
	}
	FREE(buff);
	return -1;
}

int registry_update(enum origin_t origin, const char *name, struct JsonNode *jvalues) {
	int i = 0;
	if(jvalues == NULL) {
		return -1;
	}
	if(jregistry_cache == NULL) {
		return -1;
	}
	if(jvalues->tag != JSON_NUMBER && jvalues->tag != JSON_STRING) {
		return -1;
	}
	i = registry_set_value_recursive(jregistry_cache, name, jvalues->string_, jvalues->number_, jvalues->decimals_, jvalues->tag);
	json_delete(jvalues);
	return i;
}

int registry_delete(enum origin_t origin, const char *key) {
	if(key == NULL) {
		return -1;
	}
	if(jregistry_cache == NULL) {
		return -1;
	}
	return registry_remove_value_recursive(jregistry_cache, key);
}

#ifdef EVENTS
int rules_gc(void) {
	struct rules_t *tmp_rules = NULL;
	struct rules_values_t *tmp_values = NULL;
	struct rules_actions_t *tmp_actions = NULL;
	int i = 0;

	while(rules) {
		tmp_rules = rules;
		FREE(tmp_rules->name);
		FREE(tmp_rules->rule);
		events_tree_gc(tmp_rules->tree);
		for(i=0;i<tmp_rules->nrdevices;i++) {
			FREE(tmp_rules->devices[i]);
		}
		while(tmp_rules->values) {
			tmp_values = tmp_rules->values;
			FREE(tmp_values->name);
			FREE(tmp_values->device);
			tmp_rules->values = tmp_rules->values->next;
			FREE(tmp_values);
		}
		if(tmp_rules->values != NULL) {
			FREE(tmp_rules->values);
		}
		while(tmp_rules->actions) {
			tmp_actions = tmp_rules->actions;
			if(tmp_actions->arguments != NULL) {
				json_delete(tmp_actions->arguments);
			}
			tmp_rules->actions = tmp_rules->actions->next;
			if(tmp_actions != NULL) {
				FREE(tmp_actions);
			}
		}
		if(tmp_rules->actions != NULL) {
			FREE(tmp_rules->actions);
		}
		if(tmp_rules->devices != NULL) {
			FREE(tmp_rules->devices);
		}
		rules = rules->next;
		FREE(tmp_rules);
	}
	if(rules != NULL) {
		FREE(rules);
	}
	rules = NULL;

	logprintf(LOG_DEBUG, "garbage collected config rules library");
	return 1;
}
#endif

int devices_gc(void) {
	struct device_t *tmp_device = NULL;
	int i = 0;

	while(device) {
		tmp_device = device;
		// event_action_thread_free(tmp_device);
		FREE(tmp_device->id);
		if(tmp_device->action_thread != NULL) {
			FREE(tmp_device->action_thread);
		}
		if(tmp_device->protocols != NULL) {
			FREE(tmp_device->protocols);
		}
		for(i=0;i<tmp_device->nrprotocols1;i++) {
			FREE(tmp_device->protocols1[i]);
		}
		if(tmp_device->protocols1 != NULL) {
			FREE(tmp_device->protocols1);
		}
		device = device->next;
		FREE(tmp_device);
	}
	if(device != NULL) {
		FREE(device);
	}
	return 0;
}

int storage_gc(void) {
	struct storage_t *tmp_storage = NULL;

	if(storage != NULL) {
		storage->sync();
		storage->gc();
	}

	if(jdevices_cache != NULL) {
		json_delete(jdevices_cache);
		jdevices_cache = NULL;
	}

	if(jrules_cache != NULL) {
		json_delete(jrules_cache);
		jrules_cache = NULL;
	}

	if(jgui_cache != NULL) {
		json_delete(jgui_cache);
		jgui_cache = NULL;
	}

	if(jsettings_cache != NULL) {
		json_delete(jsettings_cache);
		jsettings_cache = NULL;
	}

	if(jhardware_cache != NULL) {
		json_delete(jhardware_cache);
		jhardware_cache = NULL;
	}

	if(jregistry_cache != NULL) {
		json_delete(jregistry_cache);
		jregistry_cache = NULL;
	}

	devices_gc();
#ifdef EVENTS
	rules_gc();
#endif

	while(storage) {
		tmp_storage = storage;

		if(tmp_storage->gc != NULL) {
			tmp_storage->gc();
		}
		FREE(tmp_storage->id);
		storage = storage->next;
		FREE(tmp_storage);
	}
	if(storage != NULL) {
		FREE(storage);
	}

	logprintf(LOG_DEBUG, "garbage collected config storage library");
	return EXIT_SUCCESS;
}

struct JsonNode *config_print(int level, const char *media) {
	struct JsonNode *jroot = json_mkobject();

	if(jdevices_cache != NULL) {
		char *out = json_stringify(jdevices_cache, NULL);
		struct JsonNode *jdevices = json_decode(out);
		json_append_member(jroot, "devices", jdevices);
		json_free(out);

		struct JsonNode *jchilds = json_first_child(jdevices);
		if(level == CONFIG_INTERNAL) {
			struct JsonNode *jtimestamp = NULL;
			struct protocol_t *protocol = NULL;
			struct device_t *dev = NULL;
			char *dev_uuid = NULL;
			int i = 0;
			while(jchilds) {
				if(strlen(pilight_uuid) > 0) {
					if(json_find_string(jchilds, "uuid", &dev_uuid) != 0) {
						json_append_member(jchilds, "uuid", json_mkstring(pilight_uuid));
					}
					if(json_find_member(jchilds, "origin") == NULL) {
						json_append_member(jchilds, "origin", json_mkstring(pilight_uuid));
					}
				}
				if((jtimestamp = json_find_member(jchilds, "timestamp")) != NULL) {
					json_remove_from_parent(jtimestamp);
					json_delete(jtimestamp);
				}
				if(devices_select_struct(ORIGIN_CONFIG, jchilds->key, &dev) == 0) {
					json_append_member(jchilds, "timestamp", json_mknumber(dev->timestamp, 0));
				}
				i = 0;
				while(devices_select_protocol(ORIGIN_CONFIG, jchilds->key, i++, &protocol) == 0) {
					struct options_t *tmp_options = protocol->options;
					if(tmp_options) {
						while(tmp_options) {
							if(level == CONFIG_INTERNAL &&
							   (tmp_options->conftype == DEVICES_SETTING) &&
								 json_find_member(jchilds, tmp_options->name) == NULL) {
								if(tmp_options->vartype == JSON_NUMBER) {
									json_append_member(jchilds, tmp_options->name, json_mknumber((int)(intptr_t)tmp_options->def, 0));
								} else if(tmp_options->vartype == JSON_STRING) {
									json_append_member(jchilds, tmp_options->name, json_mkstring((char *)tmp_options->def));
								}
							}
							tmp_options = tmp_options->next;
						}
					}
				}
				jchilds = jchilds->next;
			}
		}

		/* Remove devices not relevant for this medium */
		struct JsonNode *tmp = NULL;
		int remove = 0;
		jchilds = json_first_child(jdevices);
		while(jchilds) {
			tmp = jchilds;
			remove = 0;
			if(gui_check_media(ORIGIN_CONFIG, jchilds->key, media) != 0) {
				remove = 1;
			}
			jchilds = jchilds->next;
			if(remove == 1) {
				json_remove_from_parent(tmp);
				json_delete(tmp);
			}
		}
	} else {
		json_append_member(jroot, "devices", json_mkobject());
	}
#ifdef EVENTS
	if(jrules_cache != NULL) {
		char *out = json_stringify(jrules_cache, NULL);
		json_append_member(jroot, "rules", json_decode(out));
		json_free(out);
	} else {
		json_append_member(jroot, "rules", json_mkobject());
	}
#endif
	if(jgui_cache != NULL) {
		char *out = json_stringify(jgui_cache, NULL);
		struct JsonNode *jgui = json_decode(out);
		json_append_member(jroot, "gui", jgui);
		json_free(out);

		struct JsonNode *jchilds = json_first_child(jgui);
		if(level == CONFIG_INTERNAL) {
			struct protocol_t *protocol = NULL;
			int i = 0, x = 0;
			while(jchilds) {
				x++;
				json_append_member(jchilds, "order", json_mknumber(x, 0));

				if(json_find_member(jchilds, "media") == NULL) {
					struct JsonNode *jmedia = json_mkarray();
					json_append_member(jchilds, "media", jmedia);
					json_append_element(jmedia, json_mkstring("all"));
				}

				i = 0;
				while(devices_select_protocol(ORIGIN_CONFIG, jchilds->key, i++, &protocol) == 0) {
					if(i == 1) {
						json_append_member(jchilds, "type", json_mknumber(protocol->devtype, 0));
					}
					struct options_t *tmp_options = protocol->options;
					if(tmp_options) {
						while(tmp_options) {
							if(level == CONFIG_INTERNAL &&
							   (tmp_options->conftype == GUI_SETTING) &&
								 json_find_member(jchilds, tmp_options->name) == NULL) {
								if(tmp_options->vartype == JSON_NUMBER) {
									json_append_member(jchilds, tmp_options->name, json_mknumber((int)(intptr_t)tmp_options->def, 0));
								} else if(tmp_options->vartype == JSON_STRING) {
									json_append_member(jchilds, tmp_options->name, json_mkstring((char *)tmp_options->def));
								}
							}
							tmp_options = tmp_options->next;
						}
					}
				}
				jchilds = jchilds->next;
			}
		}

		/* Remove devices not relevant for this medium */
		jchilds = json_first_child(jgui);
		struct JsonNode *tmp = NULL;
		int match = 0;
		while(jchilds) {
			match = 0;
			tmp = jchilds;
			if(gui_check_media(ORIGIN_CONFIG, jchilds->key, media) != 0) {
				match = 1;
				json_remove_from_parent(jchilds);
			}
			jchilds = jchilds->next;
			if(match == 1) {
				json_delete(tmp);
			}
		}
	} else {
		json_append_member(jroot, "gui", json_mkobject());
	}
	if(jsettings_cache != NULL) {
		char *out = json_stringify(jsettings_cache, NULL);
		json_append_member(jroot, "settings", json_decode(out));
		json_free(out);
	} else {
		json_append_member(jroot, "settings", json_mkobject());
	}
	if(jhardware_cache != NULL) {
		char *out = json_stringify(jhardware_cache, NULL);
		json_append_member(jroot, "hardware", json_decode(out));
		json_free(out);
	} else {
		json_append_member(jroot, "hardware", json_mkobject());
	}
	if(jregistry_cache != NULL) {
		char *out = json_stringify(jregistry_cache, NULL);
		json_append_member(jroot, "registry", json_decode(out));
		json_free(out);
	} else {
		json_append_member(jroot, "registry", json_mkobject());
	}
	return jroot;
}

struct JsonNode *values_print(const char *media) {
	struct JsonNode *jroot = json_mkarray();
	struct JsonNode *jdevices = NULL;
	struct JsonNode *jchilds = NULL;
	struct protocol_t *protocol = NULL;
	struct varcont_t val;
	char *setting = NULL;
	int i = 0;

	if(devices_select(ORIGIN_CONFIG, NULL, &jdevices) == 0) {
		if((jchilds = json_first_child(jdevices)) != NULL) {
			while(jchilds) {
				struct JsonNode *jdevice = json_mkobject();

				if(devices_select_protocol(ORIGIN_CONFIG, jchilds->key, 0, &protocol) == 0) {
					json_append_member(jdevice, "type", json_mknumber(protocol->devtype, 0));
				}

				struct JsonNode *jnames = json_mkarray();
				json_append_element(jnames, json_mkstring(jchilds->key));
				json_append_member(jdevice, "devices", jnames);

				i = 0;
				struct JsonNode *jvalues = json_mkobject();
				while(devices_select_settings(ORIGIN_CONFIG, jchilds->key, i++, &setting, &val) == 0) {
					if(val.type_ == JSON_NUMBER) {
						json_append_member(jvalues, setting, json_mknumber(val.number_, val.decimals_));
					} else if(val.type_ == JSON_STRING) {
						json_append_member(jvalues, setting, json_mkstring(val.string_));
					}
				}
				json_append_member(jdevice, "values", jvalues);

				json_append_element(jroot, jdevice);
				jchilds = jchilds->next;
			}
		}
	}
	return jroot;
}

/*
 *
 */
int devices_get_type(enum origin_t origin, char *id, int element, int *out) {
	struct device_t *nodes = device;
	while(nodes) {
		if(strcmp(id, nodes->id) == 0) {
			if(nodes->nrprotocols1 > element) {
				if(protocol_get_type(nodes->protocols1[element], out) == 0) {
					return 0;
				}
			}
		}
		nodes = nodes->next;
	}
	return -1;
}

int devices_get_state(enum origin_t origin, char *id, int element, char **out) {
	struct device_t *nodes = device;
	while(nodes) {
		if(strcmp(id, nodes->id) == 0) {
			if(nodes->nrprotocols1 > element) {
				if(protocol_get_state(nodes->protocols1[element], out) == 0) {
					return 0;
				}
			}
		}
		nodes = nodes->next;
	}
	return -1;
}

int devices_is_state(enum origin_t origin, char *id, int element, char *in) {
	struct device_t *nodes = device;
	while(nodes) {
		if(strcmp(id, nodes->id) == 0) {
			if(nodes->nrprotocols1 > element) {
				if(protocol_is_state(nodes->protocols1[element], in) == 0) {
					return 0;
				}
			}
		}
		nodes = nodes->next;
	}
	return -1;
}

int devices_get_value(enum origin_t origin, char *id, int element, char *in, char **out) {
	struct device_t *nodes = device;
	while(nodes) {
		if(strcmp(id, nodes->id) == 0) {
			if(nodes->nrprotocols1 > element) {
				if(protocol_get_value(nodes->protocols1[element], in, out) == 0) {
					return 0;
				}
			}
		}
		nodes = nodes->next;
	}
	return -1;
}

int devices_get_string_setting(enum origin_t origin, char *id, int element, char *in, char **out) {
	struct device_t *nodes = device;
	while(nodes) {
		if(strcmp(id, nodes->id) == 0) {
			if(nodes->nrprotocols1 > element) {
				if(protocol_get_string_setting(nodes->protocols1[element], in, out) == 0) {
					return 0;
				}
			}
		}
		nodes = nodes->next;
	}
	return -1;
}

int devices_get_number_setting(enum origin_t origin, char *id, int element, char *in, int *out) {
	struct device_t *nodes = device;
	while(nodes) {
		if(strcmp(id, nodes->id) == 0) {
			if(nodes->nrprotocols1 > element) {
				if(protocol_get_number_setting(nodes->protocols1[element], in, out) == 0) {
					return 0;
				}
			}
		}
		nodes = nodes->next;
	}
	return -1;
}

int devices_has_parameter(enum origin_t origin, char *id, int element, char *in) {
	struct device_t *nodes = device;
	while(nodes) {
		if(strcmp(id, nodes->id) == 0) {
			if(nodes->nrprotocols1 > element) {
				if(protocol_has_parameter(nodes->protocols1[element], in) == 0) {
					return 0;
				}
			}
		}
		nodes = nodes->next;
	}
	return -1;
}
