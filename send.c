/*
	Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#ifndef _WIN32
#include <wiringx.h>
#endif

#include "libs/pilight/core/threads.h"
#include "libs/pilight/core/pilight.h"
#include "libs/pilight/core/common.h"
#include "libs/pilight/core/log.h"
#include "libs/pilight/core/options.h"
#include "libs/pilight/core/socket.h"
#include "libs/pilight/core/json.h"
#include "libs/pilight/core/ssdp.h"
#include "libs/pilight/core/dso.h"
#include "libs/pilight/config/config.h"

#include "libs/pilight/protocols/protocol.h"

typedef struct pname_t {
	char *name;
	char *desc;
	struct pname_t *next;
} pname_t;

static struct pname_t *pname = NULL;

static void sort_list(void) {
	struct pname_t *a = NULL;
	struct pname_t *b = NULL;
	struct pname_t *c = NULL;
	struct pname_t *e = NULL;
	struct pname_t *tmp = NULL;

	while(pname && e != pname->next) {
		c = a = pname;
		b = a->next;
		while(a != e) {
			if(strcmp(a->name, b->name) > 0) {
				if(a == pname) {
					tmp = b->next;
					b->next = a;
					a->next = tmp;
					pname = b;
					c = b;
				} else {
					tmp = b->next;
					b->next = a;
					a->next = tmp;
					c->next = b;
					c = b;
				}
			} else {
				c = a;
				a = a->next;
			}
			b = a->next;
			if(b == e)
				e = a;
		}
	}
}

int main(int argc, char **argv) {
	// memtrack();

	const uv_thread_t pth_cur_id = uv_thread_self();
	memcpy((void *)&pth_main_id, &pth_cur_id, sizeof(uv_thread_t));

	atomicinit();

	log_file_disable();
	log_shell_enable();
	log_level_set(LOG_NOTICE);

	if((progname = MALLOC(13)) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	strcpy(progname, "pilight-send");

	struct options_t *options = NULL;
	struct ssdp_list_t *ssdp_list = NULL;

	int sockfd = 0;
	int raw[MAXPULSESTREAMLENGTH-1];
	char *recvBuff = NULL;

	char *protobuffer = NULL;
	int match = 0;

	int help = 0;
	int version = 0;
	int protohelp = 0;

	char *uuid = NULL;
	char *server = NULL;
	int port = 0;

	/* Hold the final protocol struct */
	struct protocol_t *protocol = NULL;
	JsonNode *code = NULL;

	options_add(&options, "H", "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "V", "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "p", "protocol", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "S", "server", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
	options_add(&options, "P", "port", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "[0-9]{1,4}");
	options_add(&options, "I", "instance", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "U", "uuid", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "[a-zA-Z0-9]{4}-[a-zA-Z0-9]{2}-[a-zA-Z0-9]{2}-[a-zA-Z0-9]{2}-[a-zA-Z0-9]{6}");
	options_add(&options, "Ls", "storage-root", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "[0-9]{1,4}");

	log_shell_enable();
	if(argc == 1) {
		help = 1;
	}
	if(options_parse(options, argc, argv, 0) == -1) {
		// printf("Usage: %s -p protocol [options]\n", progname);
		// goto close;
		// help = 1;
	}
	log_shell_enable();

	if(options_exists(options, "H") == 0) {
		help = 1;
	}

	if(options_exists(options, "V") == 0) {
		version = 1;
	}

	// if(options_exists(options, "C") == 0) {
		// options_get_string(options, "C", &configtmp);
	// }

	// if(options_exists(options, "Ls") == 0) {
		// char *arg = NULL;
		// options_get_string(options, "Ls", &arg);
		// if(config_root(arg) == -1) {
			// logprintf(LOG_ERR, "%s is not valid storage lua modules path", arg);
			// goto close;
		// }
	// }

	if(options_exists(options, "p") == 0) {
		options_get_string(options, "p", &protobuffer);
	}

	if(options_exists(options, "S") == 0) {
		options_get_string(options, "S", &server);
	}

	if(options_exists(options, "P") == 0) {
		options_get_number(options, "P", &port);
	}

	if(options_exists(options, "U") == 0) {
		options_get_string(options, "U", &uuid);
	};

	plua_init();
	/* Initialize protocols */
	protocol_init();

	/* Check if a protocol was given */
	if(protobuffer != NULL && strlen(protobuffer) > 0 && strcmp(protobuffer, "-V") != 0) {
		if(strlen(protobuffer) > 0 && version) {
			logprintf(LOG_NOTICE, "-p and -V cannot be combined");
		} else {
			struct protocols_t *pnode = protocols;
			/* Retrieve the used protocol */
			while(pnode) {
				/* Check if the protocol exists */
				protocol = pnode->listener;
				if(protocol_device_exists(protocol, protobuffer) == 0 && match == 0 && protocol->createCode != NULL) {
					match=1;
					/* Check if the protocol requires specific CLI arguments
					   and merge them with the main CLI arguments */
					if(protocol->options != NULL && help == 0) {
						options_merge(&options, &protocol->options);
					} else if(help == 1) {
						protohelp=1;
					}
					break;
				}
				pnode = pnode->next;
			}
			/* If no protocols matches the requested protocol */
			if(match == 0) {
				logprintf(LOG_ERR, "this protocol is not supported or doesn't support sending");
				goto close;
			}
		}
	}

	if(options_parse(options, argc, argv, 1) == -1) {
		help = 1;
	}

	/* Display help or version information */
	if(version == 1) {
		printf("%s v%s\n", progname, PILIGHT_VERSION);
		goto close;
	} else if(help == 1 || protohelp == 1 || match == 0) {
		if(protohelp == 1 && match == 1 && protocol->printHelp != NULL) {
			printf("Usage: %s -p %s [options]\n", progname, protobuffer);
		} else {
			printf("Usage: %s -p protocol [options]\n", progname);
		}
		if(help == 1) {
			printf("\t -H  --help\t\t\tdisplay this message\n");
			printf("\t -V  --version\t\t\tdisplay version\n");
			printf("\t -p  --protocol=protocol\tthe protocol that you want to control\n");
			printf("\t -S  --server=x.x.x.x\t\tconnect to server address\n");
			printf("\t -P  --port=xxxx\t\tconnect to server port\n");
			printf("\t -C  --config\t\t\tconfig file\n");
			printf("\t -U  --uuid=xxx-xx-xx-xx-xxxxxx\tUUID\n");
			// printf("\t -Ls --storage-root=xxxx\tlocation of storage lua modules\n");
		}
		if(protohelp == 1 && match == 1 && protocol->printHelp) {
			printf("\n\t[%s]\n", protobuffer);
			protocol->printHelp();
		} else {
			printf("\nThe supported protocols are:\n");
			struct protocols_t *pnode = protocols;
			/* Retrieve the used protocol */
			while(pnode) {
				protocol = pnode->listener;
				if(protocol->createCode != NULL) {
					struct protocol_devices_t *tmpdev = protocol->devices;
					while(tmpdev) {
						struct pname_t *node = MALLOC(sizeof(struct pname_t));
						if(node == NULL) {
							OUT_OF_MEMORY
						}
						if((node->name = MALLOC(strlen(tmpdev->id)+1)) == NULL) {
							OUT_OF_MEMORY
						}
						strcpy(node->name, tmpdev->id);
						if((node->desc = MALLOC(strlen(tmpdev->desc)+1)) == NULL) {
							OUT_OF_MEMORY
						}
						strcpy(node->desc, tmpdev->desc);
						node->next = pname;
						pname = node;
						tmpdev = tmpdev->next;
					}
				}
				pnode = pnode->next;
			}
			sort_list();
			struct pname_t *ptmp = NULL;
			while(pname) {
				ptmp = pname;
				printf("\t %s\t\t",ptmp->name);
				if(strlen(ptmp->name) < 7) {
					printf("\t");
				}
				if(strlen(ptmp->name) < 15) {
					printf("\t");
				}
				printf("%s\n", ptmp->desc);
				FREE(ptmp->name);
				FREE(ptmp->desc);
				pname = pname->next;
				FREE(ptmp);
			}
			FREE(pname);
		}
		goto close;
	}

	code = json_mkobject();
	struct options_t *tmp = options;
	char *id = NULL;
	while(tmp) {
		if(strlen(tmp->name) > 0) {
			/* Only send the CLI arguments that belong to this protocol, the protocol name
			and those that are called by the user */
			if(options_get_id_by_name(protocol->options, tmp->name, &id) == 0) {
				if(strcmp(id, "0") != 0 && (options_exists(options, id) == 0)) {
					if(tmp->vartype == JSON_STRING && tmp->string_ != NULL && strlen(tmp->string_) > 0) {
						if(isNumeric(tmp->string_) == 0) {
							json_append_member(code, tmp->name, json_mknumber(atof(tmp->string_), nrDecimals(tmp->string_)));
						} else {
							json_append_member(code, tmp->name, json_mkstring(tmp->string_));
						}
					} else if(tmp->vartype == JSON_NUMBER) {
						json_append_member(code, tmp->name, json_mknumber(tmp->number_, 0));
					} else if(tmp->vartype == JSON_NULL) {
						json_append_member(code, tmp->name, json_mknumber(1, 0));
					}
				}
			}
				if(strcmp(tmp->name, "protocol") == 0 && tmp->string_ != NULL) {
					JsonNode *jprotocol = json_mkarray();
					json_append_element(jprotocol, json_mkstring(tmp->string_));
					json_append_member(code, "protocol", jprotocol);
				}
		}
		tmp = tmp->next;
	}

	memset(raw, 0, sizeof(int)*(MAXPULSESTREAMLENGTH-1));
	protocol->raw = raw;

	if(protocol->createCode(code) == 0) {
		if(protocol->message) {
			json_delete(protocol->message);
		}
		if(server && port > 0) {
			if((sockfd = socket_connect(server, port)) == -1) {
				logprintf(LOG_ERR, "could not connect to pilight-daemon");
				goto close;
			}
		} else if(ssdp_seek(&ssdp_list) == -1) {
			logprintf(LOG_NOTICE, "no pilight ssdp connections found");
			goto close;
		} else {
			if((sockfd = socket_connect(ssdp_list->ip, ssdp_list->port)) == -1) {
				logprintf(LOG_ERR, "could not connect to pilight-daemon");
				goto close;
			}
		}
		if(ssdp_list) {
			ssdp_free(ssdp_list);
		}

		socket_write(sockfd, "{\"action\":\"identify\"}");
		if(socket_read(sockfd, &recvBuff, 0) != 0
		   || strcmp(recvBuff, "{\"status\":\"success\"}") != 0) {
			goto close;
		}

		JsonNode *json = json_mkobject();
		json_append_member(json, "action", json_mkstring("send"));
		if(uuid != NULL) {
			json_append_member(code, "uuid", json_mkstring(uuid));
		}
		json_append_member(json, "code", code);
		char *output = json_stringify(json, NULL);
		socket_write(sockfd, output);
		json_free(output);
		json_delete(json);

		if(socket_read(sockfd, &recvBuff, 0) != 0
		   || strcmp(recvBuff, "{\"status\":\"success\"}") != 0) {
			logprintf(LOG_ERR, "failed to send codes");
			goto close;
		}
	}

close:
	log_shell_disable();
	if(sockfd > 0) {
		socket_close(sockfd);
	}
	if(recvBuff != NULL) {
		FREE(recvBuff);
	}
	if(code != NULL) {
		json_delete(code);
	}

	plua_gc();
	protocol_gc();
	options_delete(options);
	options_gc();
	config_gc();
	threads_gc();
	dso_gc();
	log_gc();
	FREE(progname);
	xfree();

#ifdef _WIN32
	WSACleanup();
#endif

	return EXIT_SUCCESS;
}
