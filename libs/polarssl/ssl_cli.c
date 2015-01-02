/*
 *  SSLv3/TLSv1 client-side functions
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

#if defined(POLARSSL_SSL_CLI_C)

#include "debug.h"
#include "ssl.h"

#define polarssl_malloc     malloc
#define polarssl_free       free

#include <stdlib.h>
#include <stdio.h>

#if defined(_MSC_VER) && !defined(EFIX64) && !defined(EFI32)
#include <basetsd.h>
typedef UINT32 uint32_t;
#else
#include <inttypes.h>
#endif

static void ssl_write_renegotiation_ext( ssl_context *ssl,
                                         unsigned char *buf,
                                         size_t *olen )
{
    unsigned char *p = buf;

    *olen = 0;

    if( ssl->renegotiation != SSL_RENEGOTIATION )
        return;

    SSL_DEBUG_MSG( 3, ( "client hello, adding renegotiation extension" ) );

    /*
     * Secure renegotiation
     */
    *p++ = (unsigned char)( ( TLS_EXT_RENEGOTIATION_INFO >> 8 ) & 0xFF );
    *p++ = (unsigned char)( ( TLS_EXT_RENEGOTIATION_INFO      ) & 0xFF );

    *p++ = 0x00;
    *p++ = ( ssl->verify_data_len + 1 ) & 0xFF;
    *p++ = ssl->verify_data_len & 0xFF;

    memcpy( p, ssl->own_verify_data, ssl->verify_data_len );

    *olen = 5 + ssl->verify_data_len;
}

static int ssl_write_client_hello( ssl_context *ssl )
{
    int ret;
    size_t i, n, olen, ext_len = 0;
    unsigned char *buf;
    unsigned char *p, *q;
    const int *ciphersuites;
    const ssl_ciphersuite_t *ciphersuite_info;

    SSL_DEBUG_MSG( 2, ( "=> write client hello" ) );

    if( ssl->f_rng == NULL )
    {
        SSL_DEBUG_MSG( 1, ( "no RNG provided") );
        return( POLARSSL_ERR_SSL_NO_RNG );
    }

    if( ssl->renegotiation == SSL_INITIAL_HANDSHAKE )
    {
        ssl->major_ver = ssl->min_major_ver;
        ssl->minor_ver = ssl->min_minor_ver;
    }

    if( ssl->max_major_ver == 0 && ssl->max_minor_ver == 0 )
    {
        ssl->max_major_ver = SSL_MAX_MAJOR_VERSION;
        ssl->max_minor_ver = SSL_MAX_MINOR_VERSION;
    }

    /*
     *     0  .   0   handshake type
     *     1  .   3   handshake length
     *     4  .   5   highest version supported
     *     6  .   9   current UNIX time
     *    10  .  37   random bytes
     */
    buf = ssl->out_msg;
    p = buf + 4;

    *p++ = (unsigned char) ssl->max_major_ver;
    *p++ = (unsigned char) ssl->max_minor_ver;

    SSL_DEBUG_MSG( 3, ( "client hello, max version: [%d:%d]",
                   buf[4], buf[5] ) );

    if( ( ret = ssl->f_rng( ssl->p_rng, p, 4 ) ) != 0 )
        return( ret );

    p += 4;

    if( ( ret = ssl->f_rng( ssl->p_rng, p, 28 ) ) != 0 )
        return( ret );

    p += 28;

    memcpy( ssl->handshake->randbytes, buf + 6, 32 );

    SSL_DEBUG_BUF( 3, "client hello, random bytes", buf + 6, 32 );

    /*
     *    38  .  38   session id length
     *    39  . 39+n  session id
     *   40+n . 41+n  ciphersuitelist length
     *   42+n . ..    ciphersuitelist
     *   ..   . ..    compression methods length
     *   ..   . ..    compression methods
     *   ..   . ..    extensions length
     *   ..   . ..    extensions
     */
    n = ssl->session_negotiate->length;

    if( ssl->renegotiation != SSL_INITIAL_HANDSHAKE || n < 16 || n > 32 ||
        ssl->handshake->resume == 0 )
    {
        n = 0;
    }

    *p++ = (unsigned char) n;

    for( i = 0; i < n; i++ )
        *p++ = ssl->session_negotiate->id[i];

    SSL_DEBUG_MSG( 3, ( "client hello, session id len.: %d", n ) );
    SSL_DEBUG_BUF( 3,   "client hello, session id", buf + 39, n );

    ciphersuites = ssl->ciphersuite_list[ssl->minor_ver];
    n = 0;
    q = p;

    // Skip writing ciphersuite length for now
    p += 2;

    /*
     * Add TLS_EMPTY_RENEGOTIATION_INFO_SCSV
     */
    if( ssl->renegotiation == SSL_INITIAL_HANDSHAKE )
    {
        *p++ = (unsigned char)( SSL_EMPTY_RENEGOTIATION_INFO >> 8 );
        *p++ = (unsigned char)( SSL_EMPTY_RENEGOTIATION_INFO      );
        n++;
    }

    for( i = 0; ciphersuites[i] != 0; i++ )
    {
        ciphersuite_info = ssl_ciphersuite_from_id( ciphersuites[i] );

        if( ciphersuite_info == NULL )
            continue;

        if( ciphersuite_info->min_minor_ver > ssl->max_minor_ver ||
            ciphersuite_info->max_minor_ver < ssl->min_minor_ver )
            continue;

        SSL_DEBUG_MSG( 3, ( "client hello, add ciphersuite: %2d",
                       ciphersuites[i] ) );

        n++;
        *p++ = (unsigned char)( ciphersuites[i] >> 8 );
        *p++ = (unsigned char)( ciphersuites[i]      );
    }

    *q++ = (unsigned char)( n >> 7 );
    *q++ = (unsigned char)( n << 1 );

    SSL_DEBUG_MSG( 3, ( "client hello, got %d ciphersuites", n ) );

    SSL_DEBUG_MSG( 3, ( "client hello, compress len.: %d", 1 ) );
    SSL_DEBUG_MSG( 3, ( "client hello, compress alg.: %d", SSL_COMPRESS_NULL ) );

    *p++ = 1;
    *p++ = SSL_COMPRESS_NULL;

    // First write extensions, then the total length
    //

    ssl_write_renegotiation_ext( ssl, p + 2 + ext_len, &olen );
    ext_len += olen;

    SSL_DEBUG_MSG( 3, ( "client hello, total extension length: %d",
                   ext_len ) );

    *p++ = (unsigned char)( ( ext_len >> 8 ) & 0xFF );
    *p++ = (unsigned char)( ( ext_len      ) & 0xFF );
    p += ext_len;

    ssl->out_msglen  = p - buf;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0]  = SSL_HS_CLIENT_HELLO;

    ssl->state++;

    if( ( ret = ssl_write_record( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_write_record", ret );
        return( ret );
    }

    SSL_DEBUG_MSG( 2, ( "<= write client hello" ) );

    return( 0 );
}

static int ssl_parse_renegotiation_info( ssl_context *ssl,
                                         const unsigned char *buf,
                                         size_t len )
{
    int ret;

    if( ssl->renegotiation == SSL_INITIAL_HANDSHAKE )
    {
        if( len != 1 || buf[0] != 0x0 )
        {
            SSL_DEBUG_MSG( 1, ( "non-zero length renegotiated connection field" ) );

            if( ( ret = ssl_send_fatal_handshake_failure( ssl ) ) != 0 )
                return( ret );

            return( POLARSSL_ERR_SSL_BAD_HS_SERVER_HELLO );
        }

        ssl->secure_renegotiation = SSL_SECURE_RENEGOTIATION;
    }
    else
    {
        /* Check verify-data in constant-time. The length OTOH is no secret */
        if( len    != 1 + ssl->verify_data_len * 2 ||
            buf[0] !=     ssl->verify_data_len * 2 ||
            safer_memcmp( buf + 1,
                          ssl->own_verify_data, ssl->verify_data_len ) != 0 ||
            safer_memcmp( buf + 1 + ssl->verify_data_len,
                          ssl->peer_verify_data, ssl->verify_data_len ) != 0 )
        {
            SSL_DEBUG_MSG( 1, ( "non-matching renegotiated connection field" ) );

            if( ( ret = ssl_send_fatal_handshake_failure( ssl ) ) != 0 )
                return( ret );

            return( POLARSSL_ERR_SSL_BAD_HS_SERVER_HELLO );
        }
    }

    return( 0 );
}

static int ssl_parse_server_hello( ssl_context *ssl )
{
    int ret, i, comp;
    size_t n;
    size_t ext_len = 0;
    unsigned char *buf, *ext;
    int renegotiation_info_seen = 0;
    int handshake_failure = 0;

    SSL_DEBUG_MSG( 2, ( "=> parse server hello" ) );

    /*
     *     0  .   0   handshake type
     *     1  .   3   handshake length
     *     4  .   5   protocol version
     *     6  .   9   UNIX time()
     *    10  .  37   random bytes
     */
    buf = ssl->in_msg;

    if( ( ret = ssl_read_record( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_read_record", ret );
        return( ret );
    }

    if( ssl->in_msgtype != SSL_MSG_HANDSHAKE )
    {
        SSL_DEBUG_MSG( 1, ( "bad server hello message" ) );
        return( POLARSSL_ERR_SSL_UNEXPECTED_MESSAGE );
    }

    SSL_DEBUG_MSG( 3, ( "server hello, chosen version: [%d:%d]",
                   buf[4], buf[5] ) );

    if( ssl->in_hslen < 42 ||
        buf[0] != SSL_HS_SERVER_HELLO ||
        buf[4] != SSL_MAJOR_VERSION_3 )
    {
        SSL_DEBUG_MSG( 1, ( "bad server hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_SERVER_HELLO );
    }

    if( buf[5] > ssl->max_minor_ver )
    {
        SSL_DEBUG_MSG( 1, ( "bad server hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_SERVER_HELLO );
    }

    ssl->minor_ver = buf[5];

    if( ssl->minor_ver < ssl->min_minor_ver )
    {
        SSL_DEBUG_MSG( 1, ( "server only supports ssl smaller than minimum"
                            " [%d:%d] < [%d:%d]", ssl->major_ver, ssl->minor_ver,
                            buf[4], buf[5] ) );

        ssl_send_alert_message( ssl, SSL_ALERT_LEVEL_FATAL,
                                     SSL_ALERT_MSG_PROTOCOL_VERSION );

        return( POLARSSL_ERR_SSL_BAD_HS_PROTOCOL_VERSION );
    }

    memcpy( ssl->handshake->randbytes + 32, buf + 6, 32 );

    n = buf[38];

    SSL_DEBUG_BUF( 3,   "server hello, random bytes", buf + 6, 32 );

    if( n > 32 )
    {
        SSL_DEBUG_MSG( 1, ( "bad server hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_SERVER_HELLO );
    }

    /*
     *    38  .  38   session id length
     *    39  . 38+n  session id
     *   39+n . 40+n  chosen ciphersuite
     *   41+n . 41+n  chosen compression alg.
     *   42+n . 43+n  extensions length
     *   44+n . 44+n+m extensions
     */
    if( ssl->in_hslen > 42 + n )
    {
        ext_len = ( ( buf[42 + n] <<  8 )
                  | ( buf[43 + n]       ) );

        if( ( ext_len > 0 && ext_len < 4 ) ||
            ssl->in_hslen != 44 + n + ext_len )
        {
            SSL_DEBUG_MSG( 1, ( "bad server hello message" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_SERVER_HELLO );
        }
    }

    i = ( buf[39 + n] << 8 ) | buf[40 + n];
    comp = buf[41 + n];

    /*
     * Initialize update checksum functions
     */
    ssl->transform_negotiate->ciphersuite_info = ssl_ciphersuite_from_id( i );

    if( ssl->transform_negotiate->ciphersuite_info == NULL )
    {
        SSL_DEBUG_MSG( 1, ( "ciphersuite info for %04x not found", i ) );
        return( POLARSSL_ERR_SSL_BAD_INPUT_DATA );
    }

    ssl_optimize_checksum( ssl, ssl->transform_negotiate->ciphersuite_info );

    SSL_DEBUG_MSG( 3, ( "server hello, session id len.: %d", n ) );
    SSL_DEBUG_BUF( 3,   "server hello, session id", buf + 39, n );

    /*
     * Check if the session can be resumed
     */
    if( ssl->renegotiation != SSL_INITIAL_HANDSHAKE ||
        ssl->handshake->resume == 0 || n == 0 ||
        ssl->session_negotiate->ciphersuite != i ||
        ssl->session_negotiate->compression != comp ||
        ssl->session_negotiate->length != n ||
        memcmp( ssl->session_negotiate->id, buf + 39, n ) != 0 )
    {
        ssl->state++;
        ssl->handshake->resume = 0;
        ssl->session_negotiate->ciphersuite = i;
        ssl->session_negotiate->compression = comp;
        ssl->session_negotiate->length = n;
        memcpy( ssl->session_negotiate->id, buf + 39, n );
    }
    else
    {
        ssl->state = SSL_SERVER_CHANGE_CIPHER_SPEC;

        if( ( ret = ssl_derive_keys( ssl ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ssl_derive_keys", ret );
            return( ret );
        }
    }

    SSL_DEBUG_MSG( 3, ( "%s session has been resumed",
                   ssl->handshake->resume ? "a" : "no" ) );

    SSL_DEBUG_MSG( 3, ( "server hello, chosen ciphersuite: %d", i ) );
    SSL_DEBUG_MSG( 3, ( "server hello, compress alg.: %d", buf[41 + n] ) );

    i = 0;
    while( 1 )
    {
        if( ssl->ciphersuite_list[ssl->minor_ver][i] == 0 )
        {
            SSL_DEBUG_MSG( 1, ( "bad server hello message" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_SERVER_HELLO );
        }

        if( ssl->ciphersuite_list[ssl->minor_ver][i++] ==
            ssl->session_negotiate->ciphersuite )
        {
            break;
        }
    }

    if( comp != SSL_COMPRESS_NULL
      )
    {
        SSL_DEBUG_MSG( 1, ( "bad server hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_SERVER_HELLO );
    }
    ssl->session_negotiate->compression = comp;

    ext = buf + 44 + n;

    SSL_DEBUG_MSG( 2, ( "server hello, total extension length: %d", ext_len ) );

    while( ext_len )
    {
        unsigned int ext_id   = ( ( ext[0] <<  8 )
                                | ( ext[1]       ) );
        unsigned int ext_size = ( ( ext[2] <<  8 )
                                | ( ext[3]       ) );

        if( ext_size + 4 > ext_len )
        {
            SSL_DEBUG_MSG( 1, ( "bad server hello message" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_SERVER_HELLO );
        }

        switch( ext_id )
        {
        case TLS_EXT_RENEGOTIATION_INFO:
            SSL_DEBUG_MSG( 3, ( "found renegotiation extension" ) );
            renegotiation_info_seen = 1;

            if( ( ret = ssl_parse_renegotiation_info( ssl, ext + 4, ext_size ) ) != 0 )
                return( ret );

            break;

        default:
            SSL_DEBUG_MSG( 3, ( "unknown extension found: %d (ignoring)",
                           ext_id ) );
        }

        ext_len -= 4 + ext_size;
        ext += 4 + ext_size;

        if( ext_len > 0 && ext_len < 4 )
        {
            SSL_DEBUG_MSG( 1, ( "bad server hello message" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_SERVER_HELLO );
        }
    }

    /*
     * Renegotiation security checks
     */
    if( ssl->secure_renegotiation == SSL_LEGACY_RENEGOTIATION &&
        ssl->allow_legacy_renegotiation == SSL_LEGACY_BREAK_HANDSHAKE )
    {
        SSL_DEBUG_MSG( 1, ( "legacy renegotiation, breaking off handshake" ) );
        handshake_failure = 1;
    }
    else if( ssl->renegotiation == SSL_RENEGOTIATION &&
             ssl->secure_renegotiation == SSL_SECURE_RENEGOTIATION &&
             renegotiation_info_seen == 0 )
    {
        SSL_DEBUG_MSG( 1, ( "renegotiation_info extension missing (secure)" ) );
        handshake_failure = 1;
    }
    else if( ssl->renegotiation == SSL_RENEGOTIATION &&
             ssl->secure_renegotiation == SSL_LEGACY_RENEGOTIATION &&
             ssl->allow_legacy_renegotiation == SSL_LEGACY_NO_RENEGOTIATION )
    {
        SSL_DEBUG_MSG( 1, ( "legacy renegotiation not allowed" ) );
        handshake_failure = 1;
    }
    else if( ssl->renegotiation == SSL_RENEGOTIATION &&
             ssl->secure_renegotiation == SSL_LEGACY_RENEGOTIATION &&
             renegotiation_info_seen == 1 )
    {
        SSL_DEBUG_MSG( 1, ( "renegotiation_info extension present (legacy)" ) );
        handshake_failure = 1;
    }

    if( handshake_failure == 1 )
    {
        if( ( ret = ssl_send_fatal_handshake_failure( ssl ) ) != 0 )
            return( ret );

        return( POLARSSL_ERR_SSL_BAD_HS_SERVER_HELLO );
    }

    SSL_DEBUG_MSG( 2, ( "<= parse server hello" ) );

    return( 0 );
}

#if defined(POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED) ||                     \
    defined(POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED) ||                   \
    defined(POLARSSL_KEY_EXCHANGE_ECDHE_PSK_ENABLED) ||                     \
    defined(POLARSSL_KEY_EXCHANGE_ECDH_RSA_ENABLED) ||                      \
    defined(POLARSSL_KEY_EXCHANGE_ECDH_ECDSA_ENABLED)
static int ssl_check_server_ecdh_params( const ssl_context *ssl )
{
    const ecp_curve_info *curve_info;

    curve_info = ecp_curve_info_from_grp_id( ssl->handshake->ecdh_ctx.grp.id );
    if( curve_info == NULL )
    {
        SSL_DEBUG_MSG( 1, ( "Should never happen" ) );
        return( -1 );
    }

    SSL_DEBUG_MSG( 2, ( "ECDH curve: %s", curve_info->name ) );

    if( ssl->handshake->ecdh_ctx.grp.nbits < 163 ||
        ssl->handshake->ecdh_ctx.grp.nbits > 521 )
        return( -1 );

    SSL_DEBUG_ECP( 3, "ECDH: Qp", &ssl->handshake->ecdh_ctx.Qp );

    return( 0 );
}
#endif


#if defined(POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED) ||                     \
    defined(POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED) ||                   \
    defined(POLARSSL_KEY_EXCHANGE_ECDHE_PSK_ENABLED)
static int ssl_parse_server_ecdh_params( ssl_context *ssl,
                                         unsigned char **p,
                                         unsigned char *end )
{
    int ret = POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE;

    /*
     * Ephemeral ECDH parameters:
     *
     * struct {
     *     ECParameters curve_params;
     *     ECPoint      public;
     * } ServerECDHParams;
     */
    if( ( ret = ecdh_read_params( &ssl->handshake->ecdh_ctx,
                                  (const unsigned char **) p, end ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, ( "ecdh_read_params" ), ret );
        return( ret );
    }

    if( ssl_check_server_ecdh_params( ssl ) != 0 )
    {
        SSL_DEBUG_MSG( 1, ( "bad server key exchange message (ECDHE curve)" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_SERVER_KEY_EXCHANGE );
    }

    return( ret );
}
#endif /* POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED ||
          POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED ||
          POLARSSL_KEY_EXCHANGE_ECDHE_PSK_ENABLED */

static int ssl_parse_server_key_exchange( ssl_context *ssl )
{
    int ret;
    const ssl_ciphersuite_t *ciphersuite_info = ssl->transform_negotiate->ciphersuite_info;
    unsigned char *p, *end;
#if defined(POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED) ||                       \
    defined(POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED) ||                     \
    defined(POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED)
    size_t sig_len, params_len;
    unsigned char hash[64];
    md_type_t md_alg = POLARSSL_MD_NONE;
    size_t hashlen;
    pk_type_t pk_alg = POLARSSL_PK_NONE;
#endif

    SSL_DEBUG_MSG( 2, ( "=> parse server key exchange" ) );

    if( ( ret = ssl_read_record( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_read_record", ret );
        return( ret );
    }

    if( ssl->in_msgtype != SSL_MSG_HANDSHAKE )
    {
        SSL_DEBUG_MSG( 1, ( "bad server key exchange message" ) );
        return( POLARSSL_ERR_SSL_UNEXPECTED_MESSAGE );
    }

    /*
     * ServerKeyExchange may be skipped with PSK and RSA-PSK when the server
     * doesn't use a psk_identity_hint
     */
    if( ssl->in_msg[0] != SSL_HS_SERVER_KEY_EXCHANGE )
    {
        if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_PSK ||
            ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA_PSK )
        {
            ssl->record_read = 1;
            goto exit;
        }

        SSL_DEBUG_MSG( 1, ( "bad server key exchange message" ) );
        return( POLARSSL_ERR_SSL_UNEXPECTED_MESSAGE );
    }

    p   = ssl->in_msg + 4;
    end = ssl->in_msg + ssl->in_hslen;
    SSL_DEBUG_BUF( 3,   "server key exchange", p, ssl->in_hslen - 4 );

#if defined(POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED) ||                     \
    defined(POLARSSL_KEY_EXCHANGE_ECDHE_PSK_ENABLED) ||                     \
    defined(POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED)
    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_RSA ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA )
    {
        if( ssl_parse_server_ecdh_params( ssl, &p, end ) != 0 )
        {
            SSL_DEBUG_MSG( 1, ( "bad server key exchange message" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_SERVER_KEY_EXCHANGE );
        }
    }
    else
#endif /* POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED ||
          POLARSSL_KEY_EXCHANGE_ECDHE_PSK_ENABLED ||
          POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED */
    {
        SSL_DEBUG_MSG( 1, ( "should never happen" ) );
        return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
    }

#if defined(POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED) ||                       \
    defined(POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED) ||                     \
    defined(POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED)
    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_RSA ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_RSA ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA )
    {
        params_len = p - ( ssl->in_msg + 4 );

        /*
         * Handle the digitally-signed structure
         */
#if defined(POLARSSL_SSL_PROTO_TLS1_2)
        if( ssl->minor_ver == SSL_MINOR_VERSION_3 )
        {
            if( ssl_parse_signature_algorithm( ssl, &p, end,
                                               &md_alg, &pk_alg ) != 0 )
            {
                SSL_DEBUG_MSG( 1, ( "bad server key exchange message" ) );
                return( POLARSSL_ERR_SSL_BAD_HS_SERVER_KEY_EXCHANGE );
            }

            if( pk_alg != ssl_get_ciphersuite_sig_pk_alg( ciphersuite_info ) )
            {
                SSL_DEBUG_MSG( 1, ( "bad server key exchange message" ) );
                return( POLARSSL_ERR_SSL_BAD_HS_SERVER_KEY_EXCHANGE );
            }
        }
        else
#endif
#if defined(POLARSSL_SSL_PROTO_SSL3) || defined(POLARSSL_SSL_PROTO_TLS1) || \
    defined(POLARSSL_SSL_PROTO_TLS1_1)
        if( ssl->minor_ver < SSL_MINOR_VERSION_3 )
        {
            pk_alg = ssl_get_ciphersuite_sig_pk_alg( ciphersuite_info );

            /* Default hash for ECDSA is SHA-1 */
            if( pk_alg == POLARSSL_PK_ECDSA && md_alg == POLARSSL_MD_NONE )
                md_alg = POLARSSL_MD_SHA1;
        }
        else
#endif
        {
            SSL_DEBUG_MSG( 1, ( "should never happen" ) );
            return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
        }

        /*
         * Read signature
         */
        sig_len = ( p[0] << 8 ) | p[1];
        p += 2;

        if( end != p + sig_len )
        {
            SSL_DEBUG_MSG( 1, ( "bad server key exchange message" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_SERVER_KEY_EXCHANGE );
        }

        SSL_DEBUG_BUF( 3, "signature", p, sig_len );

        /*
         * Compute the hash that has been signed
         */
#if defined(POLARSSL_SSL_PROTO_SSL3) || defined(POLARSSL_SSL_PROTO_TLS1) || \
    defined(POLARSSL_SSL_PROTO_TLS1_1)
        if( md_alg == POLARSSL_MD_NONE )
        {
            md5_context md5;
            sha1_context sha1;

            hashlen = 36;

            /*
             * digitally-signed struct {
             *     opaque md5_hash[16];
             *     opaque sha_hash[20];
             * };
             *
             * md5_hash
             *     MD5(ClientHello.random + ServerHello.random
             *                            + ServerParams);
             * sha_hash
             *     SHA(ClientHello.random + ServerHello.random
             *                            + ServerParams);
             */
            md5_starts( &md5 );
            md5_update( &md5, ssl->handshake->randbytes, 64 );
            md5_update( &md5, ssl->in_msg + 4, params_len );
            md5_finish( &md5, hash );

            sha1_starts( &sha1 );
            sha1_update( &sha1, ssl->handshake->randbytes, 64 );
            sha1_update( &sha1, ssl->in_msg + 4, params_len );
            sha1_finish( &sha1, hash + 16 );
        }
        else
#endif /* POLARSSL_SSL_PROTO_SSL3 || POLARSSL_SSL_PROTO_TLS1 || \
          POLARSSL_SSL_PROTO_TLS1_1 */
#if defined(POLARSSL_SSL_PROTO_TLS1) || defined(POLARSSL_SSL_PROTO_TLS1_1) || \
    defined(POLARSSL_SSL_PROTO_TLS1_2)
        if( md_alg != POLARSSL_MD_NONE )
        {
            md_context_t ctx;

            /* Info from md_alg will be used instead */
            hashlen = 0;

            /*
             * digitally-signed struct {
             *     opaque client_random[32];
             *     opaque server_random[32];
             *     ServerDHParams params;
             * };
             */
            if( ( ret = md_init_ctx( &ctx, md_info_from_type( md_alg ) ) ) != 0 )
            {
                SSL_DEBUG_RET( 1, "md_init_ctx", ret );
                return( ret );
            }

            md_starts( &ctx );
            md_update( &ctx, ssl->handshake->randbytes, 64 );
            md_update( &ctx, ssl->in_msg + 4, params_len );
            md_finish( &ctx, hash );
            md_free_ctx( &ctx );
        }
        else
#endif /* POLARSSL_SSL_PROTO_TLS1 || POLARSSL_SSL_PROTO_TLS1_1 || \
          POLARSSL_SSL_PROTO_TLS1_2 */
        {
            SSL_DEBUG_MSG( 1, ( "should never happen" ) );
            return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
        }

        SSL_DEBUG_BUF( 3, "parameters hash", hash, hashlen != 0 ? hashlen :
                (unsigned int) ( md_info_from_type( md_alg ) )->size );

        /*
         * Verify signature
         */
        if( ! pk_can_do( &ssl->session_negotiate->peer_cert->pk, pk_alg ) )
        {
            SSL_DEBUG_MSG( 1, ( "bad server key exchange message" ) );
            return( POLARSSL_ERR_SSL_PK_TYPE_MISMATCH );
        }

        if( ( ret = pk_verify( &ssl->session_negotiate->peer_cert->pk,
                               md_alg, hash, hashlen, p, sig_len ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "pk_verify", ret );
            return( ret );
        }
    }
#endif /* POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED ||
          POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED ||
          POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED */

exit:
    ssl->state++;

    SSL_DEBUG_MSG( 2, ( "<= parse server key exchange" ) );

    return( 0 );
}

#if !defined(POLARSSL_KEY_EXCHANGE_RSA_ENABLED)       && \
    !defined(POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED)   && \
    !defined(POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED) && \
    !defined(POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED)
static int ssl_parse_certificate_request( ssl_context *ssl )
{
    int ret = POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE;
    const ssl_ciphersuite_t *ciphersuite_info = ssl->transform_negotiate->ciphersuite_info;

    SSL_DEBUG_MSG( 2, ( "=> parse certificate request" ) );

    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_PSK )
    {
        SSL_DEBUG_MSG( 2, ( "<= skip parse certificate request" ) );
        ssl->state++;
        return( 0 );
    }

    SSL_DEBUG_MSG( 1, ( "should not happen" ) );
    return( ret );
}
#else
static int ssl_parse_certificate_request( ssl_context *ssl )
{
    int ret;
    unsigned char *buf, *p;
    size_t n = 0, m = 0;
    size_t cert_type_len = 0, dn_len = 0;
    const ssl_ciphersuite_t *ciphersuite_info = ssl->transform_negotiate->ciphersuite_info;

    SSL_DEBUG_MSG( 2, ( "=> parse certificate request" ) );

    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_PSK )
    {
        SSL_DEBUG_MSG( 2, ( "<= skip parse certificate request" ) );
        ssl->state++;
        return( 0 );
    }

    /*
     *     0  .   0   handshake type
     *     1  .   3   handshake length
     *     4  .   4   cert type count
     *     5  .. m-1  cert types
     *     m  .. m+1  sig alg length (TLS 1.2 only)
     *    m+1 .. n-1  SignatureAndHashAlgorithms (TLS 1.2 only)
     *     n  .. n+1  length of all DNs
     *    n+2 .. n+3  length of DN 1
     *    n+4 .. ...  Distinguished Name #1
     *    ... .. ...  length of DN 2, etc.
     */
    if( ssl->record_read == 0 )
    {
        if( ( ret = ssl_read_record( ssl ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ssl_read_record", ret );
            return( ret );
        }

        if( ssl->in_msgtype != SSL_MSG_HANDSHAKE )
        {
            SSL_DEBUG_MSG( 1, ( "bad certificate request message" ) );
            return( POLARSSL_ERR_SSL_UNEXPECTED_MESSAGE );
        }

        ssl->record_read = 1;
    }

    ssl->client_auth = 0;
    ssl->state++;

    if( ssl->in_msg[0] == SSL_HS_CERTIFICATE_REQUEST )
        ssl->client_auth++;

    SSL_DEBUG_MSG( 3, ( "got %s certificate request",
                        ssl->client_auth ? "a" : "no" ) );

    if( ssl->client_auth == 0 )
        goto exit;

    ssl->record_read = 0;

    // TODO: handshake_failure alert for an anonymous server to request
    // client authentication

    buf = ssl->in_msg;

    // Retrieve cert types
    //
    cert_type_len = buf[4];
    n = cert_type_len;

    if( ssl->in_hslen < 6 + n )
    {
        SSL_DEBUG_MSG( 1, ( "bad certificate request message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CERTIFICATE_REQUEST );
    }

    p = buf + 5;
    while( cert_type_len > 0 )
    {
#if defined(POLARSSL_RSA_C)
        if( *p == SSL_CERT_TYPE_RSA_SIGN &&
            pk_can_do( ssl_own_key( ssl ), POLARSSL_PK_RSA ) )
        {
            ssl->handshake->cert_type = SSL_CERT_TYPE_RSA_SIGN;
            break;
        }
        else
#endif
#if defined(POLARSSL_ECDSA_C)
        if( *p == SSL_CERT_TYPE_ECDSA_SIGN &&
            pk_can_do( ssl_own_key( ssl ), POLARSSL_PK_ECDSA ) )
        {
            ssl->handshake->cert_type = SSL_CERT_TYPE_ECDSA_SIGN;
            break;
        }
        else
#endif
        {
            ; /* Unsupported cert type, ignore */
        }

        cert_type_len--;
        p++;
    }

#if defined(POLARSSL_SSL_PROTO_TLS1_2)
    if( ssl->minor_ver == SSL_MINOR_VERSION_3 )
    {
        /* Ignored, see comments about hash in write_certificate_verify */
        // TODO: should check the signature part against our pk_key though
        size_t sig_alg_len = ( ( buf[5 + n] <<  8 )
                             | ( buf[6 + n]       ) );

        p = buf + 7 + n;
        m += 2;
        n += sig_alg_len;

        if( ssl->in_hslen < 6 + n )
        {
            SSL_DEBUG_MSG( 1, ( "bad certificate request message" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_CERTIFICATE_REQUEST );
        }
    }
#endif /* POLARSSL_SSL_PROTO_TLS1_2 */

    /* Ignore certificate_authorities, we only have one cert anyway */
    // TODO: should not send cert if no CA matches
    dn_len = ( ( buf[5 + m + n] <<  8 )
             | ( buf[6 + m + n]       ) );

    n += dn_len;
    if( ssl->in_hslen != 7 + m + n )
    {
        SSL_DEBUG_MSG( 1, ( "bad certificate request message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CERTIFICATE_REQUEST );
    }

exit:
    SSL_DEBUG_MSG( 2, ( "<= parse certificate request" ) );

    return( 0 );
}
#endif /* !POLARSSL_KEY_EXCHANGE_RSA_ENABLED &&
          !POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED &&
          !POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED &&
          !POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED */

static int ssl_parse_server_hello_done( ssl_context *ssl )
{
    int ret;

    SSL_DEBUG_MSG( 2, ( "=> parse server hello done" ) );

    if( ssl->record_read == 0 )
    {
        if( ( ret = ssl_read_record( ssl ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ssl_read_record", ret );
            return( ret );
        }

        if( ssl->in_msgtype != SSL_MSG_HANDSHAKE )
        {
            SSL_DEBUG_MSG( 1, ( "bad server hello done message" ) );
            return( POLARSSL_ERR_SSL_UNEXPECTED_MESSAGE );
        }
    }
    ssl->record_read = 0;

    if( ssl->in_hslen  != 4 ||
        ssl->in_msg[0] != SSL_HS_SERVER_HELLO_DONE )
    {
        SSL_DEBUG_MSG( 1, ( "bad server hello done message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_SERVER_HELLO_DONE );
    }

    ssl->state++;

    SSL_DEBUG_MSG( 2, ( "<= parse server hello done" ) );

    return( 0 );
}

static int ssl_write_client_key_exchange( ssl_context *ssl )
{
    int ret;
    size_t i, n;
    const ssl_ciphersuite_t *ciphersuite_info = ssl->transform_negotiate->ciphersuite_info;

    SSL_DEBUG_MSG( 2, ( "=> write client key exchange" ) );

#if defined(POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED) ||                     \
    defined(POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED) ||                   \
    defined(POLARSSL_KEY_EXCHANGE_ECDH_RSA_ENABLED) ||                      \
    defined(POLARSSL_KEY_EXCHANGE_ECDH_ECDSA_ENABLED)
    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_RSA ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDH_RSA ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDH_ECDSA )
    {
        /*
         * ECDH key exchange -- send client public value
         */
        i = 4;

        ret = ecdh_make_public( &ssl->handshake->ecdh_ctx,
                                &n,
                                &ssl->out_msg[i], 1000,
                                ssl->f_rng, ssl->p_rng );
        if( ret != 0 )
        {
            SSL_DEBUG_RET( 1, "ecdh_make_public", ret );
            return( ret );
        }

        SSL_DEBUG_ECP( 3, "ECDH: Q", &ssl->handshake->ecdh_ctx.Q );

        if( ( ret = ecdh_calc_secret( &ssl->handshake->ecdh_ctx,
                                      &ssl->handshake->pmslen,
                                       ssl->handshake->premaster,
                                       POLARSSL_MPI_MAX_SIZE,
                                       ssl->f_rng, ssl->p_rng ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ecdh_calc_secret", ret );
            return( ret );
        }

        SSL_DEBUG_MPI( 3, "ECDH: z", &ssl->handshake->ecdh_ctx.z );
    }
    else
#endif /* POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED ||
          POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED ||
          POLARSSL_KEY_EXCHANGE_ECDH_RSA_ENABLED ||
          POLARSSL_KEY_EXCHANGE_ECDH_ECDSA_ENABLED */
#if defined(POLARSSL_KEY_EXCHANGE__SOME__PSK_ENABLED)
    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_PSK )
    {
        /*
         * opaque psk_identity<0..2^16-1>;
         */
        if( ssl->psk == NULL || ssl->psk_identity == NULL )
            return( POLARSSL_ERR_SSL_PRIVATE_KEY_REQUIRED );

        i = 4;
        n = ssl->psk_identity_len;
        ssl->out_msg[i++] = (unsigned char)( n >> 8 );
        ssl->out_msg[i++] = (unsigned char)( n      );

        memcpy( ssl->out_msg + i, ssl->psk_identity, ssl->psk_identity_len );
        i += ssl->psk_identity_len;

#if defined(POLARSSL_KEY_EXCHANGE_PSK_ENABLED)
        if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_PSK )
        {
            n = 0;
        }
        else
#endif
#if defined(POLARSSL_KEY_EXCHANGE_RSA_PSK_ENABLED)
        if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA_PSK )
        {
            if( ( ret = ssl_write_encrypted_pms( ssl, i, &n, 2 ) ) != 0 )
                return( ret );
        }
        else
#endif
#if defined(POLARSSL_KEY_EXCHANGE_DHE_PSK_ENABLED)
        if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_PSK )
        {
            /*
             * ClientDiffieHellmanPublic public (DHM send G^X mod P)
             */
            n = ssl->handshake->dhm_ctx.len;
            ssl->out_msg[i++] = (unsigned char)( n >> 8 );
            ssl->out_msg[i++] = (unsigned char)( n      );

            ret = dhm_make_public( &ssl->handshake->dhm_ctx,
                    (int) mpi_size( &ssl->handshake->dhm_ctx.P ),
                    &ssl->out_msg[i], n,
                    ssl->f_rng, ssl->p_rng );
            if( ret != 0 )
            {
                SSL_DEBUG_RET( 1, "dhm_make_public", ret );
                return( ret );
            }
        }
        else
#endif /* POLARSSL_KEY_EXCHANGE_DHE_PSK_ENABLED */
#if defined(POLARSSL_KEY_EXCHANGE_ECDHE_PSK_ENABLED)
        if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_PSK )
        {
            /*
             * ClientECDiffieHellmanPublic public;
             */
            ret = ecdh_make_public( &ssl->handshake->ecdh_ctx, &n,
                    &ssl->out_msg[i], SSL_MAX_CONTENT_LEN - i,
                    ssl->f_rng, ssl->p_rng );
            if( ret != 0 )
            {
                SSL_DEBUG_RET( 1, "ecdh_make_public", ret );
                return( ret );
            }

            SSL_DEBUG_ECP( 3, "ECDH: Q", &ssl->handshake->ecdh_ctx.Q );
        }
        else
#endif /* POLARSSL_KEY_EXCHANGE_ECDHE_PSK_ENABLED */
        {
            SSL_DEBUG_MSG( 1, ( "should never happen" ) );
            return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
        }

        if( ( ret = ssl_psk_derive_premaster( ssl,
                        ciphersuite_info->key_exchange ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ssl_psk_derive_premaster", ret );
            return( ret );
        }
    }
    else
#endif /* POLARSSL_KEY_EXCHANGE__SOME__PSK_ENABLED */
#if defined(POLARSSL_KEY_EXCHANGE_RSA_ENABLED)
    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA )
    {
        i = 4;
        if( ( ret = ssl_write_encrypted_pms( ssl, i, &n, 0 ) ) != 0 )
            return( ret );
    }
    else
#endif /* POLARSSL_KEY_EXCHANGE_RSA_ENABLED */
    {
        ((void) ciphersuite_info);
        SSL_DEBUG_MSG( 1, ( "should never happen" ) );
        return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
    }

    if( ( ret = ssl_derive_keys( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_derive_keys", ret );
        return( ret );
    }

    ssl->out_msglen  = i + n;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0]  = SSL_HS_CLIENT_KEY_EXCHANGE;

    ssl->state++;

    if( ( ret = ssl_write_record( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_write_record", ret );
        return( ret );
    }

    SSL_DEBUG_MSG( 2, ( "<= write client key exchange" ) );

    return( 0 );
}

#if !defined(POLARSSL_KEY_EXCHANGE_RSA_ENABLED)       && \
    !defined(POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED)   && \
    !defined(POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED) && \
    !defined(POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED)
static int ssl_write_certificate_verify( ssl_context *ssl )
{
    int ret = POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE;
    const ssl_ciphersuite_t *ciphersuite_info = ssl->transform_negotiate->ciphersuite_info;

    SSL_DEBUG_MSG( 2, ( "=> write certificate verify" ) );

    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_PSK )
    {
        SSL_DEBUG_MSG( 2, ( "<= skip write certificate verify" ) );
        ssl->state++;
        return( 0 );
    }

    SSL_DEBUG_MSG( 1, ( "should not happen" ) );
    return( ret );
}
#else
static int ssl_write_certificate_verify( ssl_context *ssl )
{
    int ret = POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE;
    const ssl_ciphersuite_t *ciphersuite_info = ssl->transform_negotiate->ciphersuite_info;
    size_t n = 0, offset = 0;
    unsigned char hash[48];
    unsigned char *hash_start = hash;
    md_type_t md_alg = POLARSSL_MD_NONE;
    unsigned int hashlen;

    SSL_DEBUG_MSG( 2, ( "=> write certificate verify" ) );

    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_PSK )
    {
        SSL_DEBUG_MSG( 2, ( "<= skip write certificate verify" ) );
        ssl->state++;
        return( 0 );
    }

    if( ssl->client_auth == 0 || ssl_own_cert( ssl ) == NULL )
    {
        SSL_DEBUG_MSG( 2, ( "<= skip write certificate verify" ) );
        ssl->state++;
        return( 0 );
    }

    if( ssl_own_key( ssl ) == NULL )
    {
        SSL_DEBUG_MSG( 1, ( "got no private key" ) );
        return( POLARSSL_ERR_SSL_PRIVATE_KEY_REQUIRED );
    }

    /*
     * Make an RSA signature of the handshake digests
     */
    ssl->handshake->calc_verify( ssl, hash );

#if defined(POLARSSL_SSL_PROTO_SSL3) || defined(POLARSSL_SSL_PROTO_TLS1) || \
    defined(POLARSSL_SSL_PROTO_TLS1_1)
    if( ssl->minor_ver != SSL_MINOR_VERSION_3 )
    {
        /*
         * digitally-signed struct {
         *     opaque md5_hash[16];
         *     opaque sha_hash[20];
         * };
         *
         * md5_hash
         *     MD5(handshake_messages);
         *
         * sha_hash
         *     SHA(handshake_messages);
         */
        hashlen = 36;
        md_alg = POLARSSL_MD_NONE;

        /*
         * For ECDSA, default hash is SHA-1 only
         */
        if( pk_can_do( ssl_own_key( ssl ), POLARSSL_PK_ECDSA ) )
        {
            hash_start += 16;
            hashlen -= 16;
            md_alg = POLARSSL_MD_SHA1;
        }
    }
    else
#endif /* POLARSSL_SSL_PROTO_SSL3 || POLARSSL_SSL_PROTO_TLS1 || \
          POLARSSL_SSL_PROTO_TLS1_1 */
#if defined(POLARSSL_SSL_PROTO_TLS1_2)
    if( ssl->minor_ver == SSL_MINOR_VERSION_3 )
    {
        /*
         * digitally-signed struct {
         *     opaque handshake_messages[handshake_messages_length];
         * };
         *
         * Taking shortcut here. We assume that the server always allows the
         * PRF Hash function and has sent it in the allowed signature
         * algorithms list received in the Certificate Request message.
         *
         * Until we encounter a server that does not, we will take this
         * shortcut.
         *
         * Reason: Otherwise we should have running hashes for SHA512 and SHA224
         *         in order to satisfy 'weird' needs from the server side.
         */
        if( ssl->transform_negotiate->ciphersuite_info->mac ==
            POLARSSL_MD_SHA384 )
        {
            md_alg = POLARSSL_MD_SHA384;
            ssl->out_msg[4] = SSL_HASH_SHA384;
        }
        else
        {
            md_alg = POLARSSL_MD_SHA256;
            ssl->out_msg[4] = SSL_HASH_SHA256;
        }
        ssl->out_msg[5] = ssl_sig_from_pk( ssl_own_key( ssl ) );

        /* Info from md_alg will be used instead */
        hashlen = 0;
        offset = 2;
    }
    else
#endif /* POLARSSL_SSL_PROTO_TLS1_2 */
    {
        SSL_DEBUG_MSG( 1, ( "should never happen" ) );
        return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
    }

    if( ( ret = pk_sign( ssl_own_key( ssl ), md_alg, hash_start, hashlen,
                         ssl->out_msg + 6 + offset, &n,
                         ssl->f_rng, ssl->p_rng ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "pk_sign", ret );
        return( ret );
    }

    ssl->out_msg[4 + offset] = (unsigned char)( n >> 8 );
    ssl->out_msg[5 + offset] = (unsigned char)( n      );

    ssl->out_msglen  = 6 + n + offset;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0]  = SSL_HS_CERTIFICATE_VERIFY;

    ssl->state++;

    if( ( ret = ssl_write_record( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_write_record", ret );
        return( ret );
    }

    SSL_DEBUG_MSG( 2, ( "<= write certificate verify" ) );

    return( ret );
}
#endif /* !POLARSSL_KEY_EXCHANGE_RSA_ENABLED &&
          !POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED &&
          !POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED */

/*
 * SSL handshake -- client side -- single step
 */
int ssl_handshake_client_step( ssl_context *ssl )
{
    int ret = 0;

    if( ssl->state == SSL_HANDSHAKE_OVER )
        return( POLARSSL_ERR_SSL_BAD_INPUT_DATA );

    SSL_DEBUG_MSG( 2, ( "client state: %d", ssl->state ) );

    if( ( ret = ssl_flush_output( ssl ) ) != 0 )
        return( ret );

    switch( ssl->state )
    {
        case SSL_HELLO_REQUEST:
            ssl->state = SSL_CLIENT_HELLO;
            break;

       /*
        *  ==>   ClientHello
        */
       case SSL_CLIENT_HELLO:
           ret = ssl_write_client_hello( ssl );
           break;

       /*
        *  <==   ServerHello
        *        Certificate
        *      ( ServerKeyExchange  )
        *      ( CertificateRequest )
        *        ServerHelloDone
        */
       case SSL_SERVER_HELLO:
           ret = ssl_parse_server_hello( ssl );
           break;

       case SSL_SERVER_CERTIFICATE:
           ret = ssl_parse_certificate( ssl );
           break;

       case SSL_SERVER_KEY_EXCHANGE:
           ret = ssl_parse_server_key_exchange( ssl );
           break;

       case SSL_CERTIFICATE_REQUEST:
           ret = ssl_parse_certificate_request( ssl );
           break;

       case SSL_SERVER_HELLO_DONE:
           ret = ssl_parse_server_hello_done( ssl );
           break;

       /*
        *  ==> ( Certificate/Alert  )
        *        ClientKeyExchange
        *      ( CertificateVerify  )
        *        ChangeCipherSpec
        *        Finished
        */
       case SSL_CLIENT_CERTIFICATE:
           ret = ssl_write_certificate( ssl );
           break;

       case SSL_CLIENT_KEY_EXCHANGE:
           ret = ssl_write_client_key_exchange( ssl );
           break;

       case SSL_CERTIFICATE_VERIFY:
           ret = ssl_write_certificate_verify( ssl );
           break;

       case SSL_CLIENT_CHANGE_CIPHER_SPEC:
           ret = ssl_write_change_cipher_spec( ssl );
           break;

       case SSL_CLIENT_FINISHED:
           ret = ssl_write_finished( ssl );
           break;

       /*
        *  <==   ( NewSessionTicket )
        *        ChangeCipherSpec
        *        Finished
        */
       case SSL_SERVER_CHANGE_CIPHER_SPEC:
               ret = ssl_parse_change_cipher_spec( ssl );
           break;

       case SSL_SERVER_FINISHED:
           ret = ssl_parse_finished( ssl );
           break;

       case SSL_FLUSH_BUFFERS:
           SSL_DEBUG_MSG( 2, ( "handshake: done" ) );
           ssl->state = SSL_HANDSHAKE_WRAPUP;
           break;

       case SSL_HANDSHAKE_WRAPUP:
           ssl_handshake_wrapup( ssl );
           break;

       default:
           SSL_DEBUG_MSG( 1, ( "invalid state %d", ssl->state ) );
           return( POLARSSL_ERR_SSL_BAD_INPUT_DATA );
   }

    return( ret );
}
#endif
