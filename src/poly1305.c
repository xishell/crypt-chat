/***********************************************************************************************
 * Poly1305 – Message Authentication Code (utan standardheaders)
 *
 * Nyckel: key[32] = r(16) || s(16), little-endian
 *   - r: 128-bit multiplikationsnyckel (clampas enligt RFC 8439)
 *   - s: 128-bit adderingsnyckel (adderas i slutet)
 *
 * Meddelande: bearbetas i 16-byte block (m0..m4 i 26-bitars representation)
 * Modulus: p = 2^130 - 5
 *
 * Resultat: tag[16] (128-bit, little-endian)
 ***********************************************************************************************/

#include "poly1305.h"

/* ---------------- Hjälpfunktioner: little-endian load/store ---------------- */

/* Läs 32 bit little-endian */
static inline uint32_t load32_le(const uint8_t b[4]) {
    return ((uint32_t)b[0])
         | ((uint32_t)b[1] << 8)
         | ((uint32_t)b[2] << 16)
         | ((uint32_t)b[3] << 24);
}

/* Läs 64 bit little-endian */
static inline uint64_t load64_le(const uint8_t b[8]) {
    return (uint64_t)load32_le(b) | ((uint64_t)load32_le(b + 4) << 32);
}

/* Skriv 32 bit little-endian */
static inline void store32_le(uint8_t out[4], uint32_t w) {
    out[0] = (uint8_t)w;
    out[1] = (uint8_t)(w >> 8);
    out[2] = (uint8_t)(w >> 16);
    out[3] = (uint8_t)(w >> 24);
}

/* ---------------- Poly1305 huvudfunktion ---------------- */
void poly1305_auth(uint8_t tag[16],
                   const uint8_t *m, size_t mlen,
                   const uint8_t key[32])
{
    /* 1) Läs r och dela i 5×26-bitars delar */
    uint64_t t0 = load64_le(key + 0);
    uint64_t t1 = load64_le(key + 8);

    uint32_t r0 = (uint32_t)((t0) & 0x3ffffff);
    uint32_t r1 = (uint32_t)((t0 >> 26) & 0x3ffffff);
    uint32_t r2 = (uint32_t)(((t0 >> 52) | (t1 << 12)) & 0x3ffffff);
    uint32_t r3 = (uint32_t)((t1 >> 14) & 0x3ffffff);
    uint32_t r4 = (uint32_t)((t1 >> 40) & 0x3ffffff);

    /* 2) Clamp (maskera bort förbjudna bitar) */
    r0 &= 0x3ffffff;
    r1 &= 0x3ffff03;
    r2 &= 0x3ffc0ff;
    r3 &= 0x3f03fff;
    r4 &= 0x00fffff;

    /* Förberäkna 5*r_i för snabbare reduktion */
    uint32_t s1 = r1 * 5u;
    uint32_t s2 = r2 * 5u;
    uint32_t s3 = r3 * 5u;
    uint32_t s4 = r4 * 5u;

    /* 3) Ackumulator h = 0 */
    uint32_t h0 = 0, h1 = 0, h2 = 0, h3 = 0, h4 = 0;

    /* 4) Processa meddelandet i 16-byte block */
while (mlen > 0) {
    uint8_t block[16] = {0};
    size_t take = (mlen > 16) ? 16 : mlen;

    for (size_t i = 0; i < take; i++) block[i] = m[i];
    if (take < 16) block[take] = 1;   /* <-- VIKTIGT för partiellt block (Poly1305-padding) */

    m    += take;
    mlen -= take;

    uint64_t n0 = load64_le(block + 0);
    uint64_t n1 = load64_le(block + 8);

    uint32_t m0 = (uint32_t)( ( n0                    ) & 0x3ffffff );
    uint32_t m1 = (uint32_t)( ( n0 >> 26              ) & 0x3ffffff );
    uint32_t m2 = (uint32_t)( ((n0 >> 52) | (n1 << 12)) & 0x3ffffff );
    uint32_t m3 = (uint32_t)( ( n1 >> 14              ) & 0x3ffffff );
    uint32_t m4 = (uint32_t)( ( n1 >> 40              ) & 0x3ffffff );

    if (take == 16) m4 += (1u << 24); /* hibit = 1<<24 för fulla block; 0 annars */


        /* h = h + m */
        uint64_t d0 = (uint64_t)h0 + m0;
        uint64_t d1 = (uint64_t)h1 + m1;
        uint64_t d2 = (uint64_t)h2 + m2;
        uint64_t d3 = (uint64_t)h3 + m3;
        uint64_t d4 = (uint64_t)h4 + m4;

        /* h = (h + m) * r (med 5*r_i i korsprodukterna) */
        uint64_t c0 = d0*r0 + d1*s4 + d2*s3 + d3*s2 + d4*s1;
        uint64_t c1 = d0*r1 + d1*r0 + d2*s4 + d3*s3 + d4*s2;
        uint64_t c2 = d0*r2 + d1*r1 + d2*r0 + d3*s4 + d4*s3;
        uint64_t c3 = d0*r3 + d1*r2 + d2*r1 + d3*r0 + d4*s4;
        uint64_t c4 = d0*r4 + d1*r3 + d2*r2 + d3*r1 + d4*r0;

        /* Reducera till 26-bitars limbs */
        uint64_t t;
        t = c0 & 0x3ffffff; h0 = (uint32_t)t; c1 += (c0 >> 26);
        t = c1 & 0x3ffffff; h1 = (uint32_t)t; c2 += (c1 >> 26);
        t = c2 & 0x3ffffff; h2 = (uint32_t)t; c3 += (c2 >> 26);
        t = c3 & 0x3ffffff; h3 = (uint32_t)t; c4 += (c3 >> 26);
        t = c4 & 0x3ffffff; h4 = (uint32_t)t;

        uint64_t carry = (c4 >> 26);
        h0 += (uint32_t)(carry * 5u);
        uint32_t cc = h0 >> 26; h0 &= 0x3ffffff; h1 += cc;
    }

    /* 5) Slutreduktion */
    uint32_t cc;
    cc = h1 >> 26; h1 &= 0x3ffffff; h2 += cc;
    cc = h2 >> 26; h2 &= 0x3ffffff; h3 += cc;
    cc = h3 >> 26; h3 &= 0x3ffffff; h4 += cc;
    cc = h4 >> 26; h4 &= 0x3ffffff; h0 += cc * 5u;
    cc = h0 >> 26; h0 &= 0x3ffffff; h1 += cc;

    /* 6) Kandidat g = h + 5 (avgör om h ≥ p) */
    uint32_t g0 = h0 + 5;
    uint32_t g1 = h1 + (g0 >> 26); g0 &= 0x3ffffff;
    uint32_t g2 = h2 + (g1 >> 26); g1 &= 0x3ffffff;
    uint32_t g3 = h3 + (g2 >> 26); g2 &= 0x3ffffff;
    uint32_t g4 = h4 + (g3 >> 26) - (1u << 26); g3 &= 0x3ffffff;

    uint32_t mask  = (g4 >> 31) - 1;
    uint32_t nmask = ~mask;
    h0 = (h0 & nmask) | (g0 & mask);
    h1 = (h1 & nmask) | (g1 & mask);
    h2 = (h2 & nmask) | (g2 & mask);
    h3 = (h3 & nmask) | (g3 & mask);
    h4 = (h4 & nmask) | (g4 & mask);

    // 7) Packa till 128 bit 
    uint64_t g_lo = ((uint64_t)h0) | ((uint64_t)h1 << 26) | ((uint64_t)h2 << 52);
    uint64_t g_hi = ((uint64_t)h2 >> 12) | ((uint64_t)h3 << 14) | ((uint64_t)h4 << 40);

   // 8) Lägg till s (key[16..31]) modulo 2^128 och skriv ut tag 
{
    uint64_t s0_64 = load64_le(key + 16);
    uint64_t s1_64 = load64_le(key + 24);

    uint64_t t0_64   = g_lo + s0_64;
    uint64_t carry64 = (t0_64 < g_lo);          // carry om 64-bit overflow 
    uint64_t t1_64   = g_hi + s1_64 + carry64;

    store32_le(tag +  0, (uint32_t)t0_64);
    store32_le(tag +  4, (uint32_t)(t0_64 >> 32));
    store32_le(tag +  8, (uint32_t)t1_64);
    store32_le(tag + 12, (uint32_t)(t1_64 >> 32));
}
}

