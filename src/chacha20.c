/***********************************************************************************************
 * ChaCha20 – 256-bit stream cipher (utan standardheaders)
 *
 * Implementerar RFC 8439:
 *  - Key:   32 bytes (256 bitar)
 *  - Nonce: 12 bytes (96 bitar)
 *  - Counter: 32-bit (oftast start = 1)
 *
 * 64-byte block genereras per iteration av 20 rundor (10 double rounds).
 * Samma funktion används för kryptering/dekryptering via XOR.
 ***********************************************************************************************/

#include "chacha20.h"

/* ---------------- Hjälpfunktioner för endianess och bitrotation ---------------- */

/* Läs 32-bit little-endian från byte[4] */
static inline uint32_t load32_le(const uint8_t b[4]) {
    return  ((uint32_t)b[0])        |
            ((uint32_t)b[1] << 8)   |
            ((uint32_t)b[2] << 16)  |
            ((uint32_t)b[3] << 24);
}

/* Skriv 32-bit till 4 bytes little-endian */
static inline void store32_le(uint8_t out[4], uint32_t w) {
    out[0] = (uint8_t)(w);
    out[1] = (uint8_t)(w >> 8);
    out[2] = (uint8_t)(w >> 16);
    out[3] = (uint8_t)(w >> 24);
}

/* 32-bit vänsterrotation (mod 32) */
static inline uint32_t rotl32(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

/* ---------------- Quarter Round ---------------- */
static inline void quarter_round(uint32_t *a, uint32_t *b,
                                 uint32_t *c, uint32_t *d)
{
    *a += *b;  *d ^= *a;  *d = rotl32(*d, 16);
    *c += *d;  *b ^= *c;  *b = rotl32(*b, 12);
    *a += *b;  *d ^= *a;  *d = rotl32(*d,  8);
    *c += *d;  *b ^= *c;  *b = rotl32(*b,  7);
}

/* ---------------- Blockfunktion (64 byte per anrop) ---------------- */
static void chacha20_block(const chacha20_ctx *ctx, uint8_t out[64])
{
    uint32_t s[16];
    for (int i = 0; i < 16; i++) s[i] = ctx->state[i];  /* kopiera state */

    for (int i = 0; i < 10; i++) {
        /* Kolumnrundor */
        quarter_round(&s[0],  &s[4],  &s[8],  &s[12]);
        quarter_round(&s[1],  &s[5],  &s[9],  &s[13]);
        quarter_round(&s[2],  &s[6],  &s[10], &s[14]);
        quarter_round(&s[3],  &s[7],  &s[11], &s[15]);
        /* Diagonalrundor */
        quarter_round(&s[0],  &s[5],  &s[10], &s[15]);
        quarter_round(&s[1],  &s[6],  &s[11], &s[12]);
        quarter_round(&s[2],  &s[7],  &s[8],  &s[13]);
        quarter_round(&s[3],  &s[4],  &s[9],  &s[14]);
    }

    /* Lägg till ursprungsstate och skriv ut little-endian */
    for (int i = 0; i < 16; i++) s[i] += ctx->state[i];
    for (int i = 0; i < 16; i++) store32_le(out + 4*i, s[i]);
}

/* ---------------- Initiering ---------------- */
void chacha20_init(chacha20_ctx *ctx,
                   const uint8_t key[32],
                   const uint8_t nonce[12],
                   uint32_t counter)
{
    /* "expand 32-byte k" (konstanter i LE) */
    ctx->state[0]  = 0x61707865;  /* "expa" */
    ctx->state[1]  = 0x3320646e;  /* "nd 3" */
    ctx->state[2]  = 0x79622d32;  /* "2-by" */
    ctx->state[3]  = 0x6b206574;  /* "te k" */

    /* 256-bit key som 8 × 32-bit LE-ord */
    for (int i = 0; i < 8; i++)
        ctx->state[4 + i] = load32_le(&key[4*i]);

    /* 32-bit block counter */
    ctx->state[12] = counter;

    /* 96-bit nonce som 3 × 32-bit LE-ord */
    for (int i = 0; i < 3; i++)
        ctx->state[13 + i] = load32_le(&nonce[4*i]);
}

/* ---------------- Kryptering / Dekryptering ---------------- */
void chacha20_xor(chacha20_ctx *ctx,
                  const uint8_t *in,
                  uint8_t *out,
                  size_t len)
{
    uint8_t block[64];
    size_t  off = 0;

    while (len > 0) {
        chacha20_block(ctx, block);
        size_t take = (len > 64) ? 64 : len;

        for (size_t i = 0; i < take; i++)
            out[off + i] = in[off + i] ^ block[i];

        ctx->state[12]++;
        off += take;
        len -= take;
    }

    /* Nollställ temporär buffer (skydd mot dataläckage) */
    volatile uint8_t *p = block;
    for (int i = 0; i < 64; i++) p[i] = 0;
}