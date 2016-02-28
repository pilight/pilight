/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _SSL_H_
#define _SSL_H_

#include "../../mbedtls/mbedtls/error.h"
#include "../../mbedtls/mbedtls/pk.h"
#include "../../mbedtls/mbedtls/net.h"
#include "../../mbedtls/mbedtls/x509_crt.h"
#include "../../mbedtls/mbedtls/ctr_drbg.h"
#include "../../mbedtls/mbedtls/entropy.h"
#include "../../mbedtls/mbedtls/ssl.h"
#include "../../mbedtls/mbedtls/debug.h"
#include "../../mbedtls/mbedtls/cipher.h"
#include "../../mbedtls/mbedtls/ssl_cache.h"

mbedtls_entropy_context ssl_entropy;
mbedtls_ctr_drbg_context ssl_ctr_drbg;
mbedtls_pk_context ssl_pk_key;
mbedtls_x509_crt ssl_server_crt;
mbedtls_ssl_cache_context ssl_cache;
struct mbedtls_ssl_config ssl_client_conf;
struct mbedtls_ssl_config ssl_server_conf;

int ssl_client_init_status(void);
int ssl_server_init_status(void);
void ssl_init(void);
void ssl_gc(void);

#endif
