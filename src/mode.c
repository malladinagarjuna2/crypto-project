#include <string.h>
#include "mode.h"
#include "feistel.h"
#include "hmac.h"

int rc_ctr_xor(const rc_key *k, const uint8_t nonce[RC_NONCE], uint8_t *buf, size_t len)
{
    uint8_t ctr_block[RC_BLOCK], ks[RC_BLOCK];
    uint32_t counter = 0;
    size_t off = 0;

    /* 32-bit counter: at most 2^32 blocks (64 GiB) per nonce */
    if (((uint64_t)len - 1) / RC_BLOCK > 0xFFFFFFFFu && len > 0)
        return RC_ERR_TOO_LONG;

    memcpy(ctr_block, nonce, RC_NONCE);
    while (off < len) {
        size_t take = len - off, i;
        if (take > RC_BLOCK) take = RC_BLOCK;
        ctr_block[12] = (uint8_t)(counter >> 24);
        ctr_block[13] = (uint8_t)(counter >> 16);
        ctr_block[14] = (uint8_t)(counter >> 8);
        ctr_block[15] = (uint8_t)counter;
        feistel_encrypt_block(k, ctr_block, ks);
        for (i = 0; i < take; i++)
            buf[off + i] ^= ks[i];
        off += take;
        counter++;
    }
    memset(ks, 0, sizeof ks);
    return RC_OK;
}

static void compute_tag(const rc_key *k, const uint8_t *header, const uint8_t *ct, size_t len,
                        uint8_t tag[RC_TAG])
{
    hmac_sha256_ctx h;
    hmac_sha256_init(&h, k->mac_key, sizeof k->mac_key);
    hmac_sha256_update(&h, header, RC_MAGIC_LEN + RC_NONCE);
    hmac_sha256_update(&h, ct, len);
    hmac_sha256_final(&h, tag);
}

int rc_seal(const rc_key *k, const uint8_t nonce[RC_NONCE],
            const uint8_t *pt, size_t len, uint8_t *out)
{
    uint8_t *ct = out + RC_MAGIC_LEN + RC_NONCE;
    int rc;

    memcpy(out, RC_MAGIC, RC_MAGIC_LEN);
    memcpy(out + RC_MAGIC_LEN, nonce, RC_NONCE);
    if (len > 0)
        memmove(ct, pt, len);
    rc = rc_ctr_xor(k, nonce, ct, len);
    if (rc != RC_OK)
        return rc;
    compute_tag(k, out, ct, len, ct + len);              /* encrypt-then-MAC */
    return RC_OK;
}

int rc_open(const rc_key *k, const uint8_t *in, size_t in_len, uint8_t *pt, size_t *pt_len)
{
    uint8_t tag[RC_TAG];
    const uint8_t *ct;
    size_t len;

    if (in_len < RC_OVERHEAD || memcmp(in, RC_MAGIC, RC_MAGIC_LEN) != 0)
        return RC_ERR_FORMAT;
    len = in_len - RC_OVERHEAD;
    ct = in + RC_MAGIC_LEN + RC_NONCE;

    compute_tag(k, in, ct, len, tag);
    if (!ct_equal(tag, ct + len, RC_TAG))
        return RC_ERR_AUTH;                               /* nothing is decrypted */

    if (len > 0)
        memmove(pt, ct, len);
    *pt_len = len;
    return rc_ctr_xor(k, in + RC_MAGIC_LEN, pt, len);
}

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * rc_ctr_xor():
 *   Builds a 16-byte "counter block" = 12-byte nonce + 4-byte counter.
 *   For every 16 bytes of data it encrypts the counter block with the
 *   Feistel cipher, XORs the result into the data, and adds 1 to the
 *   counter. The last piece can be shorter than 16 bytes, so no padding
 *   is needed. A 4-byte counter allows up to 64 GiB per message.
 *
 * compute_tag():
 *   HMAC-SHA256 with the separate MAC key over magic + nonce + ciphertext.
 *   Any change to any of those bytes gives a completely different tag.
 *
 * rc_seal() (encrypt):
 *   writes "RFC1" + nonce, copies the plaintext after them, encrypts it
 *   in place with CTR, then appends the tag ("encrypt-then-MAC").
 *
 * rc_open() (decrypt):
 *   1. Checks the size and the "RFC1" marker.
 *   2. Recomputes the tag and compares it in constant time.
 *   3. If the tag is wrong, STOP and return RC_ERR_AUTH without
 *      decrypting anything (we never hand out tampered data).
 *   4. Otherwise copies the ciphertext and runs CTR again to decrypt.
 * memmove is used instead of memcpy so the input and output buffers may
 * even be the same memory.
 * ===================================================================== */
