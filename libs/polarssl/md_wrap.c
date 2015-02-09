/**
 * \file md_wrap.c

 * \brief Generic message digest wrapper for PolarSSL
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

#if defined(POLARSSL_MD_C)

#include "md_wrap.h"

#if defined(POLARSSL_MD5_C)
#include "md5.h"
#endif

#if defined(POLARSSL_SHA1_C)
#include "sha1.h"
#endif

#if defined(POLARSSL_SHA512_C)
#include "sha512.h"
#endif

#define polarssl_malloc     malloc
#define polarssl_free       free

#include <stdlib.h>

#if defined(POLARSSL_MD5_C)

static void md5_starts_wrap( void *ctx )
{
    md5_starts( (md5_context *) ctx );
}

static void md5_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    md5_update( (md5_context *) ctx, input, ilen );
}

static void md5_finish_wrap( void *ctx, unsigned char *output )
{
    md5_finish( (md5_context *) ctx, output );
}

static int md5_file_wrap( const char *path, unsigned char *output )
{
    ((void) path);
    ((void) output);
    return POLARSSL_ERR_MD_FEATURE_UNAVAILABLE;
}

static void md5_hmac_starts_wrap( void *ctx, const unsigned char *key, size_t keylen )
{
    md5_hmac_starts( (md5_context *) ctx, key, keylen );
}

static void md5_hmac_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    md5_hmac_update( (md5_context *) ctx, input, ilen );
}

static void md5_hmac_finish_wrap( void *ctx, unsigned char *output )
{
    md5_hmac_finish( (md5_context *) ctx, output );
}

static void md5_hmac_reset_wrap( void *ctx )
{
    md5_hmac_reset( (md5_context *) ctx );
}

static void * md5_ctx_alloc( void )
{
    return polarssl_malloc( sizeof( md5_context ) );
}

static void md5_ctx_free( void *ctx )
{
    polarssl_free( ctx );
}

static void md5_process_wrap( void *ctx, const unsigned char *data )
{
    md5_process( (md5_context *) ctx, data );
}

const md_info_t md5_info = {
    POLARSSL_MD_MD5,
    "MD5",
    16,
    md5_starts_wrap,
    md5_update_wrap,
    md5_finish_wrap,
    md5,
    md5_file_wrap,
    md5_hmac_starts_wrap,
    md5_hmac_update_wrap,
    md5_hmac_finish_wrap,
    md5_hmac_reset_wrap,
    md5_hmac,
    md5_ctx_alloc,
    md5_ctx_free,
    md5_process_wrap,
};

#endif

#if defined(POLARSSL_SHA1_C)

static void sha1_starts_wrap( void *ctx )
{
    sha1_starts( (sha1_context *) ctx );
}

static void sha1_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    sha1_update( (sha1_context *) ctx, input, ilen );
}

static void sha1_finish_wrap( void *ctx, unsigned char *output )
{
    sha1_finish( (sha1_context *) ctx, output );
}

static int sha1_file_wrap( const char *path, unsigned char *output )
{
    ((void) path);
    ((void) output);
    return POLARSSL_ERR_MD_FEATURE_UNAVAILABLE;
}

static void sha1_hmac_starts_wrap( void *ctx, const unsigned char *key, size_t keylen )
{
    sha1_hmac_starts( (sha1_context *) ctx, key, keylen );
}

static void sha1_hmac_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    sha1_hmac_update( (sha1_context *) ctx, input, ilen );
}

static void sha1_hmac_finish_wrap( void *ctx, unsigned char *output )
{
    sha1_hmac_finish( (sha1_context *) ctx, output );
}

static void sha1_hmac_reset_wrap( void *ctx )
{
    sha1_hmac_reset( (sha1_context *) ctx );
}

static void * sha1_ctx_alloc( void )
{
    return polarssl_malloc( sizeof( sha1_context ) );
}

static void sha1_ctx_free( void *ctx )
{
    polarssl_free( ctx );
}

static void sha1_process_wrap( void *ctx, const unsigned char *data )
{
    sha1_process( (sha1_context *) ctx, data );
}

const md_info_t sha1_info = {
    POLARSSL_MD_SHA1,
    "SHA1",
    20,
    sha1_starts_wrap,
    sha1_update_wrap,
    sha1_finish_wrap,
    sha1,
    sha1_file_wrap,
    sha1_hmac_starts_wrap,
    sha1_hmac_update_wrap,
    sha1_hmac_finish_wrap,
    sha1_hmac_reset_wrap,
    sha1_hmac,
    sha1_ctx_alloc,
    sha1_ctx_free,
    sha1_process_wrap,
};

#endif

#if defined(POLARSSL_SHA512_C)

static void sha384_starts_wrap( void *ctx )
{
    sha512_starts( (sha512_context *) ctx, 1 );
}

static void sha384_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    sha512_update( (sha512_context *) ctx, input, ilen );
}

static void sha384_finish_wrap( void *ctx, unsigned char *output )
{
    sha512_finish( (sha512_context *) ctx, output );
}

static void sha384_wrap( const unsigned char *input, size_t ilen,
                    unsigned char *output )
{
    sha512( input, ilen, output, 1 );
}

static int sha384_file_wrap( const char *path, unsigned char *output )
{
    ((void) path);
    ((void) output);
    return POLARSSL_ERR_MD_FEATURE_UNAVAILABLE;
}

static void sha384_hmac_starts_wrap( void *ctx, const unsigned char *key, size_t keylen )
{
    sha512_hmac_starts( (sha512_context *) ctx, key, keylen, 1 );
}

static void sha384_hmac_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    sha512_hmac_update( (sha512_context *) ctx, input, ilen );
}

static void sha384_hmac_finish_wrap( void *ctx, unsigned char *output )
{
    sha512_hmac_finish( (sha512_context *) ctx, output );
}

static void sha384_hmac_reset_wrap( void *ctx )
{
    sha512_hmac_reset( (sha512_context *) ctx );
}

static void sha384_hmac_wrap( const unsigned char *key, size_t keylen,
        const unsigned char *input, size_t ilen,
        unsigned char *output )
{
    sha512_hmac( key, keylen, input, ilen, output, 1 );
}

static void * sha384_ctx_alloc( void )
{
    return polarssl_malloc( sizeof( sha512_context ) );
}

static void sha384_ctx_free( void *ctx )
{
    polarssl_free( ctx );
}

static void sha384_process_wrap( void *ctx, const unsigned char *data )
{
    sha512_process( (sha512_context *) ctx, data );
}

const md_info_t sha384_info = {
    POLARSSL_MD_SHA384,
    "SHA384",
    48,
    sha384_starts_wrap,
    sha384_update_wrap,
    sha384_finish_wrap,
    sha384_wrap,
    sha384_file_wrap,
    sha384_hmac_starts_wrap,
    sha384_hmac_update_wrap,
    sha384_hmac_finish_wrap,
    sha384_hmac_reset_wrap,
    sha384_hmac_wrap,
    sha384_ctx_alloc,
    sha384_ctx_free,
    sha384_process_wrap,
};

static void sha512_starts_wrap( void *ctx )
{
    sha512_starts( (sha512_context *) ctx, 0 );
}

static void sha512_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    sha512_update( (sha512_context *) ctx, input, ilen );
}

static void sha512_finish_wrap( void *ctx, unsigned char *output )
{
    sha512_finish( (sha512_context *) ctx, output );
}

static void sha512_wrap( const unsigned char *input, size_t ilen,
                    unsigned char *output )
{
    sha512( input, ilen, output, 0 );
}

static int sha512_file_wrap( const char *path, unsigned char *output )
{
    ((void) path);
    ((void) output);
    return POLARSSL_ERR_MD_FEATURE_UNAVAILABLE;
}

static void sha512_hmac_starts_wrap( void *ctx, const unsigned char *key, size_t keylen )
{
    sha512_hmac_starts( (sha512_context *) ctx, key, keylen, 0 );
}

static void sha512_hmac_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    sha512_hmac_update( (sha512_context *) ctx, input, ilen );
}

static void sha512_hmac_finish_wrap( void *ctx, unsigned char *output )
{
    sha512_hmac_finish( (sha512_context *) ctx, output );
}

static void sha512_hmac_reset_wrap( void *ctx )
{
    sha512_hmac_reset( (sha512_context *) ctx );
}

static void sha512_hmac_wrap( const unsigned char *key, size_t keylen,
        const unsigned char *input, size_t ilen,
        unsigned char *output )
{
    sha512_hmac( key, keylen, input, ilen, output, 0 );
}

static void * sha512_ctx_alloc( void )
{
    return polarssl_malloc( sizeof( sha512_context ) );
}

static void sha512_ctx_free( void *ctx )
{
    polarssl_free( ctx );
}

static void sha512_process_wrap( void *ctx, const unsigned char *data )
{
    sha512_process( (sha512_context *) ctx, data );
}

const md_info_t sha512_info = {
    POLARSSL_MD_SHA512,
    "SHA512",
    64,
    sha512_starts_wrap,
    sha512_update_wrap,
    sha512_finish_wrap,
    sha512_wrap,
    sha512_file_wrap,
    sha512_hmac_starts_wrap,
    sha512_hmac_update_wrap,
    sha512_hmac_finish_wrap,
    sha512_hmac_reset_wrap,
    sha512_hmac_wrap,
    sha512_ctx_alloc,
    sha512_ctx_free,
    sha512_process_wrap,
};

#endif

#endif
