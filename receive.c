/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#ifndef _WIN32
	#include <unistd.h>
	#include <sys/socket.h>
#endif
#include <sys/types.h>

#include "libs/libuv/uv.h"
#include "libs/pilight/core/http.h"
#include "libs/pilight/core/eventpool.h"
#include "libs/pilight/core/pilight.h"
#include "libs/pilight/core/common.h"
#include "libs/pilight/core/log.h"
#include "libs/pilight/core/options.h"
#include "libs/pilight/core/socket.h"
#include "libs/pilight/core/ssdp.h"
#include "libs/pilight/protocols/protocol.h"

#define INCLUDE		1
#define EXCLUDE		2

static uv_tty_t *tty_req = NULL;
static uv_signal_t *signal_req = NULL;

static struct ssdp_list_t *ssdp_list = NULL;
static char *instance = NULL;
static char **filters = NULL;
static int ssdp_list_size = 0;
static unsigned int nrfilter = 0;
static unsigned short stats = 0;
static unsigned short connecting = 0;
static unsigned short connected = 0;
static unsigned short found = 0;
static unsigned short filtertype = 0;

typedef struct ssdp_list_t {
	char server[INET_ADDRSTRLEN+1];
	unsigned short port;
	char name[17];
	struct ssdp_list_t *next;
} ssdp_list_t;

static void signal_cb(uv_signal_t *, int);
static uv_timer_t *ssdp_reseek_req = NULL;

static int main_gc(void) {
	if(filters != NULL) {
		array_free(&filters, nrfilter);
	}

	struct ssdp_list_t *tmp = NULL;
	while(ssdp_list) {
		tmp = ssdp_list;
		ssdp_list = ssdp_list->next;
		FREE(tmp);
	}

	socket_gc();
	protocol_gc();
	eventpool_gc();
	ssdp_gc();

	log_shell_disable();
	eventpool_gc();
	log_gc();
	FREE(progname);

	return 0;
}

static void timeout_cb(uv_timer_t *param) {
	if(connected == 0) {
		logprintf(LOG_ERR, "could not connect to the pilight instance", "");
		signal_cb(NULL, SIGINT);
	}
}

static void ssdp_not_found(uv_timer_t *param) {
	if(found == 0) {
		logprintf(LOG_ERR, "could not find pilight instance: %s", instance);
		signal_cb(NULL, SIGINT);
	}
}

static void alloc_cb(uv_handle_t *handle, size_t len, uv_buf_t *buf) {
	buf->len = len;
	if((buf->base = malloc(len)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(buf->base, 0, len);
}

static void output(struct JsonNode *json) {
	if(filtertype != 0) {
		int filtered = 0, j = 0;
		char *protocol = NULL;
		if(filtertype == INCLUDE) {
			filtered = 1;
		}
		json_find_string(json, "protocol", &protocol);
		for(j=0;j<nrfilter;j++) {
			if(strcmp(filters[j], protocol) == 0) {
				if(filtertype == INCLUDE) {
					filtered = 0;
					break;
				} else if(filtertype == EXCLUDE) {
					filtered = 1;
					break;
				}
			}
		}
		if(filtered == 0) {
			char *out = json_stringify(json, "\t");
			printf("%s\n", out);
			json_free(out);
		}
	} else {
		char *out = json_stringify(json, "\t");
		printf("%s\n", out);
		json_free(out);
	}
	json_delete(json);

	return;
}

static void on_read(int fd, char *buf, ssize_t len, char **buf1, ssize_t *len1) {
	if(strcmp(buf, "1") != 0 &&
	   strncmp(buf, "{\"status\":\"success\"}", 20) != 0) {
		if(socket_recv(buf, len, buf1, len1) > 0) {
			if(strstr(*buf1, "\n") != NULL) {
				char **array = NULL;
				int n = explode(*buf1, "\n", &array), i = 0;
				for(i=0;i<n;i++) {
					struct JsonNode *json = json_decode(array[i]);
					if(json != NULL) {
						output(json);
					} else {
						logprintf(LOG_ERR, "invalid JSON received: %s", buf);
					}
				}
				array_free(&array, n);
			} else {
				struct JsonNode *json = json_decode(*buf1);
				if(json != NULL) {
					output(json);
				} else {
					logprintf(LOG_ERR, "invalid JSON received: %s", buf);
				}
			}
			FREE(*buf1);
			*len1 = 0;
		}
	}

	return;
}

static void *socket_disconnected(int reason, void *param, void *userdata) {
	struct reason_socket_disconnected_t *data = param;

	socket_close(data->fd);
	signal_cb(NULL, SIGINT);

	return NULL;
}

static void *socket_connected(int reason, void *param, void *userdata) {
	struct reason_socket_connected_t *data = param;

	connected = 1;

	struct JsonNode *jclient = json_mkobject();
	struct JsonNode *joptions = json_mkobject();
	json_append_member(jclient, "action", json_mkstring("identify"));
	json_append_member(joptions, "receiver", json_mknumber(1, 0));
	json_append_member(joptions, "stats", json_mknumber(stats, 0));
	json_append_member(jclient, "options", joptions);

	char *out = json_stringify(jclient, NULL);

	socket_write(data->fd, out);

	json_delete(jclient);
	json_free(out);
	return NULL;
}

static void connect_to_server(char *server, int port) {
	socket_connect(server, port, on_read);

	uv_timer_t *socket_timeout_req = NULL;
	
	if((socket_timeout_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_timer_init(uv_default_loop(), socket_timeout_req);
	uv_timer_start(socket_timeout_req, timeout_cb, 1000, 0);
}

static int select_server(int server) {
	struct ssdp_list_t *tmp = ssdp_list;
	int i = 0;
	while(tmp) {
		if((ssdp_list_size-i) == server) {
			connect_to_server(tmp->server, tmp->port);
			return 0;
		}
		i++;
		tmp = tmp->next;
	}
	return -1;
}

static void *ssdp_found(int reason, void *param, void *userdata) {
	struct reason_ssdp_received_t *data = param;
	struct ssdp_list_t *node = NULL;
	int match = 0;

	if(connecting == 0 && data->ip != NULL && data->port > 0 && data->name != NULL) {
		if(instance == NULL) {
			struct ssdp_list_t *tmp = ssdp_list;
			while(tmp) {
				if(strcmp(tmp->server, data->ip) == 0 && tmp->port == data->port) {
					match = 1;
					break;
				}
				tmp = tmp->next;
			}
			if(match == 0) {
				if((node = MALLOC(sizeof(struct ssdp_list_t))) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				strncpy(node->server, data->ip, INET_ADDRSTRLEN);
				node->port = data->port;
				strncpy(node->name, data->name, 17);

				ssdp_list_size++;

				printf("\r[%2d] %15s:%-5d %-16s\n", ssdp_list_size, node->server, node->port, node->name);
				printf("To which server do you want to connect?: ");
				fflush(stdout);

				node->next = ssdp_list;
				ssdp_list = node;
			}
		} else {
			if(strcmp(data->name, instance) == 0) {
				found = 1;
				uv_timer_stop(ssdp_reseek_req);
				connect_to_server(data->ip, data->port);
			}
		}
	}
	return NULL;
}

static void read_cb(uv_stream_t *stream, ssize_t len, const uv_buf_t *buf) {
#ifdef _WIN32
	if(len == 1 && buf->base[0] == 3) {
		signal_cb(NULL, SIGINT);
	}
#else
	buf->base[len-1] = '\0';
#endif

#ifdef _WIN32
	/* Remove windows vertical tab */
	if(buf->base[len-2] == 13) {
		buf->base[len-2] = '\0';
	}
#endif

	if(isNumeric(buf->base) == 0) {
		select_server(atoi(buf->base));
	}
	free(buf->base);
}

static void close_cb(uv_handle_t *handle) {
	FREE(handle);
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void signal_cb(uv_signal_t *handle, int signum) {
	if(instance == NULL && tty_req != NULL) {
		uv_read_stop((uv_stream_t *)tty_req);
		tty_req = NULL;
	}
	uv_stop(uv_default_loop());
	main_gc();
}

static void main_loop(int onclose) {
	if(onclose == 1) {
		signal_cb(NULL, SIGINT);
	}
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	if(onclose == 1) {
		while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
			uv_run(uv_default_loop(), UV_RUN_DEFAULT);
		}
	}
}

int main(int argc, char **argv) {
	const uv_thread_t pth_cur_id = uv_thread_self();
	memcpy((void *)&pth_main_id, &pth_cur_id, sizeof(uv_thread_t));

	log_init();
	log_shell_enable();
	log_file_disable();
	log_level_set(LOG_NOTICE);

	uv_replace_allocator(_MALLOC, _REALLOC, _CALLOC, _FREE);

	pilight.process = PROCESS_CLIENT;

	if((progname = MALLOC(16)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(progname, "pilight-receive");

	if((signal_req = malloc(sizeof(uv_signal_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_signal_init(uv_default_loop(), signal_req);
	uv_signal_start(signal_req, signal_cb, SIGINT);

	struct options_t *options = NULL;

	char *args = NULL;
	char *server = NULL;
	char *filter = NULL;
	unsigned short port = 0;

	options_add(&options, "H", "help", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "V", "version", OPTION_NO_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "S", "server", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
	options_add(&options, "P", "port", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, "[0-9]{1,4}");
	options_add(&options, "I", "instance", OPTION_HAS_VALUE, 0, JSON_NULL, NULL, NULL);
	options_add(&options, "s", "stats", OPTION_NO_VALUE, 0, JSON_NULL, NULL, "[0-9]{1,4}");
	options_add(&options, "F", "filter", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&options, "X", "exclude", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);

	/* Store all CLI arguments for later usage
	   and also check if the CLI arguments where
	   used correctly by the user. This will also
	   fill all necessary values in the options struct */
	while(1) {
		int c;
		c = options_parse1(&options, argc, argv, 1, &args, NULL);
		if(c == -1)
			break;
		if(c == -2)
			c = 'H';
		switch(c) {
			case 'H':
				printf("\t -H --help\t\t\tdisplay this message\n");
				printf("\t -V --version\t\t\tdisplay version\n");
				printf("\t -S --server=x.x.x.x\t\tconnect to server address\n");
				printf("\t -P --port=xxxx\t\t\tconnect to server port\n");
				printf("\t -I --instance=name\t\tconnect to pilight instance\n");
				printf("\t -s --stats\t\t\tshow CPU and RAM statistics\n");
				printf("\t -F --filter=protocol\t\tfilter to include protocol(s) in output\n");
				printf("\t -X --exclude=protocol\t\texclude protocol(s) from output\n");
				goto close;
			break;
			case 'V':
				printf("%s v%s\n", progname, PILIGHT_VERSION);
				goto close;
			break;
			case 'S':
				if((server = MALLOC(strlen(args)+1)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				strcpy(server, args);
			break;
			case 'I':
				if((instance = MALLOC(strlen(args)+1)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				strcpy(instance, args);
			break;
			case 'P':
				port = (unsigned short)atoi(args);
			break;
			case 's':
				stats = 1;
			break;
			case 'F':
				if((filter = MALLOC(strlen(args)+1)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				strcpy(filter, args);
				filtertype = INCLUDE;
			break;
			case 'X':
				if(filtertype == 0) {
					if((filter = MALLOC(strlen(args)+1)) == NULL) {
						OUT_OF_MEMORY /* LCOV_EXCL_LINE */
					}
					strcpy(filter, args);
					filtertype = EXCLUDE;
				} else {
					logprintf(LOG_ERR, "Cannot filter and exclude simultaneously");
					goto close;
				}
			break;
			default:
				printf("Usage: %s \n", progname);
				goto close;
			break;
		}
	}

	if(filtertype > 0) {
		struct protocol_t *protocol = NULL;
		unsigned int match = 0, j = 0;

		protocol_init();

		nrfilter = explode(filter, ",", &filters);
		
		for(j=0;j<nrfilter;j++) {
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
					logprintf(LOG_ERR, "invalid protocol: %s", filters[j]);
					goto close;
				}
			}
		}
		FREE(filter);
	}

	eventpool_init(EVENTPOOL_NO_THREADS);
	eventpool_callback(REASON_SSDP_RECEIVED, ssdp_found, NULL);
	eventpool_callback(REASON_SOCKET_CONNECTED, socket_connected, NULL);
	eventpool_callback(REASON_SOCKET_DISCONNECTED, socket_disconnected, NULL);

	if(server != NULL && port > 0) {
		connect_to_server(server, port);
	} else {
		ssdp_seek();
		if((ssdp_reseek_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}

		uv_timer_init(uv_default_loop(), ssdp_reseek_req);
		uv_timer_start(ssdp_reseek_req, (void (*)(uv_timer_t *))ssdp_seek, 3000, 3000);
		if(instance == NULL) {
			printf("[%2s] %15s:%-5s %-16s\n", "#", "server", "port", "name");
			printf("To which server do you want to connect?:\r");
			fflush(stdout);

			if((tty_req = MALLOC(sizeof(uv_tty_t))) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}

			uv_tty_init(uv_default_loop(), tty_req, 0, 1);
#ifdef _WIN32
			uv_tty_set_mode(tty_req, UV_TTY_MODE_RAW);
#endif
			uv_read_start((uv_stream_t *)tty_req, alloc_cb, read_cb);
		} else {
			uv_timer_t *ssdp_not_found_req = NULL;
			if((ssdp_not_found_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}

			uv_timer_init(uv_default_loop(), ssdp_not_found_req);
			uv_timer_start(ssdp_not_found_req, ssdp_not_found, 1000, 0);
		}
	}

	main_loop(0);

close:
	if(options != NULL) {
		options_delete(options);
		options_gc();
		options = NULL;
	}

	if(filter != NULL) {
		FREE(filter);
	}

	main_loop(1);

	if(server != NULL) {
		FREE(server);
	}

	if(instance != NULL) {
		FREE(instance);
	}

	main_gc();

	return EXIT_SUCCESS;
}
