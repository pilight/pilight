/*
 * Copyright (c) 2015 Cesanta Software Limited
 * All rights reserved
 * This software is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. For the terms of this
 * license, see <http://www.gnu.org/licenses/>.
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * Alternatively, you can license this software under a commercial
 * license, as set out in <http://cesanta.com/>.
 */

#ifndef POLARSSL_SUPPORT_HEADER_INCLUDED
#define POLARSSL_SUPPORT_HEADER_INCLUDED

#include "../../polarssl/polarssl/ssl.h"
#include "../../polarssl/polarssl/net.h"
#include "../../polarssl/polarssl/ctr_drbg.h"
#include "../../polarssl/polarssl/certs.h"
#include "../../polarssl/polarssl/x509.h"

#define SSL_ERROR_WANT_READ POLARSSL_ERR_NET_WANT_READ
#define SSL_ERROR_WANT_WRITE POLARSSL_ERR_NET_WANT_WRITE

#define SSL_VERIFY_PEER 1
#define SSL_VERIFY_FAIL_IF_NO_PEER_CERT 2

/*
 * SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER is not used at the moment.
 * see SSL_CTX_set_mode function implementation
 */
#define SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER 1

typedef struct ssl_method {
  int endpoint_type;
  int ssl_maj_ver;
  int ssl_min_ver;
} SSL_METHOD;

typedef struct ps_ssl_ctx {
  /* Own cert & private key */
  x509_crt cert;
  pk_context pk;
  /* CA certs */
  x509_crt CA_cert;
  /* SSL_VERIFY_REQUIRED in this implementation */
  int authmode;
  /* endpoint details */
  SSL_METHOD* ssl_method;
} SSL_CTX;

typedef struct ps_ssl {
  ssl_context cntx;
  /* last SSL error. see SSL_get_error implementation. */
  int last_error;
  /* associated socket */
  int fd;
  /* parent context (for debug purposes) */
  SSL_CTX* ssl_ctx;
} SSL;

int SSL_read(SSL *ssl, void *buf, int num);
int SSL_write(SSL *ssl, const void *buf, int num);
int SSL_get_error(const SSL *ssl, int ret);
int SSL_connect(SSL *ssl);
int SSL_set_fd(SSL *ssl, int fd);
int SSL_accept(SSL *ssl);
int SSL_library_init();
SSL_METHOD* SSLv23_client_method();
SSL_METHOD* SSLv23_server_method();
SSL *SSL_new(SSL_CTX *ctx);
void SSL_free(SSL *ssl);

void SSL_CTX_free(SSL_CTX *ctx);
void SSL_CTX_set_verify(SSL_CTX *ctx, int mode, void* reserved);
int SSL_CTX_load_verify_locations(SSL_CTX *ctx, const char *CAfile,
                                  const char *CApath);
int SSL_CTX_use_certificate_file(SSL_CTX *ctx, const char *file, int type);
int SSL_CTX_use_PrivateKey_file(SSL_CTX *ctx, const char *file, int type);
long SSL_CTX_set_mode(SSL_CTX *ctx, long mode);
int SSL_CTX_use_certificate_chain_file(SSL_CTX *ctx, const char *file);
SSL_CTX *SSL_CTX_new(SSL_METHOD* ssl_method);

#endif
