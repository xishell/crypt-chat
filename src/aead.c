#include "aead.h"
#include "crypto_key.h"

/* Use the demo implementations from aead-demo */
#include "chacha20.h"
#include "poly1305.h"

static unsigned int s_nonce_ctr = 1;

static void derive_poly_key(const unsigned char *nonce,
                            unsigned char poly_key[32]) {
    chacha20_ctx kctx;
    unsigned char zero[32] = {0};
    chacha20_init(&kctx, CRYPTO_KEY, nonce, 0);
    chacha20_xor(&kctx, zero, poly_key, sizeof(zero));
}

int aead_encrypt_pack(const unsigned char *pt, int pt_len, unsigned char *out,
                      int max_out) {
    if (pt_len < 0)
        return -1;
    /* format: nonce(12) + ct(pt_len) + tag(16) */
    if (12 + pt_len + 16 > max_out)
        return -2;

    unsigned char nonce[12] = {0};
    unsigned int ctr = s_nonce_ctr++;
    nonce[0] = (unsigned char)(ctr & 0xFF);
    nonce[1] = (unsigned char)((ctr >> 8) & 0xFF);
    nonce[2] = (unsigned char)((ctr >> 16) & 0xFF);
    nonce[3] = (unsigned char)((ctr >> 24) & 0xFF);

    for (int i = 0; i < 12; i++)
        out[i] = nonce[i];

    chacha20_ctx ctx;
    chacha20_init(&ctx, CRYPTO_KEY, nonce, 1);
    chacha20_xor(&ctx, pt, out + 12, (size_t)pt_len);

    unsigned char poly_key[32];
    derive_poly_key(nonce, poly_key);
    poly1305_auth(out + 12 + pt_len, out + 12, (size_t)pt_len, poly_key);

    return 12 + pt_len + 16;
}

int aead_decrypt_unpack(const unsigned char *in, int in_len, unsigned char *pt,
                        int max_pt) {
    if (in_len < 12 + 16)
        return -1;
    int ct_len = in_len - 12 - 16;
    if (ct_len > max_pt)
        return -2;

    const unsigned char *nonce = in;
    const unsigned char *ct = in + 12;
    const unsigned char *tag = in + 12 + ct_len;

    unsigned char poly_key[32], calc_tag[16];
    derive_poly_key(nonce, poly_key);
    poly1305_auth(calc_tag, ct, (size_t)ct_len, poly_key);
    for (int i = 0; i < 16; i++) {
        if (calc_tag[i] != tag[i])
            return -3;
    }

    chacha20_ctx ctx;
    chacha20_init(&ctx, CRYPTO_KEY, nonce, 1);
    chacha20_xor(&ctx, ct, pt, (size_t)ct_len);
    return ct_len;
}
