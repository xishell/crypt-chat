#ifndef POLY1305_H
#define POLY1305_H

#include "basetypes.h"   /* egna datatyper */

/* Konstanter */
#define POLY1305_TAGLEN    16u   /* 16-byte MAC-tag */
#define POLY1305_KEYLEN    32u   /* 32-byte nyckel (r||s) */
#define POLY1305_BLOCKLEN  16u   /* 16-byte meddelandeblock */

/* Funktionsdeklaration
   Beräknar 16-byte autentiseringstag (MAC) över meddelandet m. */
void poly1305_auth(uint8_t tag[POLY1305_TAGLEN],
                   const uint8_t *m, size_t mlen,
                   const uint8_t key[POLY1305_KEYLEN]);

#endif /* POLY1305_H */