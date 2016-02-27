/*
	Copyright (C) 2013 - 2014 CurlyMo

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

extern char pilight_uuid[UUID_LENGTH];

#endif
