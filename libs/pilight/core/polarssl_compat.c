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

#include "polarssl_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../polarssl/polarssl/entropy.h"
#include "../../polarssl/polarssl/md5.h"

/* Utility functions */

#if defined(__APPLE__)

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if_dl.h>
#include <ifaddrs.h>

int get_custom_data(char *buf, int len) {
  int offset = 0;
  struct ifaddrs *ifap, *ifaptr;
  unsigned char *ptr;

  memset(buf, 0, len);

  if (getifaddrs(&ifap) == 0) {
    for (ifaptr = ifap; ifaptr != NULL &&
         offset < len; ifaptr = ifaptr->ifa_next) {
      if (ifaptr->ifa_addr->sa_family == AF_LINK) {
        ptr = (unsigned char *)
            LLADDR((struct sockaddr_dl *)ifaptr->ifa_addr);
        offset += snprintf(buf + offset, len - offset,
                           "%02x:%02x:%02x:%02x:%02x:%02x\n",
                           *ptr, *(ptr + 1), *(ptr + 2), *(ptr + 3),
                           *(ptr + 4), *(ptr + 5));
      }
    }
    freeifaddrs(ifap);
  }
  buf[len - 1] = 0;

  return strlen(buf);
}

#elif defined(__linux__)

#include <sys/socket.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <linux/if.h>
#include <linux/sockios.h>

int get_custom_data(char *buf, int len) {
  int sock, offset = 0;
  struct ifconf conf;
  char ifconfbuf[128 * sizeof(struct ifreq)];
  struct ifreq *ifr;
  unsigned char *ptr;
  memset(buf, 0, len);
  if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) == 0) {
    return 0;
  }

  memset(ifconfbuf, 0, sizeof(ifconfbuf));
  conf.ifc_buf = ifconfbuf;
  conf.ifc_len = sizeof(ifconfbuf);
  if (ioctl(sock, SIOCGIFCONF, &conf ) != 0) {
    return 0;
  }

  for (ifr = conf.ifc_req;
       (unsigned char *)ifr < (unsigned char *)conf.ifc_req + conf.ifc_len &&
       offset < len; ifr++ ) {
    if (ioctl(sock, SIOCGIFFLAGS, ifr) != 0
        || ioctl(sock, SIOCGIFHWADDR, ifr) != 0) {
      continue;
    }

    ptr = (unsigned char *)&ifr->ifr_addr.sa_data;
    offset += snprintf(buf + offset, len - offset,
                       "%02x:%02x:%02x:%02x:%02x:%02x\n",
                       *ptr, *(ptr + 1), *(ptr + 2), *(ptr + 3),
                       *(ptr + 4), *(ptr + 5));
  }

  close(sock);
  buf[len - 1] = 0;

  return strlen(buf);
}

#elif defined(_WIN32)

#include <windows.h>
#include <iphlpapi.h>

int get_custom_data(char *buf, int len) {
  int offset = 0;
  IP_ADAPTER_INFO AdapterInfo[32];
  DWORD dwBufLen = sizeof(AdapterInfo);

  DWORD dwStatus = GetAdaptersInfo(AdapterInfo, &dwBufLen);
  if (dwStatus != ERROR_SUCCESS) {
    return 0;
  }

  PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo;
  while (pAdapterInfo != NULL && offset < len) {
    if (pAdapterInfo->AddressLength != 0) {
      offset += snprintf(buf + offset, len - offset,
                         "%02x:%02x:%02x:%02x:%02x:%02x\n",
                         pAdapterInfo->Address[0], pAdapterInfo->Address[1],
                         pAdapterInfo->Address[2], pAdapterInfo->Address[3],
                         pAdapterInfo->Address[4], pAdapterInfo->Address[5]);
    };

    pAdapterInfo = pAdapterInfo->Next;
  }

  buf[len - 1] = 0;

  return strlen(buf);
}

#else

int get_custom_data(char *buf, int len) {
  strncpy(buf, "Unknown OS", len);
  buf[len - 1] = 0;

  return strlen(buf);
}

#endif

static entropy_context g_entropy_context;
static ctr_drbg_context g_ctr_drbg_context;

/* Library initialization */

int SSL_library_init() {
  char custom_data[200];
  unsigned char custom_data_md5[16];
  int custom_data_size;

  entropy_init(&g_entropy_context);

  /* Use collection of MAC addresses as custom data */
  custom_data_size = get_custom_data(custom_data, sizeof(custom_data));

  /* Since PolarSSL limits size of custom data use its MD5 */
  md5((unsigned char*)custom_data, custom_data_size, custom_data_md5);
  ctr_drbg_init(&g_ctr_drbg_context, entropy_func, &g_entropy_context,
                custom_data_md5, sizeof(custom_data_md5));

  /* SSL_library_init() always returns "1" */
  return 1;
}

/* CTX functions */

SSL_METHOD* SSLv23_client_method() {
  static SSL_METHOD SSLv23_client = { SSL_IS_CLIENT,
                                      SSL_MAJOR_VERSION_3,
                                      SSL_MINOR_VERSION_0 };

  return &SSLv23_client;
}

SSL_METHOD* SSLv23_server_method() {
  static SSL_METHOD SSLv23_server = { SSL_IS_SERVER,
                                      SSL_MAJOR_VERSION_3,
                                      SSL_MINOR_VERSION_0 };

  return &SSLv23_server;
}

SSL_CTX *SSL_CTX_new(SSL_METHOD* ssl_method) {
  SSL_CTX *ctx = calloc(1, sizeof(*ctx));
  ctx->ssl_method = ssl_method;

  /*
   * PolarSSL requires SSL* for the rest of operations
   * So, do nothing here
   */
  return ctx;
}

void SSL_CTX_free(SSL_CTX *ctx) {
  x509_crt_free(&ctx->cert);
  x509_crt_free(&ctx->CA_cert);
  pk_free(&ctx->pk);
  free(ctx);
}

void SSL_CTX_set_verify(SSL_CTX *ctx, int mode, void* reserved) {
  (void) reserved;
  /*
   * PolarSSL default mode is SSL_VERIFY_NONE (???)
   * SSL_VERIFY_FAIL_IF_NO_PEER_CERT is not supported
   * so, if any verification required use SSL_VERIFY_REQUIRED
   */
  if (mode & SSL_VERIFY_PEER) {
    ctx->authmode = SSL_VERIFY_REQUIRED;
  }
}

long SSL_CTX_set_mode(SSL_CTX *ctx, long mode) {
  (void) ctx;
  /*
   * PolarSSL required to recall ssl_write with the SAME parameters
   * in case of WANT_WRITE and SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER
   * doesn't support directly
   * but since PolarSSL stores sent data as OFFSET (not pointer)
   * it is not a problem to move buffer (if data remains the same)
   * Cannot return error from SSL_CTX_set_mode
   * As result - do nothing
   */

  return mode;
}

int SSL_CTX_use_certificate_file(SSL_CTX *ctx, const char *file, int type) {
  (void) type;
  /* don't verify type, just trying to parse it */

  x509_crt_init(&ctx->cert);
  /* x509_crt_parse_file returns 0 on success */
  return x509_crt_parse_file(&ctx->cert, file) == 0;
}

int SSL_CTX_use_PrivateKey_file(SSL_CTX *ctx, const char *file, int type) {
  (void) type;
  /* don't verify type, just trying to parse it */

  pk_init(&ctx->pk);
  /*
   * mongoose doesn't use SSL_CTX_set_default_passwd_cb,
   * assume non-encrypted private key.
   * pk_parse_keyfile returns 0 on success
   */
  return pk_parse_keyfile(&ctx->pk, file, NULL) == 0;
}

int SSL_CTX_use_certificate_chain_file(SSL_CTX *ctx, const char *file) {
  /*
   * PolarSSL uses the same function for parsing
   * certificate and certificates chain.
   */
  return SSL_CTX_use_certificate_file(ctx, file, 1);
}

int SSL_CTX_load_verify_locations(SSL_CTX *ctx, const char *CAfile,
                                  const char *CApath) {
  /* Path is not supported */
  if (CApath != NULL) {
    return 0;
  }

  /*
   * PolasrSSL requires CA certificates to be explicitly load
   * x509_crt_parse_file returns 0 on success
   */
  x509_crt_init(&ctx->CA_cert);
  return x509_crt_parse_file(&ctx->CA_cert, CAfile) == 0;
}

/* SSL functions */

SSL *SSL_new(SSL_CTX *ctx) {
  int res;

  SSL *ssl = (SSL*)calloc(1, sizeof(*ssl));
  res = ssl_init(&ssl->cntx);

  if (res == 0) {
    ssl_set_endpoint(&ssl->cntx, ctx->ssl_method->endpoint_type);
    ssl_set_authmode(&ssl->cntx, ctx->authmode);
    ssl_set_min_version(&ssl->cntx, ctx->ssl_method->ssl_maj_ver,
                        ctx->ssl_method->ssl_min_ver);
    ssl_set_ca_chain(&ssl->cntx, &ctx->CA_cert, NULL, NULL);
    ssl_set_rng( &ssl->cntx, ctr_drbg_random, &g_ctr_drbg_context );

    res = ssl_set_own_cert(&ssl->cntx, &ctx->cert, &ctx->pk);
  }

  if (res != 0) {
    free(ssl);
    return NULL;
  }

  ssl->fd = -1;
  ssl->ssl_ctx = ctx;

  return ssl;
}

void SSL_free(SSL *ssl) {
  ssl_free(&ssl->cntx);
  free(ssl);
}

/*
 * PolarSSL functions return error code direcly,
 * it stores in ssl::last_error because some OpenSSL functions
 * assumes 0/1 retval only.
 */
int SSL_get_error(const SSL *ssl, int ret) {
  (void) ret;
  return ssl->last_error;
}

int SSL_set_fd(SSL *ssl, int fd) {
  ssl->fd = fd;
  ssl_set_bio( &ssl->cntx, net_recv, &ssl->fd, net_send, &ssl->fd );
  /*
   * ssl_set_bio doesn't return errors,
   * as result SSL_set_fd always returns success
   * anyway, mongoose doesn't handle SSL_set_fd result usually
   */
  ssl->last_error = 0;

  return 1;
}

/* PolarSSL read/write functions work as OpenSSL analogues */
int SSL_read(SSL *ssl, void *buf, int num) {
  ssl->last_error = ssl_read(&ssl->cntx, buf, num);
  return ssl->last_error;
}

int SSL_write(SSL *ssl, const void *buf, int num) {
  ssl->last_error = ssl_write(&ssl->cntx, buf, num);
  return ssl->last_error;
}

int SSL_connect(SSL *ssl) {
  ssl->last_error = ssl_handshake(&ssl->cntx);
  /*
   * ssl_handshake returns 0 if successful
   * mongoose doesn't handle negative values (The shutdown was not clean),
   * so, return 0/1 only.
   */
  return ssl->last_error == 0;
}

int SSL_accept(SSL *ssl) {
  /*
   * looks like non-blocking accept in PolarSSL
   * is the same as SSL_connect
   */
  return SSL_connect(ssl);
}
