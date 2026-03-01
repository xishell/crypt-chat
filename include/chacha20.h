#ifndef CHACHA20_H
#define CHACHA20_H

#include <stdint.h>
#include <stddef.h>

/* Tillståndsstruktur: 16 st 32-bitars ord */
typedef struct {
    uint32_t state[16];
} chacha20_ctx;

/* Initiera algoritmen (key + nonce + counter) */
void chacha20_init(chacha20_ctx *ctx,
                   const uint8_t key[32],
                   const uint8_t nonce[12],
                   uint32_t counter);

/* XOR-kryptering/dekryptering */
void chacha20_xor(chacha20_ctx *ctx,
                  const uint8_t *in,
                  uint8_t *out,
                  size_t len);

#endif /* CHACHA20_H */


/******************************************************
   DOKUMENTATION / KOMMENTARER
   ----------------------------

   Filnamn: chacha20.h
   Syfte:   Deklarerar ChaCha20:s publika API och
            tillståndsstruktur. Implementation i chacha20.c.

-------------------------------------------------------
1) Egna typdefinitioner
-------------------------------------------------------
Eftersom vi inte får använda <stdint.h> eller <stddef.h>
definierar vi själva:
   uint8_t  →  8-bitars osignerad byte
   uint32_t →  32-bitars osignerat heltal
   size_t   →  används för längder och index (osignerad)

Guarden CRYPTO_BASETYPES_DEFINED gör att typerna inte
definieras igen om en annan header också deklarerar dem.

-------------------------------------------------------
2) Strukturen chacha20_ctx
-------------------------------------------------------
Håller algoritmens inre tillstånd:
   state[16] → 16 ord (32 bitar var)
Totalt 512 bitar som består av:
   - 4 konstanter
   - 8 ord för nyckeln
   - 1 ord för räknare (counter)
   - 3 ord för nonce

-------------------------------------------------------
3) Funktioner
-------------------------------------------------------

void chacha20_init(...)
   - Fyller ctx->state med konstanter, nyckel, räknare, nonce.
   - key:    32 byte (256 bit)
   - nonce:  12 byte (96 bit)
   - counter: startvärde för blockräknaren (ofta 1)

void chacha20_xor(...)
   - Utför kryptering/avkryptering (XOR med keystream).
   - Symmetrisk:
         ciphertext = plaintext XOR keystream
         plaintext  = ciphertext XOR keystream
   - Parametrar:
         ctx  → tillstånd (initierat)
         in   → input (plaintext/ciphertext)
         out  → output (ciphertext/plaintext)
         len  → antal bytes att behandla

-------------------------------------------------------
4) Översikt
-------------------------------------------------------
• chacha20_init() sätter upp intern state.
• chacha20_xor() genererar keystream-block om 64 byte och
  XOR:ar dem över indata.
• Counter ökas per block; funktionen kan anropas i flera
  steg på samma ctx.

-------------------------------------------------------
5) Sammanfattning
-------------------------------------------------------
Headern specificerar *vad* ChaCha20 gör (gränssnittet),
medan chacha20.c implementerar *hur* det görs.
******************************************************/