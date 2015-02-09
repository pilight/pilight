/*
 *  Public Key layer for writing key files and structures
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

#if defined(POLARSSL_PK_WRITE_C)

#include "pk.h"
#include "asn1write.h"
#include "oid.h"

#if defined(POLARSSL_RSA_C)
#include "rsa.h"
#endif

#include <stdlib.h>
#define polarssl_malloc     malloc
#define polarssl_free       free

#if defined(POLARSSL_RSA_C)
/*
 *  RSAPublicKey ::= SEQUENCE {
 *      modulus           INTEGER,  -- n
 *      publicExponent    INTEGER   -- e
 *  }
 */
static int pk_write_rsa_pubkey( unsigned char **p, unsigned char *start,
                                  rsa_context *rsa )
{
    int ret;
    size_t len = 0;

    ASN1_CHK_ADD( len, asn1_write_mpi( p, start, &rsa->E ) );
    ASN1_CHK_ADD( len, asn1_write_mpi( p, start, &rsa->N ) );

    ASN1_CHK_ADD( len, asn1_write_len( p, start, len ) );
    ASN1_CHK_ADD( len, asn1_write_tag( p, start, ASN1_CONSTRUCTED | ASN1_SEQUENCE ) );

    return( (int) len );
}
#endif /* POLARSSL_RSA_C */

int pk_write_pubkey( unsigned char **p, unsigned char *start,
                     const pk_context *key )
{
    int ret;
    size_t len = 0;

#if defined(POLARSSL_RSA_C)
    if( pk_get_type( key ) == POLARSSL_PK_RSA )
        ASN1_CHK_ADD( len, pk_write_rsa_pubkey( p, start, pk_rsa( *key ) ) );
    else
#endif
        return( POLARSSL_ERR_PK_FEATURE_UNAVAILABLE );

    return( (int) len );
}

int pk_write_pubkey_der( pk_context *key, unsigned char *buf, size_t size )
{
    int ret;
    unsigned char *c;
    size_t len = 0, par_len = 0, oid_len;
    const char *oid;

    c = buf + size;

    ASN1_CHK_ADD( len, pk_write_pubkey( &c, buf, key ) );

    if( c - buf < 1 )
        return( POLARSSL_ERR_ASN1_BUF_TOO_SMALL );

    /*
     *  SubjectPublicKeyInfo  ::=  SEQUENCE  {
     *       algorithm            AlgorithmIdentifier,
     *       subjectPublicKey     BIT STRING }
     */
    *--c = 0;
    len += 1;

    ASN1_CHK_ADD( len, asn1_write_len( &c, buf, len ) );
    ASN1_CHK_ADD( len, asn1_write_tag( &c, buf, ASN1_BIT_STRING ) );

    if( ( ret = oid_get_oid_by_pk_alg( pk_get_type( key ),
                                       &oid, &oid_len ) ) != 0 )
    {
        return( ret );
    }

    ASN1_CHK_ADD( len, asn1_write_algorithm_identifier( &c, buf, oid, oid_len,
                                                        par_len ) );

    ASN1_CHK_ADD( len, asn1_write_len( &c, buf, len ) );
    ASN1_CHK_ADD( len, asn1_write_tag( &c, buf, ASN1_CONSTRUCTED | ASN1_SEQUENCE ) );

    return( (int) len );
}

int pk_write_key_der( pk_context *key, unsigned char *buf, size_t size )
{
    int ret;
    unsigned char *c = buf + size;
    size_t len = 0;

#if defined(POLARSSL_RSA_C)
    if( pk_get_type( key ) == POLARSSL_PK_RSA )
    {
        rsa_context *rsa = pk_rsa( *key );

        ASN1_CHK_ADD( len, asn1_write_mpi( &c, buf, &rsa->QP ) );
        ASN1_CHK_ADD( len, asn1_write_mpi( &c, buf, &rsa->DQ ) );
        ASN1_CHK_ADD( len, asn1_write_mpi( &c, buf, &rsa->DP ) );
        ASN1_CHK_ADD( len, asn1_write_mpi( &c, buf, &rsa->Q ) );
        ASN1_CHK_ADD( len, asn1_write_mpi( &c, buf, &rsa->P ) );
        ASN1_CHK_ADD( len, asn1_write_mpi( &c, buf, &rsa->D ) );
        ASN1_CHK_ADD( len, asn1_write_mpi( &c, buf, &rsa->E ) );
        ASN1_CHK_ADD( len, asn1_write_mpi( &c, buf, &rsa->N ) );
        ASN1_CHK_ADD( len, asn1_write_int( &c, buf, 0 ) );

        ASN1_CHK_ADD( len, asn1_write_len( &c, buf, len ) );
        ASN1_CHK_ADD( len, asn1_write_tag( &c, buf, ASN1_CONSTRUCTED | ASN1_SEQUENCE ) );
    }
    else
#endif
        return( POLARSSL_ERR_PK_FEATURE_UNAVAILABLE );

    return( (int) len );
}

#endif /* POLARSSL_PK_WRITE_C */
