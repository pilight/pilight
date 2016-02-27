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
