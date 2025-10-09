#ifndef AEAD_H
#define AEAD_H

/* Simple ChaCha20-Poly1305 helpers for demo use.
 * Payload format: [nonce(12)] [ciphertext(n)] [tag(16)]
 */

int aead_encrypt_pack(const unsigned char *pt, int pt_len,
                      unsigned char *out, int max_out);

int aead_decrypt_unpack(const unsigned char *in, int in_len,
                        unsigned char *pt, int max_pt);

#endif /* AEAD_H */

