/*
  Copyright (C) CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "libs/pilight/core/pilight.h"
#include "libs/pilight/core/common.h"
#include "libs/pilight/config/settings.h"
#include "libs/pilight/core/log.h"
#include "libs/pilight/core/options.h"
#include "libs/pilight/core/socket.h"
#include "libs/pilight/core/ssdp.h"
#include "libs/pilight/core/gc.h"

static int main_loop = 1;
static int sockfd = 0;
static char *recvBuff = NULL;
char **filters = NULL;
unsigned int m = 0;

int main_gc(void) {
	main_loop = 0;
	sleep(1);

	if(recvBuff != NULL) {
		FREE(recvBuff);
		recvBuff = NULL;
	}
	if(sockfd > 0) {
		socket_write(sockfd, "HEART");
		socket_close(sockfd);
	}
	xfree();

#ifdef _WIN32
	WSACleanup();
#endif

	return 0;
}

int main(int argc, char **argv) {
	// memtrack();

	atomicinit();
	gc_attach(main_gc);

	/* Catch all exit signals for gc */
	gc_catch();

	log_shell_enable();
	log_file_disable();

	log_level_set(LOG_NOTICE);

	if((progname = MALLOC(16)) == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	strcpy(progname, "pilight-receive");
	struct options_t *options = NULL;
	struct ssdp_list_t *ssdp_list = NULL;

	char *server = NULL;
	char *filter = NULL;
	int port = 0;
	int stats = 0;
	int filteropt = 0;
	int help = 0;

	options_add(&options, "H", "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "V", "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "S", "server", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
	options_add(&options, "P", "port", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "[0-9]{1,4}");
	options_add(&options, "s", "stats", OPTION_NO_VALUE, 0, JSON_NULL, NULL, "[0-9]{1,4}");
	options_add(&options, "F", "filter", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);

	if(options_parse(options, argc, argv, 1) == -1) {
		printf("Usage: %s \n", progname);
		goto close;
	}

	if(options_exists(options, "H") == 0 || help == 1) {
		printf("\t -H --help\t\t\tdisplay this message\n");
		printf("\t -V --version\t\t\tdisplay version\n");
		printf("\t -S --server=x.x.x.x\t\tconnect to server address\n");
		printf("\t -P --port=xxxx\t\t\tconnect to server port\n");
		printf("\t -s --stats\t\t\tshow CPU and RAM statistics\n");
		printf("\t -F --filter=protocol\t\tfilter out protocol(s)\n");
		goto close;
	}

	if(options_exists(options, "V") == 0) {
		printf("%s v%s\n", progname, PILIGHT_VERSION);
		goto close;
	}

	if(options_exists(options, "S") == 0) {
		options_get_string(options, "S", &server);
	}

	if(options_exists(options, "P") == 0) {
		options_get_number(options, "P", &port);
	}

	if(options_exists(options, "s") == 0) {
		stats = 1;
	}

	if(options_exists(options, "F") == 0) {
		options_get_string(options, "F", &filter);
		filteropt = 1;
	}

	if(filteropt == 1) {
		struct protocol_t *protocol = NULL;
		m = explode(filter, ",", &filters);
		int match = 0, j = 0;

		plua_init();
		protocol_init();

		for(j=0;j<m;j++) {
			match = 0;
			struct protocols_t *pnode = protocols;
			if(filters[j] != NULL && strlen(filters[j]) > 0) {
				while(pnode) {
					protocol = pnode->listener;
					if(protocol_device_exists(protocol, filters[j]) == 0 && match == 0) {
						match = 1;
						break;
					}
					pnode = pnode->next;
				}
				if(match == 0) {
					logprintf(LOG_ERR, "Invalid protocol: %s", filters[j]);
					goto close;
				}
			}
		}
	}

	if(server != NULL && port > 0) {
		if((sockfd = socket_connect(server, port)) == -1) {
			logprintf(LOG_ERR, "could not connect to pilight-daemon");
			return EXIT_FAILURE;
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
	if(ssdp_list != NULL) {
		ssdp_free(ssdp_list);
	}
	/*
	 * Already freed with the options struct
	 */
	// if(server != NULL) {
		// FREE(server);
	// }

	struct JsonNode *jclient = json_mkobject();
	struct JsonNode *joptions = json_mkobject();
	json_append_member(jclient, "action", json_mkstring("identify"));
	json_append_member(joptions, "receiver", json_mknumber(1, 0));
	json_append_member(joptions, "stats", json_mknumber(stats, 0));
	json_append_member(jclient, "options", joptions);
	char *out = json_stringify(jclient, NULL);
	socket_write(sockfd, out);
	json_free(out);
	json_delete(jclient);

	if(socket_read(sockfd, &recvBuff, 0) != 0 ||
		strcmp(recvBuff, "{\"status\":\"success\"}") != 0) {
			goto close;
	}

	while(main_loop) {
		if(socket_read(sockfd, &recvBuff, 0) != 0) {
			goto close;
		}
		char *protocol = NULL;
		char **array = NULL;
		unsigned int n = explode(recvBuff, "\n", &array), i = 0;

		for(i=0;i<n;i++) {
			struct JsonNode *jcontent = json_decode(array[i]);
			struct JsonNode *jtype = json_find_member(jcontent, "type");
			if(jtype != NULL) {
				json_remove_from_parent(jtype);
				json_delete(jtype);
			}
			if(filteropt == 1) {
				int filtered = 0, j = 0;
				json_find_string(jcontent, "protocol", &protocol);
				for(j=0;j<m;j++) {
					if(strcmp(filters[j], protocol) == 0) {
						filtered = 1;
						break;
					}
				}
				if(filtered == 0) {
					char *content = json_stringify(jcontent, "\t");
					printf("%s\n", content);
					json_free(content);
				}
			} else {
				char *content = json_stringify(jcontent, "\t");
				printf("%s\n", content);
				json_free(content);
			}
			json_delete(jcontent);
		}
		array_free(&array, n);
	}

close:
	if(sockfd > 0) {
		socket_close(sockfd);
	}
	if(recvBuff != NULL) {
		FREE(recvBuff);
		recvBuff = NULL;
	}
	options_delete(options);
	array_free(&filters, m);

	plua_gc();
	protocol_gc();
	options_gc();
	log_shell_disable();
	log_gc();
	FREE(progname);
	return EXIT_SUCCESS;
}
