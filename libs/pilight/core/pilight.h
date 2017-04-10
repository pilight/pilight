/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _PILIGHT_H_
#define _PILIGHT_H_

typedef enum runmode_t {
	STANDALONE,
	ADHOC
} runmode_t;

typedef enum process_t {
	PROCESS_DAEMON,
	PROCESS_CLIENT
} process_t;

typedef enum origin_t {
	ORIGIN_RECEIVER = 0,
	ORIGIN_SENDER,
	ORIGIN_CONTROLLER,
	ORIGIN_MASTER,
	ORIGIN_NODE,
	ORIGIN_FW,
	ORIGIN_STATS,
	ORIGIN_ACTION,
	ORIGIN_RULE,
	ORIGIN_PROTOCOL,
	ORIGIN_HARDWARE,
	ORIGIN_CONFIG,
	ORIGIN_WEBSERVER,
	ORIGIN_SSDP
} origin_t;

#include "../../libuv/uv.h"

#include "defines.h"
#include "eventpool.h"
#include "json.h"
#include "mem.h"

#include "../storage/storage.h"

#include "../../mbedtls/mbedtls/error.h"
#include "../../mbedtls/mbedtls/pk.h"
#include "../../mbedtls/mbedtls/net.h"
#include "../../mbedtls/mbedtls/x509_crt.h"
#include "../../mbedtls/mbedtls/ctr_drbg.h"
#include "../../mbedtls/mbedtls/entropy.h"
#include "../../mbedtls/mbedtls/ssl.h"
#include "../../mbedtls/mbedtls/ssl_cache.h"

mbedtls_entropy_context ssl_entropy;
mbedtls_ctr_drbg_context ssl_ctr_drbg;
mbedtls_pk_context ssl_pk_key;
mbedtls_x509_crt ssl_server_crt;
mbedtls_ssl_cache_context ssl_cache;
struct mbedtls_ssl_config ssl_client_conf;
struct mbedtls_ssl_config ssl_server_conf;

struct pilight_t {
	// void (*broadcast)(char *name, struct JsonNode *message, enum origin_t origin);
	int (*send)(JsonNode *, enum origin_t);
	int (*control)(char *, char *, struct JsonNode *, enum origin_t);
	void (*receive)(struct JsonNode *, int);
	int (*socket)(char *, char *, char **);

	runmode_t runmode;
	/* pilight actually runs in this stage and the configuration is fully validated */
	int running;
	int debuglevel;
	process_t process;
} pilight_t;

extern struct pilight_t pilight;
extern const uv_thread_t pth_main_id;
extern char pilight_uuid[UUID_LENGTH];

#endif
