/*
 *  The RSA public-key cryptosystem
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
/*
 *  RSA was designed by Ron Rivest, Adi Shamir and Len Adleman.
 *
 *  http://theory.lcs.mit.edu/~rivest/rsapaper.pdf
 *  http://www.cacr.math.uwaterloo.ca/hac/about/chap8.pdf
 */

#include "polarssl.h"

#if defined(POLARSSL_RSA_C)

#include "rsa.h"
#include "oid.h"

#if defined(POLARSSL_PKCS1_V21)
#include "md.h"
#endif

#include <stdlib.h>
#include <stdio.h>

#define polarssl_printf printf

/*
 * Initialize an RSA context
 */
void rsa_init( rsa_context *ctx,
               int padding,
               int hash_id )
{
    memset( ctx, 0, sizeof( rsa_context ) );

    rsa_set_padding( ctx, padding, hash_id );

#if defined(POLARSSL_THREADING_C)
    polarssl_mutex_init( &ctx->mutex );
#endif
}

/*
 * Set padding for an existing RSA context
 */
void rsa_set_padding( rsa_context *ctx, int padding, int hash_id )
{
    ctx->padding = padding;
    ctx->hash_id = hash_id;
}

/*
 * Check a public RSA key
 */
int rsa_check_pubkey( const rsa_context *ctx )
{
    if( !ctx->N.p || !ctx->E.p )
        return( POLARSSL_ERR_RSA_KEY_CHECK_FAILED );

    if( ( ctx->N.p[0] & 1 ) == 0 ||
        ( ctx->E.p[0] & 1 ) == 0 )
        return( POLARSSL_ERR_RSA_KEY_CHECK_FAILED );

    if( mpi_msb( &ctx->N ) < 128 ||
        mpi_msb( &ctx->N ) > POLARSSL_MPI_MAX_BITS )
        return( POLARSSL_ERR_RSA_KEY_CHECK_FAILED );

    if( mpi_msb( &ctx->E ) < 2 ||
        mpi_msb( &ctx->E ) > 64 )
        return( POLARSSL_ERR_RSA_KEY_CHECK_FAILED );

    return( 0 );
}

/*
 * Check a private RSA key
 */
int rsa_check_privkey( const rsa_context *ctx )
{
    int ret;
    mpi PQ, DE, P1, Q1, H, I, G, G2, L1, L2, DP, DQ, QP;

    if( ( ret = rsa_check_pubkey( ctx ) ) != 0 )
        return( ret );

    if( !ctx->P.p || !ctx->Q.p || !ctx->D.p )
        return( POLARSSL_ERR_RSA_KEY_CHECK_FAILED );

    mpi_init( &PQ ); mpi_init( &DE ); mpi_init( &P1 ); mpi_init( &Q1 );
    mpi_init( &H  ); mpi_init( &I  ); mpi_init( &G  ); mpi_init( &G2 );
    mpi_init( &L1 ); mpi_init( &L2 ); mpi_init( &DP ); mpi_init( &DQ );
    mpi_init( &QP );

    MPI_CHK( mpi_mul_mpi( &PQ, &ctx->P, &ctx->Q ) );
    MPI_CHK( mpi_mul_mpi( &DE, &ctx->D, &ctx->E ) );
    MPI_CHK( mpi_sub_int( &P1, &ctx->P, 1 ) );
    MPI_CHK( mpi_sub_int( &Q1, &ctx->Q, 1 ) );
    MPI_CHK( mpi_mul_mpi( &H, &P1, &Q1 ) );
    MPI_CHK( mpi_gcd( &G, &ctx->E, &H  ) );

    MPI_CHK( mpi_gcd( &G2, &P1, &Q1 ) );
    MPI_CHK( mpi_div_mpi( &L1, &L2, &H, &G2 ) );
    MPI_CHK( mpi_mod_mpi( &I, &DE, &L1  ) );

    MPI_CHK( mpi_mod_mpi( &DP, &ctx->D, &P1 ) );
    MPI_CHK( mpi_mod_mpi( &DQ, &ctx->D, &Q1 ) );
    MPI_CHK( mpi_inv_mod( &QP, &ctx->Q, &ctx->P ) );
    /*
     * Check for a valid PKCS1v2 private key
     */
    if( mpi_cmp_mpi( &PQ, &ctx->N ) != 0 ||
        mpi_cmp_mpi( &DP, &ctx->DP ) != 0 ||
        mpi_cmp_mpi( &DQ, &ctx->DQ ) != 0 ||
        mpi_cmp_mpi( &QP, &ctx->QP ) != 0 ||
        mpi_cmp_int( &L2, 0 ) != 0 ||
        mpi_cmp_int( &I, 1 ) != 0 ||
        mpi_cmp_int( &G, 1 ) != 0 )
    {
        ret = POLARSSL_ERR_RSA_KEY_CHECK_FAILED;
    }

cleanup:
    mpi_free( &PQ ); mpi_free( &DE ); mpi_free( &P1 ); mpi_free( &Q1 );
    mpi_free( &H  ); mpi_free( &I  ); mpi_free( &G  ); mpi_free( &G2 );
    mpi_free( &L1 ); mpi_free( &L2 ); mpi_free( &DP ); mpi_free( &DQ );
    mpi_free( &QP );

    if( ret == POLARSSL_ERR_RSA_KEY_CHECK_FAILED )
        return( ret );

    if( ret != 0 )
        return( POLARSSL_ERR_RSA_KEY_CHECK_FAILED + ret );

    return( 0 );
}

/*
 * Do an RSA public key operation
 */
int rsa_public( rsa_context *ctx,
                const unsigned char *input,
                unsigned char *output )
{
    int ret;
    size_t olen;
    mpi T;

    mpi_init( &T );

    MPI_CHK( mpi_read_binary( &T, input, ctx->len ) );

    if( mpi_cmp_mpi( &T, &ctx->N ) >= 0 )
    {
        mpi_free( &T );
        return( POLARSSL_ERR_RSA_BAD_INPUT_DATA );
    }

    olen = ctx->len;
    MPI_CHK( mpi_exp_mod( &T, &T, &ctx->E, &ctx->N, &ctx->RN ) );
    MPI_CHK( mpi_write_binary( &T, output, olen ) );

cleanup:

    mpi_free( &T );

    if( ret != 0 )
        return( POLARSSL_ERR_RSA_PUBLIC_FAILED + ret );

    return( 0 );
}

#if !defined(POLARSSL_RSA_NO_CRT)
/*
 * Generate or update blinding values, see section 10 of:
 *  KOCHER, Paul C. Timing attacks on implementations of Diffie-Hellman, RSA,
 *  DSS, and other systems. In : Advances in Cryptology—CRYPTO’96. Springer
 *  Berlin Heidelberg, 1996. p. 104-113.
 */
static int rsa_prepare_blinding( rsa_context *ctx, mpi *Vi, mpi *Vf,
                 int (*f_rng)(void *, unsigned char *, size_t), void *p_rng )
{
    int ret, count = 0;

#if defined(POLARSSL_THREADING_C)
    polarssl_mutex_lock( &ctx->mutex );
#endif

    if( ctx->Vf.p != NULL )
    {
        /* We already have blinding values, just update them by squaring */
        MPI_CHK( mpi_mul_mpi( &ctx->Vi, &ctx->Vi, &ctx->Vi ) );
        MPI_CHK( mpi_mod_mpi( &ctx->Vi, &ctx->Vi, &ctx->N ) );
        MPI_CHK( mpi_mul_mpi( &ctx->Vf, &ctx->Vf, &ctx->Vf ) );
        MPI_CHK( mpi_mod_mpi( &ctx->Vf, &ctx->Vf, &ctx->N ) );

        goto done;
    }

    /* Unblinding value: Vf = random number, invertible mod N */
    do {
        if( count++ > 10 )
            return( POLARSSL_ERR_RSA_RNG_FAILED );

        MPI_CHK( mpi_fill_random( &ctx->Vf, ctx->len - 1, f_rng, p_rng ) );
        MPI_CHK( mpi_gcd( &ctx->Vi, &ctx->Vf, &ctx->N ) );
    } while( mpi_cmp_int( &ctx->Vi, 1 ) != 0 );

    /* Blinding value: Vi =  Vf^(-e) mod N */
    MPI_CHK( mpi_inv_mod( &ctx->Vi, &ctx->Vf, &ctx->N ) );
    MPI_CHK( mpi_exp_mod( &ctx->Vi, &ctx->Vi, &ctx->E, &ctx->N, &ctx->RN ) );

done:
    if( Vi != &ctx->Vi )
    {
        MPI_CHK( mpi_copy( Vi, &ctx->Vi ) );
        MPI_CHK( mpi_copy( Vf, &ctx->Vf ) );
    }

cleanup:
#if defined(POLARSSL_THREADING_C)
    polarssl_mutex_unlock( &ctx->mutex );
#endif

    return( ret );
}
#endif

/*
 * Do an RSA private key operation
 */
int rsa_private( rsa_context *ctx,
                 int (*f_rng)(void *, unsigned char *, size_t),
                 void *p_rng,
                 const unsigned char *input,
                 unsigned char *output )
{
    int ret;
    size_t olen;
    mpi T, T1, T2;
#if !defined(POLARSSL_RSA_NO_CRT)
    mpi *Vi, *Vf;

    /*
     * When using the Chinese Remainder Theorem, we use blinding values.
     * Without threading, we just read them directly from the context,
     * otherwise we make a local copy in order to reduce locking contention.
     */
#if defined(POLARSSL_THREADING_C)
    mpi Vi_copy, Vf_copy;

    mpi_init( &Vi_copy ); mpi_init( &Vf_copy );
    Vi = &Vi_copy;
    Vf = &Vf_copy;
#else
    Vi = &ctx->Vi;
    Vf = &ctx->Vf;
#endif
#endif

    mpi_init( &T ); mpi_init( &T1 ); mpi_init( &T2 );

    MPI_CHK( mpi_read_binary( &T, input, ctx->len ) );
    if( mpi_cmp_mpi( &T, &ctx->N ) >= 0 )
    {
        mpi_free( &T );
        return( POLARSSL_ERR_RSA_BAD_INPUT_DATA );
    }

#if defined(POLARSSL_RSA_NO_CRT)
    ((void) f_rng);
    ((void) p_rng);
    MPI_CHK( mpi_exp_mod( &T, &T, &ctx->D, &ctx->N, &ctx->RN ) );
#else
    if( f_rng != NULL )
    {
        /*
         * Blinding
         * T = T * Vi mod N
         */
        MPI_CHK( rsa_prepare_blinding( ctx, Vi, Vf, f_rng, p_rng ) );
        MPI_CHK( mpi_mul_mpi( &T, &T, Vi ) );
        MPI_CHK( mpi_mod_mpi( &T, &T, &ctx->N ) );
    }

    /*
     * faster decryption using the CRT
     *
     * T1 = input ^ dP mod P
     * T2 = input ^ dQ mod Q
     */
    MPI_CHK( mpi_exp_mod( &T1, &T, &ctx->DP, &ctx->P, &ctx->RP ) );
    MPI_CHK( mpi_exp_mod( &T2, &T, &ctx->DQ, &ctx->Q, &ctx->RQ ) );

    /*
     * T = (T1 - T2) * (Q^-1 mod P) mod P
     */
    MPI_CHK( mpi_sub_mpi( &T, &T1, &T2 ) );
    MPI_CHK( mpi_mul_mpi( &T1, &T, &ctx->QP ) );
    MPI_CHK( mpi_mod_mpi( &T, &T1, &ctx->P ) );

    /*
     * T = T2 + T * Q
     */
    MPI_CHK( mpi_mul_mpi( &T1, &T, &ctx->Q ) );
    MPI_CHK( mpi_add_mpi( &T, &T2, &T1 ) );

    if( f_rng != NULL )
    {
        /*
         * Unblind
         * T = T * Vf mod N
         */
        MPI_CHK( mpi_mul_mpi( &T, &T, Vf ) );
        MPI_CHK( mpi_mod_mpi( &T, &T, &ctx->N ) );
    }
#endif

    olen = ctx->len;
    MPI_CHK( mpi_write_binary( &T, output, olen ) );

cleanup:
    mpi_free( &T ); mpi_free( &T1 ); mpi_free( &T2 );
#if !defined(POLARSSL_RSA_NO_CRT) && defined(POLARSSL_THREADING_C)
    mpi_free( &Vi_copy ); mpi_free( &Vf_copy );
#endif

    if( ret != 0 )
        return( POLARSSL_ERR_RSA_PRIVATE_FAILED + ret );

    return( 0 );
}

#if defined(POLARSSL_PKCS1_V15)
/*
 * Implementation of the PKCS#1 v2.1 RSAES-PKCS1-V1_5-ENCRYPT function
 */
int rsa_rsaes_pkcs1_v15_encrypt( rsa_context *ctx,
                                 int (*f_rng)(void *, unsigned char *, size_t),
                                 void *p_rng,
                                 int mode, size_t ilen,
                                 const unsigned char *input,
                                 unsigned char *output )
{
    size_t nb_pad, olen;
    int ret;
    unsigned char *p = output;

    if( ctx->padding != RSA_PKCS_V15 || f_rng == NULL )
        return( POLARSSL_ERR_RSA_BAD_INPUT_DATA );

    olen = ctx->len;

    if( olen < ilen + 11 )
        return( POLARSSL_ERR_RSA_BAD_INPUT_DATA );

    nb_pad = olen - 3 - ilen;

    *p++ = 0;
    if( mode == RSA_PUBLIC )
    {
        *p++ = RSA_CRYPT;

        while( nb_pad-- > 0 )
        {
            int rng_dl = 100;

            do {
                ret = f_rng( p_rng, p, 1 );
            } while( *p == 0 && --rng_dl && ret == 0 );

            // Check if RNG failed to generate data
            //
            if( rng_dl == 0 || ret != 0)
                return POLARSSL_ERR_RSA_RNG_FAILED + ret;

            p++;
        }
    }
    else
    {
        *p++ = RSA_SIGN;

        while( nb_pad-- > 0 )
            *p++ = 0xFF;
    }

    *p++ = 0;
    memcpy( p, input, ilen );

    return( ( mode == RSA_PUBLIC )
            ? rsa_public(  ctx, output, output )
            : rsa_private( ctx, f_rng, p_rng, output, output ) );
}
#endif /* POLARSSL_PKCS1_V15 */

/*
 * Add the message padding, then do an RSA operation
 */
int rsa_pkcs1_encrypt( rsa_context *ctx,
                       int (*f_rng)(void *, unsigned char *, size_t),
                       void *p_rng,
                       int mode, size_t ilen,
                       const unsigned char *input,
                       unsigned char *output )
{
    switch( ctx->padding )
    {
#if defined(POLARSSL_PKCS1_V15)
        case RSA_PKCS_V15:
            return rsa_rsaes_pkcs1_v15_encrypt( ctx, f_rng, p_rng, mode, ilen,
                                                input, output );
#endif
        default:
            return( POLARSSL_ERR_RSA_INVALID_PADDING );
    }
}

#if defined(POLARSSL_PKCS1_V15)
/*
 * Implementation of the PKCS#1 v2.1 RSAES-PKCS1-V1_5-DECRYPT function
 */
int rsa_rsaes_pkcs1_v15_decrypt( rsa_context *ctx,
                                 int (*f_rng)(void *, unsigned char *, size_t),
                                 void *p_rng,
                                 int mode, size_t *olen,
                                 const unsigned char *input,
                                 unsigned char *output,
                                 size_t output_max_len)
{
    int ret;
    size_t ilen, pad_count = 0, i;
    unsigned char *p, bad, pad_done = 0;
    unsigned char buf[POLARSSL_MPI_MAX_SIZE];

    if( ctx->padding != RSA_PKCS_V15 )
        return( POLARSSL_ERR_RSA_BAD_INPUT_DATA );

    ilen = ctx->len;

    if( ilen < 16 || ilen > sizeof( buf ) )
        return( POLARSSL_ERR_RSA_BAD_INPUT_DATA );

    ret = ( mode == RSA_PUBLIC )
          ? rsa_public(  ctx, input, buf )
          : rsa_private( ctx, f_rng, p_rng, input, buf );

    if( ret != 0 )
        return( ret );

    p = buf;
    bad = 0;

    /*
     * Check and get padding len in "constant-time"
     */
    bad |= *p++; /* First byte must be 0 */

    /* This test does not depend on secret data */
    if( mode == RSA_PRIVATE )
    {
        bad |= *p++ ^ RSA_CRYPT;

        /* Get padding len, but always read till end of buffer
         * (minus one, for the 00 byte) */
        for( i = 0; i < ilen - 3; i++ )
        {
            pad_done |= ( p[i] == 0 );
            pad_count += ( pad_done == 0 );
        }

        p += pad_count;
        bad |= *p++; /* Must be zero */
    }
    else
    {
        bad |= *p++ ^ RSA_SIGN;

        /* Get padding len, but always read till end of buffer
         * (minus one, for the 00 byte) */
        for( i = 0; i < ilen - 3; i++ )
        {
            pad_done |= ( p[i] != 0xFF );
            pad_count += ( pad_done == 0 );
        }

        p += pad_count;
        bad |= *p++; /* Must be zero */
    }

    if( bad )
        return( POLARSSL_ERR_RSA_INVALID_PADDING );

    if (ilen - (p - buf) > output_max_len)
        return( POLARSSL_ERR_RSA_OUTPUT_TOO_LARGE );

    *olen = ilen - (p - buf);
    memcpy( output, p, *olen );

    return( 0 );
}
#endif /* POLARSSL_PKCS1_V15 */

/*
 * Do an RSA operation, then remove the message padding
 */
int rsa_pkcs1_decrypt( rsa_context *ctx,
                       int (*f_rng)(void *, unsigned char *, size_t),
                       void *p_rng,
                       int mode, size_t *olen,
                       const unsigned char *input,
                       unsigned char *output,
                       size_t output_max_len)
{
    switch( ctx->padding )
    {
#if defined(POLARSSL_PKCS1_V15)
        case RSA_PKCS_V15:
            return rsa_rsaes_pkcs1_v15_decrypt( ctx, f_rng, p_rng, mode, olen,
                                                input, output, output_max_len );
#endif

        default:
            return( POLARSSL_ERR_RSA_INVALID_PADDING );
    }
}

#if defined(POLARSSL_PKCS1_V15)
/*
 * Implementation of the PKCS#1 v2.1 RSASSA-PKCS1-V1_5-SIGN function
 */
/*
 * Do an RSA operation to sign the message digest
 */
int rsa_rsassa_pkcs1_v15_sign( rsa_context *ctx,
                               int (*f_rng)(void *, unsigned char *, size_t),
                               void *p_rng,
                               int mode,
                               md_type_t md_alg,
                               unsigned int hashlen,
                               const unsigned char *hash,
                               unsigned char *sig )
{
    size_t nb_pad, olen, oid_size = 0;
    unsigned char *p = sig;
    const char *oid;

    if( ctx->padding != RSA_PKCS_V15 )
        return( POLARSSL_ERR_RSA_BAD_INPUT_DATA );

    olen = ctx->len;
    nb_pad = olen - 3;

    if( md_alg != POLARSSL_MD_NONE )
    {
        const md_info_t *md_info = md_info_from_type( md_alg );
        if( md_info == NULL )
            return( POLARSSL_ERR_RSA_BAD_INPUT_DATA );

        if( oid_get_oid_by_md( md_alg, &oid, &oid_size ) != 0 )
            return( POLARSSL_ERR_RSA_BAD_INPUT_DATA );

        nb_pad -= 10 + oid_size;

        hashlen = md_get_size( md_info );
    }

    nb_pad -= hashlen;

    if( ( nb_pad < 8 ) || ( nb_pad > olen ) )
        return( POLARSSL_ERR_RSA_BAD_INPUT_DATA );

    *p++ = 0;
    *p++ = RSA_SIGN;
    memset( p, 0xFF, nb_pad );
    p += nb_pad;
    *p++ = 0;

    if( md_alg == POLARSSL_MD_NONE )
    {
        memcpy( p, hash, hashlen );
    }
    else
    {
        /*
         * DigestInfo ::= SEQUENCE {
         *   digestAlgorithm DigestAlgorithmIdentifier,
         *   digest Digest }
         *
         * DigestAlgorithmIdentifier ::= AlgorithmIdentifier
         *
         * Digest ::= OCTET STRING
         */
        *p++ = ASN1_SEQUENCE | ASN1_CONSTRUCTED;
        *p++ = (unsigned char) ( 0x08 + oid_size + hashlen );
        *p++ = ASN1_SEQUENCE | ASN1_CONSTRUCTED;
        *p++ = (unsigned char) ( 0x04 + oid_size );
        *p++ = ASN1_OID;
        *p++ = oid_size & 0xFF;
        memcpy( p, oid, oid_size );
        p += oid_size;
        *p++ = ASN1_NULL;
        *p++ = 0x00;
        *p++ = ASN1_OCTET_STRING;
        *p++ = hashlen;
        memcpy( p, hash, hashlen );
    }

    return( ( mode == RSA_PUBLIC )
            ? rsa_public(  ctx, sig, sig )
            : rsa_private( ctx, f_rng, p_rng, sig, sig ) );
}
#endif /* POLARSSL_PKCS1_V15 */

/*
 * Do an RSA operation to sign the message digest
 */
int rsa_pkcs1_sign( rsa_context *ctx,
                    int (*f_rng)(void *, unsigned char *, size_t),
                    void *p_rng,
                    int mode,
                    md_type_t md_alg,
                    unsigned int hashlen,
                    const unsigned char *hash,
                    unsigned char *sig )
{
    switch( ctx->padding )
    {
#if defined(POLARSSL_PKCS1_V15)
        case RSA_PKCS_V15:
            return rsa_rsassa_pkcs1_v15_sign( ctx, f_rng, p_rng, mode, md_alg,
                                              hashlen, hash, sig );
#endif
        default:
            return( POLARSSL_ERR_RSA_INVALID_PADDING );
    }
}

#if defined(POLARSSL_PKCS1_V15)
/*
 * Implementation of the PKCS#1 v2.1 RSASSA-PKCS1-v1_5-VERIFY function
 */
int rsa_rsassa_pkcs1_v15_verify( rsa_context *ctx,
                                 int (*f_rng)(void *, unsigned char *, size_t),
                                 void *p_rng,
                                 int mode,
                                 md_type_t md_alg,
                                 unsigned int hashlen,
                                 const unsigned char *hash,
                                 const unsigned char *sig )
{
    int ret;
    size_t len, siglen, asn1_len;
    unsigned char *p, *end;
    unsigned char buf[POLARSSL_MPI_MAX_SIZE];
    md_type_t msg_md_alg;
    const md_info_t *md_info;
    asn1_buf oid;

    if( ctx->padding != RSA_PKCS_V15 )
        return( POLARSSL_ERR_RSA_BAD_INPUT_DATA );

    siglen = ctx->len;

    if( siglen < 16 || siglen > sizeof( buf ) )
        return( POLARSSL_ERR_RSA_BAD_INPUT_DATA );

    ret = ( mode == RSA_PUBLIC )
          ? rsa_public(  ctx, sig, buf )
          : rsa_private( ctx, f_rng, p_rng, sig, buf );

    if( ret != 0 )
        return( ret );

    p = buf;

    if( *p++ != 0 || *p++ != RSA_SIGN )
        return( POLARSSL_ERR_RSA_INVALID_PADDING );

    while( *p != 0 )
    {
        if( p >= buf + siglen - 1 || *p != 0xFF )
            return( POLARSSL_ERR_RSA_INVALID_PADDING );
        p++;
    }
    p++;

    len = siglen - ( p - buf );

    if( len == hashlen && md_alg == POLARSSL_MD_NONE )
    {
        if( memcmp( p, hash, hashlen ) == 0 )
            return( 0 );
        else
            return( POLARSSL_ERR_RSA_VERIFY_FAILED );
    }

    md_info = md_info_from_type( md_alg );
    if( md_info == NULL )
        return( POLARSSL_ERR_RSA_BAD_INPUT_DATA );
    hashlen = md_get_size( md_info );

    end = p + len;

    // Parse the ASN.1 structure inside the PKCS#1 v1.5 structure
    //
    if( ( ret = asn1_get_tag( &p, end, &asn1_len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
        return( POLARSSL_ERR_RSA_VERIFY_FAILED );

    if( asn1_len + 2 != len )
        return( POLARSSL_ERR_RSA_VERIFY_FAILED );

    if( ( ret = asn1_get_tag( &p, end, &asn1_len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
        return( POLARSSL_ERR_RSA_VERIFY_FAILED );

    if( asn1_len + 6 + hashlen != len )
        return( POLARSSL_ERR_RSA_VERIFY_FAILED );

    if( ( ret = asn1_get_tag( &p, end, &oid.len, ASN1_OID ) ) != 0 )
        return( POLARSSL_ERR_RSA_VERIFY_FAILED );

    oid.p = p;
    p += oid.len;

    if( oid_get_md_alg( &oid, &msg_md_alg ) != 0 )
        return( POLARSSL_ERR_RSA_VERIFY_FAILED );

    if( md_alg != msg_md_alg )
        return( POLARSSL_ERR_RSA_VERIFY_FAILED );

    /*
     * assume the algorithm parameters must be NULL
     */
    if( ( ret = asn1_get_tag( &p, end, &asn1_len, ASN1_NULL ) ) != 0 )
        return( POLARSSL_ERR_RSA_VERIFY_FAILED );

    if( ( ret = asn1_get_tag( &p, end, &asn1_len, ASN1_OCTET_STRING ) ) != 0 )
        return( POLARSSL_ERR_RSA_VERIFY_FAILED );

    if( asn1_len != hashlen )
        return( POLARSSL_ERR_RSA_VERIFY_FAILED );

    if( memcmp( p, hash, hashlen ) != 0 )
        return( POLARSSL_ERR_RSA_VERIFY_FAILED );

    p += hashlen;

    if( p != end )
        return( POLARSSL_ERR_RSA_VERIFY_FAILED );

    return( 0 );
}
#endif /* POLARSSL_PKCS1_V15 */

/*
 * Do an RSA operation and check the message digest
 */
int rsa_pkcs1_verify( rsa_context *ctx,
                      int (*f_rng)(void *, unsigned char *, size_t),
                      void *p_rng,
                      int mode,
                      md_type_t md_alg,
                      unsigned int hashlen,
                      const unsigned char *hash,
                      const unsigned char *sig )
{
    switch( ctx->padding )
    {
#if defined(POLARSSL_PKCS1_V15)
        case RSA_PKCS_V15:
            return rsa_rsassa_pkcs1_v15_verify( ctx, f_rng, p_rng, mode, md_alg,
                                                hashlen, hash, sig );
#endif

        default:
            return( POLARSSL_ERR_RSA_INVALID_PADDING );
    }
}

/*
 * Copy the components of an RSA key
 */
int rsa_copy( rsa_context *dst, const rsa_context *src )
{
    int ret;

    dst->ver = src->ver;
    dst->len = src->len;

    MPI_CHK( mpi_copy( &dst->N, &src->N ) );
    MPI_CHK( mpi_copy( &dst->E, &src->E ) );

    MPI_CHK( mpi_copy( &dst->D, &src->D ) );
    MPI_CHK( mpi_copy( &dst->P, &src->P ) );
    MPI_CHK( mpi_copy( &dst->Q, &src->Q ) );
    MPI_CHK( mpi_copy( &dst->DP, &src->DP ) );
    MPI_CHK( mpi_copy( &dst->DQ, &src->DQ ) );
    MPI_CHK( mpi_copy( &dst->QP, &src->QP ) );

    MPI_CHK( mpi_copy( &dst->RN, &src->RN ) );
    MPI_CHK( mpi_copy( &dst->RP, &src->RP ) );
    MPI_CHK( mpi_copy( &dst->RQ, &src->RQ ) );

#if !defined(POLARSSL_RSA_NO_CRT)
    MPI_CHK( mpi_copy( &dst->Vi, &src->Vi ) );
    MPI_CHK( mpi_copy( &dst->Vf, &src->Vf ) );
#endif

    dst->padding = src->padding;
    dst->hash_id = src->hash_id;

cleanup:
    if( ret != 0 )
        rsa_free( dst );

    return( ret );
}

/*
 * Free the components of an RSA key
 */
void rsa_free( rsa_context *ctx )
{
#if !defined(POLARSSL_RSA_NO_CRT)
    mpi_free( &ctx->Vi ); mpi_free( &ctx->Vf );
#endif
    mpi_free( &ctx->RQ ); mpi_free( &ctx->RP ); mpi_free( &ctx->RN );
    mpi_free( &ctx->QP ); mpi_free( &ctx->DQ ); mpi_free( &ctx->DP );
    mpi_free( &ctx->Q  ); mpi_free( &ctx->P  ); mpi_free( &ctx->D );
    mpi_free( &ctx->E  ); mpi_free( &ctx->N  );

#if defined(POLARSSL_THREADING_C)
    polarssl_mutex_free( &ctx->mutex );
#endif
}

#endif
