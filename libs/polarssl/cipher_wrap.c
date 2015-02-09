/**
 * \file cipher_wrap.c
 *
 * \brief Generic cipher wrapper for PolarSSL
 *
 * \author Adriaan de Jong <dejong@fox-it.com>
 *
 *  Copyright (C) 2006-2014, Brainspark B.V.
 *
 *  This file is part of PolarSSL (http://www.polarssl.org)
 *  Lead Maintainer: Paul Bakker <polarssl_maintainer at polarssl.org>
 *
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "polarssl.h"

#if defined(POLARSSL_CIPHER_C)

#include "cipher_wrap.h"

#if defined(POLARSSL_AES_C)
#include "aes.h"
#endif

#define polarssl_malloc     malloc
#define polarssl_free       free

#include <stdlib.h>



#if defined(POLARSSL_AES_C)

static int aes_crypt_ecb_wrap( void *ctx, operation_t operation,
        const unsigned char *input, unsigned char *output )
{
    return aes_crypt_ecb( (aes_context *) ctx, operation, input, output );
}

static int aes_crypt_cbc_wrap( void *ctx, operation_t operation, size_t length,
        unsigned char *iv, const unsigned char *input, unsigned char *output )
{
#if defined(POLARSSL_CIPHER_MODE_CBC)
    return aes_crypt_cbc( (aes_context *) ctx, operation, length, iv, input, output );
#else
    ((void) ctx);
    ((void) operation);
    ((void) length);
    ((void) iv);
    ((void) input);
    ((void) output);

    return POLARSSL_ERR_CIPHER_FEATURE_UNAVAILABLE;
#endif /* POLARSSL_CIPHER_MODE_CBC */
}

static int aes_crypt_cfb128_wrap( void *ctx, operation_t operation, size_t length,
        size_t *iv_off, unsigned char *iv, const unsigned char *input, unsigned char *output )
{
#if defined(POLARSSL_CIPHER_MODE_CFB)
    return aes_crypt_cfb128( (aes_context *) ctx, operation, length, iv_off, iv, input, output );
#else
    ((void) ctx);
    ((void) operation);
    ((void) length);
    ((void) iv_off);
    ((void) iv);
    ((void) input);
    ((void) output);

    return POLARSSL_ERR_CIPHER_FEATURE_UNAVAILABLE;
#endif
}

static int aes_crypt_ctr_wrap( void *ctx, size_t length,
        size_t *nc_off, unsigned char *nonce_counter, unsigned char *stream_block,
        const unsigned char *input, unsigned char *output )
{
#if defined(POLARSSL_CIPHER_MODE_CTR)
    return aes_crypt_ctr( (aes_context *) ctx, length, nc_off, nonce_counter,
                          stream_block, input, output );
#else
    ((void) ctx);
    ((void) length);
    ((void) nc_off);
    ((void) nonce_counter);
    ((void) stream_block);
    ((void) input);
    ((void) output);

    return POLARSSL_ERR_CIPHER_FEATURE_UNAVAILABLE;
#endif
}

static int aes_setkey_dec_wrap( void *ctx, const unsigned char *key, unsigned int key_length )
{
    return aes_setkey_dec( (aes_context *) ctx, key, key_length );
}

static int aes_setkey_enc_wrap( void *ctx, const unsigned char *key, unsigned int key_length )
{
    return aes_setkey_enc( (aes_context *) ctx, key, key_length );
}

static void * aes_ctx_alloc( void )
{
    return polarssl_malloc( sizeof( aes_context ) );
}

static void aes_ctx_free( void *ctx )
{
    polarssl_free( ctx );
}

const cipher_base_t aes_info = {
    POLARSSL_CIPHER_ID_AES,
    aes_crypt_ecb_wrap,
    aes_crypt_cbc_wrap,
    aes_crypt_cfb128_wrap,
    aes_crypt_ctr_wrap,
    NULL,
    aes_setkey_enc_wrap,
    aes_setkey_dec_wrap,
    aes_ctx_alloc,
    aes_ctx_free
};

const cipher_info_t aes_128_ecb_info = {
    POLARSSL_CIPHER_AES_128_ECB,
    POLARSSL_MODE_ECB,
    128,
    "AES-128-ECB",
    16,
    0,
    16,
    &aes_info
};

const cipher_info_t aes_192_ecb_info = {
    POLARSSL_CIPHER_AES_192_ECB,
    POLARSSL_MODE_ECB,
    192,
    "AES-192-ECB",
    16,
    0,
    16,
    &aes_info
};

const cipher_info_t aes_256_ecb_info = {
    POLARSSL_CIPHER_AES_256_ECB,
    POLARSSL_MODE_ECB,
    256,
    "AES-256-ECB",
    16,
    0,
    16,
    &aes_info
};

#if defined(POLARSSL_CIPHER_MODE_CBC)
const cipher_info_t aes_128_cbc_info = {
    POLARSSL_CIPHER_AES_128_CBC,
    POLARSSL_MODE_CBC,
    128,
    "AES-128-CBC",
    16,
    0,
    16,
    &aes_info
};

const cipher_info_t aes_192_cbc_info = {
    POLARSSL_CIPHER_AES_192_CBC,
    POLARSSL_MODE_CBC,
    192,
    "AES-192-CBC",
    16,
    0,
    16,
    &aes_info
};

const cipher_info_t aes_256_cbc_info = {
    POLARSSL_CIPHER_AES_256_CBC,
    POLARSSL_MODE_CBC,
    256,
    "AES-256-CBC",
    16,
    0,
    16,
    &aes_info
};
#endif /* POLARSSL_CIPHER_MODE_CBC */

#endif

const cipher_definition_t cipher_definitions[] =
{
#if defined(POLARSSL_AES_C)
    { POLARSSL_CIPHER_AES_128_ECB,          &aes_128_ecb_info },
    { POLARSSL_CIPHER_AES_192_ECB,          &aes_192_ecb_info },
    { POLARSSL_CIPHER_AES_256_ECB,          &aes_256_ecb_info },
#if defined(POLARSSL_CIPHER_MODE_CBC)
    { POLARSSL_CIPHER_AES_128_CBC,          &aes_128_cbc_info },
    { POLARSSL_CIPHER_AES_192_CBC,          &aes_192_cbc_info },
    { POLARSSL_CIPHER_AES_256_CBC,          &aes_256_cbc_info },
#endif
#if defined(POLARSSL_CIPHER_MODE_CFB)
    { POLARSSL_CIPHER_AES_128_CFB128,       &aes_128_cfb128_info },
    { POLARSSL_CIPHER_AES_192_CFB128,       &aes_192_cfb128_info },
    { POLARSSL_CIPHER_AES_256_CFB128,       &aes_256_cfb128_info },
#endif
#if defined(POLARSSL_CIPHER_MODE_CTR)
    { POLARSSL_CIPHER_AES_128_CTR,          &aes_128_ctr_info },
    { POLARSSL_CIPHER_AES_192_CTR,          &aes_192_ctr_info },
    { POLARSSL_CIPHER_AES_256_CTR,          &aes_256_ctr_info },
#endif
#if defined(POLARSSL_GCM_C)
    { POLARSSL_CIPHER_AES_128_GCM,          &aes_128_gcm_info },
    { POLARSSL_CIPHER_AES_192_GCM,          &aes_192_gcm_info },
    { POLARSSL_CIPHER_AES_256_GCM,          &aes_256_gcm_info },
#endif
#endif /* POLARSSL_AES_C */

    { 0, NULL }
};

#define NUM_CIPHERS sizeof cipher_definitions / sizeof cipher_definitions[0]
int supported_ciphers[NUM_CIPHERS];

#endif
