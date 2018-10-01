/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

#include "../../../lirc/config_file.h"
#include "../../../lirc/transmit.h"
#include "../../../lirc/ir_remote_types.h"
#include "../../../lirc/ir_remote.h"
#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../../core/binary.h"
#include "../../config/config.h"
#include "../protocol.h"
#include "generic.h"

#define IR_REMOTES_ROOT "/etc/pilight/ir-remotes/"

struct ir_keys_t {
	char *name;
	unsigned int id;
	int length;
	int nrfalse;
	int raw[MAXPULSESTREAMLENGTH];
	struct ir_keys_t *next;
} ir_keys;

struct ir_remotes_t {
	char *name;
	int toggle_bit_mask;
	int bits;
	int flags;
	int phead, shead;
	int pone, sone;
	int pzero, szero;
	int pre_data;
	int pre_data_bits;
	int ptrail;
	int rc6_mask;
	struct ir_keys_t *keys;
	struct ir_remotes_t *next;
} ir_remotes;

struct ir_remotes_t *remotes = NULL;
struct ir_remote *rs = NULL;

static int validate(void) {
	if(generic_ir->rawlen >= generic_ir->minrawlen && generic_ir->rawlen <= generic_ir->maxrawlen) {
		return 0;
	}

	return -1;
}

static int validateHead(struct ir_remotes_t *remote, int *bit) {
	*bit = 2;
	if(((double)generic_ir->raw[0]/(double)remote->phead) > 0.75 &&
	   ((double)generic_ir->raw[1]/(double)remote->shead) > 0.75) {
		return 0;
	}
	return -1;
}

static int validatePreDataMC(struct ir_remotes_t *remote, int *bit, int *state) {
	int x = 0, pre_data = 0, decBitCount = 0;

	*state = 0;
	x = *bit;

	while(decBitCount < remote->pre_data_bits) {
		if((double)generic_ir->raw[x + 1] / (double)remote->pone < 1.5) {
			pre_data |= *state << ((remote->pre_data_bits - 1) - decBitCount);
			x += 2;
		} else if((double)generic_ir->raw[x + 1] / (double)remote->pone > 1.5) {
			pre_data |= *state << ((remote->pre_data_bits - 1) - decBitCount);
			*state ^= 1;
			x += 1;
		}
		decBitCount++;
	}

	*bit = x;

	if(pre_data == remote->pre_data || ((pre_data ^ (remote->toggle_bit_mask >> remote->bits)) == remote->pre_data)) {
		return -1;
	}
	return 0;
}

static int validatePreData(struct ir_remotes_t *remote, int *bit) {
	int x = 0, pre_data = 0;
	for(x=*bit;x<*bit+remote->pre_data_bits*2;x+=2) {
		if((double)generic_ir->raw[x]/(double)remote->pone > 0.75 && ((double)generic_ir->raw[x+1]/(double)remote->sone) > 0.75) {
			pre_data |= 1 << ((remote->pre_data_bits-1)-((x/2)-(*bit/2)));
		} else if((double)generic_ir->raw[x]/(double)remote->pzero > 0.75 && ((double)generic_ir->raw[x+1]/(double)remote->szero) > 0.75) {
			pre_data &= ~(1 << ((remote->pre_data_bits-1)-((x/2)-(*bit/2))));
		} else {
			// Invalid code
			break;
		}
	}

	*bit = x;
	if(pre_data == remote->pre_data) {
		return 0;
	}
	return -1;
}

static int validateDataMC(struct ir_remotes_t *remote, struct ir_keys_t *key, int *bit, int *state) {
	int x = 0, keycode = 0, decBitCount = 0;
	x = *bit;

	while(decBitCount < remote->bits) {
		if((double)generic_ir->raw[x + 1] / (double)remote->pone < 1.5) {
			keycode |= *state << ((remote->bits - 1) - decBitCount);
			x += 2;
		} else if((double)generic_ir->raw[x + 1] / (double)remote->pone > 1.5) {
			keycode |= *state << ((remote->bits - 1) - decBitCount);
			*state ^= 1;
			x += 1;
		}
		decBitCount++;
	}

	*bit = x;

	if((int)key->id == keycode) {
		return 0;
	}
	return -1;
}

static int validateData(struct ir_remotes_t *remote, struct ir_keys_t *key, int *bit) {
	int x = 0, keycode = 0;
	for(x=*bit;x<*bit+remote->bits*2;x+=2) {
		if(((double)generic_ir->raw[x]/(double)remote->pone > 0.75) && ((double)generic_ir->raw[x+1]/(double)remote->sone) > 0.75) {
			keycode |= 1 << ((remote->bits-1)-((x/2)-(*bit/2)));
		} else if(((double)generic_ir->raw[x]/(double)remote->pzero > 0.75) && ((double)generic_ir->raw[x+1]/(double)remote->szero) > 0.75) {
			keycode &= ~(1 << ((remote->bits-1)-((x/2)-(*bit/2))));
		} else {
			// Invalid code
			break;
		}
	}
	x = *bit;

	if((int)key->id == keycode) {
		return 0;
	}
	return -1;
}

static void parseCode(char **message) {
	struct ir_remotes_t *tmp_remotes = remotes;
	struct ir_remote t;
	struct ir_keys_t *tmp_keys = NULL;
	int bit = 0;
	int state = 0;
	int matches = 0;
	int match = 0;

	// int i = 0;
	// for(i=0;i<generic_ir->rawlen;i++) {
		// printf("%d ", generic_ir->raw[i]);
	// }
	// printf("\n");
	while(tmp_remotes) {
		t.flags = tmp_remotes->flags;
		t.rc6_mask = tmp_remotes->rc6_mask;
		tmp_keys = tmp_remotes->keys;
		while(tmp_keys) {
			match = 0;
			matches = 0;
			bit = 0;
			state = 0;
			if(is_space_enc(&t)) {
				matches++;
				if(tmp_keys->length == (generic_ir->rawlen-1)) {
					match++;
				}
			}
			if(tmp_remotes->phead > 0 && tmp_remotes->shead > 0) {
				matches++;
				if(validateHead(tmp_remotes, &bit) == 0) {
					match++;
				}
			}
			if(is_rc6(&t) || is_rc5(&t)) {
				// logprintf(LOG_NOTICE, "RC6 and RC5 type remotes are not supported yet");
				if(tmp_remotes->pre_data > 0) {
					matches++;
					if(validatePreDataMC(tmp_remotes, &bit, &state) == 0) {
						match++;
					}
				}
				if(tmp_remotes->bits > 0) {
					matches++;
					if(validateDataMC(tmp_remotes, tmp_keys, &bit, &state) == 0) {
						match++;
					}
				}
				if(tmp_remotes->ptrail > 0 && bit > 0) {
					matches++;
				}
			} else {
				if(tmp_remotes->pre_data > 0) {
					matches++;
					if(validatePreData(tmp_remotes, &bit) == 0) {
						match++;
					}
				}
				if(tmp_remotes->bits > 0) {
					matches++;
					if(validateData(tmp_remotes, tmp_keys, &bit) == 0) {
						match++;
					}
				}
				if(tmp_remotes->ptrail > 0 && bit > 0) {
					matches++;
				}
			}
			// printf("bit: %d\n", bit);
			// printf("ptrail: %d %d %f\n", generic_ir->raw[bit], tmp_remotes->ptrail, ((double)generic_ir->raw[bit]/(double)tmp_remotes->ptrail));
			if(bit > 0 && ((double)generic_ir->raw[bit]/(double)tmp_remotes->ptrail) > 0.75) {
				match++;
			}
			if(match == matches && match > 0 && matches > 0) {
				printf("%s\n", tmp_keys->name);
			}
			tmp_keys = tmp_keys->next;
		}
		tmp_remotes = tmp_remotes->next;
	}
}

static void loadFiles(void) {
	struct ir_remote *r = NULL;
	struct ir_ncode *c = NULL;
	struct dirent *file = NULL;
	struct stat s;
	char path[PATH_MAX], *ir_remotes_root = NULL;
	int i = 0;
	DIR *d = NULL;
	FILE *fp = NULL;

	if(config_setting_get_string("protocol-root", 0, &ir_remotes_root) != 0) {
		/* If no protocol root was set, use the default protocol root */
		if((ir_remotes_root = MALLOC(strlen(IR_REMOTES_ROOT)+1)) == NULL) {
			fprintf(stderr, "out of memory\n");
			exit(EXIT_FAILURE);
		}
		strcpy(ir_remotes_root, IR_REMOTES_ROOT);
	}
	size_t len = strlen(ir_remotes_root);
	if(ir_remotes_root[len-1] != '/') {
		strcat(ir_remotes_root, "/");
	}

	if((d = opendir(ir_remotes_root))) {
		while((file = readdir(d)) != NULL) {
			memset(path, '\0', PATH_MAX);
			sprintf(path, "%s%s", ir_remotes_root, file->d_name);
			if(stat(path, &s) == 0) {
				/* Check if file */
				if(S_ISREG(s.st_mode)) {
					if((fp = fopen(path, "rb")) == NULL) {
						logprintf(LOG_ERR, "cannot read ir-remotes file: %s", path);
						continue;
					}
					rs = read_config(fp, path);
					fclose(fp);
					if(rs == (void *)-1) {
						logprintf(LOG_NOTICE, "cannot read ir-remotes file: %s", path);
						continue;
					}
					if(rs == NULL) {
						logprintf(LOG_NOTICE, "config file contains no valid remote control definition: %s", path);
						continue;
					}

					for(r = rs; r != NULL; r = r->next) {
						struct ir_remotes_t *node = MALLOC(sizeof(struct ir_remotes_t));
						if(node == NULL) {
							fprintf(stderr, "out of memory\n");
							exit(EXIT_FAILURE);
						}
						if((node->name = MALLOC(strlen(r->name)+1)) == NULL) {
							fprintf(stderr, "out of memory\n");
							exit(EXIT_FAILURE);
						}
						strcpy(node->name, r->name);

						node->flags = rs->flags;
						node->toggle_bit_mask = rs->toggle_bit_mask;
						node->bits = rs->bits;
						node->pone = rs->pone;
						node->sone = rs->sone;
						node->pzero = rs->pzero;
						node->szero = rs->szero;
						node->ptrail = rs->ptrail;
						node->phead = rs->phead;
						node->shead = rs->shead;
						node->rc6_mask = rs->rc6_mask;
						node->pre_data = rs->pre_data;
						node->pre_data_bits = rs->pre_data_bits;

						node->keys = NULL;
						for(c = r->codes; c->name; c++) {
							int success = init_send(r, c);
							if(success == 1) {
								struct ir_keys_t *key = MALLOC(sizeof(struct ir_keys_t));
								if(key == NULL) {
									fprintf(stderr, "out of memory\n");
									exit(EXIT_FAILURE);
								}
								if((key->name = MALLOC(strlen(c->name)+1)) == NULL) {
									fprintf(stderr, "out of memory\n");
									exit(EXIT_FAILURE);
								}
								strcpy(key->name, c->name);
								key->next = NULL;
								key->id = c->code;
								key->length = send_buffer.wptr;
								if(key->length < generic_ir->minrawlen) {
									generic_ir->minrawlen = key->length-1;
								}
								if(key->length > generic_ir->maxrawlen) {
									generic_ir->maxrawlen = key->length+1;
								}
								for(i=0;i<send_buffer.wptr;i++) {
									key->raw[i] = send_buffer.data[i];
									if(i == MAXPULSESTREAMLENGTH) {
										logprintf(LOG_NOTICE, "ir-remote raw code too long: %s", r->name);
										break;
									}
								}
								struct ir_keys_t *tmp_keys = node->keys;
								if(tmp_keys) {
									while(tmp_keys->next != NULL) {
										tmp_keys = tmp_keys->next;
									}
									tmp_keys->next = key;
								} else {
									key->next = node->keys;
									node->keys = key;
								}
							}
						}
						node->next = remotes;
						remotes = node;
					}
					free_config(rs);
				}
			} else {
				perror("stat");
			}
		}
		closedir(d);
	}
	if(ir_remotes_root != NULL) {
		FREE(ir_remotes_root);
	}
}

static void gc(void) {
	struct ir_remotes_t *r = NULL;
	struct ir_keys_t *k = NULL;
	while(remotes) {
		r = remotes;
		while(remotes->keys) {
			k = remotes->keys;
			FREE(remotes->keys->name);
			remotes->keys = remotes->keys->next;
			FREE(k);
		}
		if(remotes->keys) {
			FREE(remotes->keys);
		}
		FREE(remotes->name);
		remotes = remotes->next;
		FREE(r);
	}
	if(remotes != NULL) {
		FREE(remotes);
	}
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void genericIRInit(void) {
	protocol_register(&generic_ir);
	protocol_set_id(generic_ir, "generic_ir");
	protocol_device_add(generic_ir, "generic_ir", "Generic IR Parser");
	generic_ir->devtype = LIRC;
	generic_ir->hwtype = RFIR;
	generic_ir->minrawlen = 1000;
	generic_ir->maxrawlen = 0;
	generic_ir->mingaplen = 10000;
	generic_ir->maxgaplen = 100000;

	options_add(&generic_ir->options, "c", "code", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&generic_ir->options, "a", "repeat", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, "[0-9]");
	options_add(&generic_ir->options, "b", "button", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&generic_ir->options, "r", "remote", OPTION_HAS_VALUE, DEVICES_ID, JSON_STRING, NULL, NULL);

	generic_ir->parseCode=&parseCode;
	generic_ir->validate=&validate;
	generic_ir->gc=&gc;

	loadFiles();
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "generic_ir";
	module->version = "3.0";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	genericIRInit();
}
#endif
