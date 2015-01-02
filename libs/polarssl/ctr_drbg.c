/*
 *  CTR_DRBG implementation based on AES-256 (NIST SP 800-90)
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
 *  The NIST SP 800-90 DRBGs are described in the following publucation.
 *
 *  http://csrc.nist.gov/publications/nistpubs/800-90/SP800-90revised_March2007.pdf
 */

#include "polarssl.h"

#if defined(POLARSSL_CTR_DRBG_C)

#include "ctr_drbg.h"

#define polarssl_printf printf

/*
 * Non-public function wrapped by ctr_crbg_init(). Necessary to allow NIST
 * tests to succeed (which require known length fixed entropy)
 */
int ctr_drbg_init_entropy_len(
                   ctr_drbg_context *ctx,
                   int (*f_entropy)(void *, unsigned char *, size_t),
                   void *p_entropy,
                   const unsigned char *custom,
                   size_t len,
                   size_t entropy_len )
{
    int ret;
    unsigned char key[CTR_DRBG_KEYSIZE];

    memset( ctx, 0, sizeof(ctr_drbg_context) );
    memset( key, 0, CTR_DRBG_KEYSIZE );

    ctx->f_entropy = f_entropy;
    ctx->p_entropy = p_entropy;

    ctx->entropy_len = entropy_len;
    ctx->reseed_interval = CTR_DRBG_RESEED_INTERVAL;

    /*
     * Initialize with an empty key
     */
    aes_setkey_enc( &ctx->aes_ctx, key, CTR_DRBG_KEYBITS );

    if( ( ret = ctr_drbg_reseed( ctx, custom, len ) ) != 0 )
        return( ret );

    return( 0 );
}

int ctr_drbg_init( ctr_drbg_context *ctx,
                   int (*f_entropy)(void *, unsigned char *, size_t),
                   void *p_entropy,
                   const unsigned char *custom,
                   size_t len )
{
    return( ctr_drbg_init_entropy_len( ctx, f_entropy, p_entropy, custom, len,
                                       CTR_DRBG_ENTROPY_LEN ) );
}

void ctr_drbg_set_prediction_resistance( ctr_drbg_context *ctx, int resistance )
{
    ctx->prediction_resistance = resistance;
}

void ctr_drbg_set_entropy_len( ctr_drbg_context *ctx, size_t len )
{
    ctx->entropy_len = len;
}

void ctr_drbg_set_reseed_interval( ctr_drbg_context *ctx, int interval )
{
    ctx->reseed_interval = interval;
}

static int block_cipher_df( unsigned char *output,
                            const unsigned char *data, size_t data_len )
{
    unsigned char buf[CTR_DRBG_MAX_SEED_INPUT + CTR_DRBG_BLOCKSIZE + 16];
    unsigned char tmp[CTR_DRBG_SEEDLEN];
    unsigned char key[CTR_DRBG_KEYSIZE];
    unsigned char chain[CTR_DRBG_BLOCKSIZE];
    unsigned char *p, *iv;
    aes_context aes_ctx;

    int i, j;
    size_t buf_len, use_len;

    memset( buf, 0, CTR_DRBG_MAX_SEED_INPUT + CTR_DRBG_BLOCKSIZE + 16 );

    /*
     * Construct IV (16 bytes) and S in buffer
     * IV = Counter (in 32-bits) padded to 16 with zeroes
     * S = Length input string (in 32-bits) || Length of output (in 32-bits) ||
     *     data || 0x80
     *     (Total is padded to a multiple of 16-bytes with zeroes)
     */
    p = buf + CTR_DRBG_BLOCKSIZE;
    *p++ = ( data_len >> 24 ) & 0xff;
    *p++ = ( data_len >> 16 ) & 0xff;
    *p++ = ( data_len >> 8  ) & 0xff;
    *p++ = ( data_len       ) & 0xff;
    p += 3;
    *p++ = CTR_DRBG_SEEDLEN;
    memcpy( p, data, data_len );
    p[data_len] = 0x80;

    buf_len = CTR_DRBG_BLOCKSIZE + 8 + data_len + 1;

    for( i = 0; i < CTR_DRBG_KEYSIZE; i++ )
        key[i] = i;

    aes_setkey_enc( &aes_ctx, key, CTR_DRBG_KEYBITS );

    /*
     * Reduce data to POLARSSL_CTR_DRBG_SEEDLEN bytes of data
     */
    for( j = 0; j < CTR_DRBG_SEEDLEN; j += CTR_DRBG_BLOCKSIZE )
    {
        p = buf;
        memset( chain, 0, CTR_DRBG_BLOCKSIZE );
        use_len = buf_len;

        while( use_len > 0 )
        {
            for( i = 0; i < CTR_DRBG_BLOCKSIZE; i++ )
                chain[i] ^= p[i];
            p += CTR_DRBG_BLOCKSIZE;
            use_len -= ( use_len >= CTR_DRBG_BLOCKSIZE ) ?
                       CTR_DRBG_BLOCKSIZE : use_len;

            aes_crypt_ecb( &aes_ctx, AES_ENCRYPT, chain, chain );
        }

        memcpy( tmp + j, chain, CTR_DRBG_BLOCKSIZE );

        /*
         * Update IV
         */
        buf[3]++;
    }

    /*
     * Do final encryption with reduced data
     */
    aes_setkey_enc( &aes_ctx, tmp, CTR_DRBG_KEYBITS );
    iv = tmp + CTR_DRBG_KEYSIZE;
    p = output;

    for( j = 0; j < CTR_DRBG_SEEDLEN; j += CTR_DRBG_BLOCKSIZE )
    {
        aes_crypt_ecb( &aes_ctx, AES_ENCRYPT, iv, iv );
        memcpy( p, iv, CTR_DRBG_BLOCKSIZE );
        p += CTR_DRBG_BLOCKSIZE;
    }

    return( 0 );
}

static int ctr_drbg_update_internal( ctr_drbg_context *ctx,
                              const unsigned char data[CTR_DRBG_SEEDLEN] )
{
    unsigned char tmp[CTR_DRBG_SEEDLEN];
    unsigned char *p = tmp;
    int i, j;

    memset( tmp, 0, CTR_DRBG_SEEDLEN );

    for( j = 0; j < CTR_DRBG_SEEDLEN; j += CTR_DRBG_BLOCKSIZE )
    {
        /*
         * Increase counter
         */
        for( i = CTR_DRBG_BLOCKSIZE; i > 0; i-- )
            if( ++ctx->counter[i - 1] != 0 )
                break;

        /*
         * Crypt counter block
         */
        aes_crypt_ecb( &ctx->aes_ctx, AES_ENCRYPT, ctx->counter, p );

        p += CTR_DRBG_BLOCKSIZE;
    }

    for( i = 0; i < CTR_DRBG_SEEDLEN; i++ )
        tmp[i] ^= data[i];

    /*
     * Update key and counter
     */
    aes_setkey_enc( &ctx->aes_ctx, tmp, CTR_DRBG_KEYBITS );
    memcpy( ctx->counter, tmp + CTR_DRBG_KEYSIZE, CTR_DRBG_BLOCKSIZE );

    return( 0 );
}

void ctr_drbg_update( ctr_drbg_context *ctx,
                      const unsigned char *additional, size_t add_len )
{
    unsigned char add_input[CTR_DRBG_SEEDLEN];

    if( add_len > 0 )
    {
        block_cipher_df( add_input, additional, add_len );
        ctr_drbg_update_internal( ctx, add_input );
    }
}

int ctr_drbg_reseed( ctr_drbg_context *ctx,
                     const unsigned char *additional, size_t len )
{
    unsigned char seed[CTR_DRBG_MAX_SEED_INPUT];
    size_t seedlen = 0;

    if( ctx->entropy_len + len > CTR_DRBG_MAX_SEED_INPUT )
        return( POLARSSL_ERR_CTR_DRBG_INPUT_TOO_BIG );

    memset( seed, 0, CTR_DRBG_MAX_SEED_INPUT );

    /*
     * Gather entropy_len bytes of entropy to seed state
     */
    if( 0 != ctx->f_entropy( ctx->p_entropy, seed,
                             ctx->entropy_len ) )
    {
        return( POLARSSL_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED );
    }

    seedlen += ctx->entropy_len;

    /*
     * Add additional data
     */
    if( additional && len )
    {
        memcpy( seed + seedlen, additional, len );
        seedlen += len;
    }

    /*
     * Reduce to 384 bits
     */
    block_cipher_df( seed, seed, seedlen );

    /*
     * Update state
     */
    ctr_drbg_update_internal( ctx, seed );
    ctx->reseed_counter = 1;

    return( 0 );
}
    
int ctr_drbg_random_with_add( void *p_rng,
                              unsigned char *output, size_t output_len,
                              const unsigned char *additional, size_t add_len )
{
    int ret = 0;
    ctr_drbg_context *ctx = (ctr_drbg_context *) p_rng;
    unsigned char add_input[CTR_DRBG_SEEDLEN];
    unsigned char *p = output;
    unsigned char tmp[CTR_DRBG_BLOCKSIZE];
    int i;
    size_t use_len;

    if( output_len > CTR_DRBG_MAX_REQUEST )
        return( POLARSSL_ERR_CTR_DRBG_REQUEST_TOO_BIG );

    if( add_len > CTR_DRBG_MAX_INPUT )
        return( POLARSSL_ERR_CTR_DRBG_INPUT_TOO_BIG );

    memset( add_input, 0, CTR_DRBG_SEEDLEN );

    if( ctx->reseed_counter > ctx->reseed_interval ||
        ctx->prediction_resistance )
    {
        if( ( ret = ctr_drbg_reseed( ctx, additional, add_len ) ) != 0 )
            return( ret );

        add_len = 0;
    }

    if( add_len > 0 )
    {
        block_cipher_df( add_input, additional, add_len );
        ctr_drbg_update_internal( ctx, add_input );
    }

    while( output_len > 0 )
    {
        /*
         * Increase counter
         */
        for( i = CTR_DRBG_BLOCKSIZE; i > 0; i-- )
            if( ++ctx->counter[i - 1] != 0 )
                break;

        /*
         * Crypt counter block
         */
        aes_crypt_ecb( &ctx->aes_ctx, AES_ENCRYPT, ctx->counter, tmp );

        use_len = (output_len > CTR_DRBG_BLOCKSIZE ) ? CTR_DRBG_BLOCKSIZE : output_len;
        /*
         * Copy random block to destination
         */
        memcpy( p, tmp, use_len );
        p += use_len;
        output_len -= use_len;
    }

    ctr_drbg_update_internal( ctx, add_input );

    ctx->reseed_counter++;

    return( 0 );
}

int ctr_drbg_random( void *p_rng, unsigned char *output, size_t output_len )
{
    return ctr_drbg_random_with_add( p_rng, output, output_len, NULL, 0 );
}

#endif
