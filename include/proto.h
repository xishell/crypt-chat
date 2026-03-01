/* proto.h – Paketformat och API (utan standardheaders) */
#ifndef PROTO_H
#define PROTO_H

#include <stdint.h>
#include <stddef.h>

// --- Konstanter --- 
#define MAGIC       0xC1      // paketstart
#define VERSION     0x01      // protokollversion
#define NONCE_LEN   12        // nonce = 12 byte 
#define TAG_LEN     16        // Poly1305 tag = 16 byte 
#define HDR_LEN     18        // headerstorlek: 1+1+1+1 + 12 + 2 = 18 

// --- Flaggor --- 
enum { FLAG_TEXT = 0x01 };

/**********************************************************************
 --- Headerlayout (exakt 18 byte) ---
   magic(1) | version(1) | flags(1) | rsv(1) | nonce(12) | clen_le(2)
************************************************************************/
typedef struct {                       // Notera: 'clen_le' är ciphertext-längden i *little-endian på tråden
    uint8_t  magic;                    // 1 
    uint8_t  version;                  // 1 
    uint8_t  flags;                    // 1 
    uint8_t  rsv;                      // 1, alltid 0 
    uint8_t  nonce[NONCE_LEN];         // 12 
    uint16_t clen_le;                  // 2, ciphertext length (LE på tråden) 
} PacketHeader;

/* Compile-time kontroll av headerstorleken (C11+). */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(PacketHeader) == HDR_LEN, "Header size mismatch");
#endif

/* --- Publikt API --- */

/* Nollställ/återställ intern parser-state (t.ex. efter fel). */
void   proto_restart(void);

/* Mata in mottagna bytes (kan komma i små bitar). */
void   proto_feed(const uint8_t *src, size_t n);

/* Finns ett komplett, verifierat plaintextmeddelande att hämta? (0/1) */
uint8_t proto_has_msg(void);

/* Läs ut plaintext. Returnerar antal kopierade bytes (0 om inget). */
size_t  proto_read_msg(uint8_t *dst, size_t max);

/* Bygg och sänd ett paket av given plaintext (LEN||HEADER||CIPHERTEXT||TAG). */
void    proto_build_and_send(const uint8_t *pt, size_t n);

#endif /* PROTO_H */

/* 
============================ Dokumentation ============================

Trådram:
  LEN(2, LE) || HEADER(18) || CIPHERTEXT(clen) || TAG(16)

HEADER:
  magic=0xC1, version=0x01, flags (t.ex. FLAG_TEXT), rsv=0,
  nonce[12], clen_le (LSB först / little-endian på tråden).

Regler:
  LEN = HDR_LEN + clen + TAG_LEN
  rsv != 0 → avvisa paket
  okänd version/flag → avvisa paket

Krypto (översikt; måste matcha i proto.c):
  - Kryptering: ChaCha20 (key 32B, nonce 12B, counter=1…)
  - Tag: Poly1305 över definierat AAD + ciphertext (AEAD); var konsekvent
  - Nonce: unikt per sändning (t.ex. 4B slump + 8B räknare)
  - Inget loggande av nycklar/taggar/nonce i klartext

Endianness:
  - Fältet 'clen_le' är little-endian i headern på tråden
  - All övrig intern aritmetik kan vara CPU-native; var explicit vid (de)serialisering

======================================================================= 
*/