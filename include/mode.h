#ifndef REFCRYPT_MODE_H
#define REFCRYPT_MODE_H

#include <stddef.h>
#include <stdint.h>
#include "keysched.h"

#define RC_MAGIC      "RFC1"
#define RC_MAGIC_LEN  4
#define RC_NONCE      12
#define RC_TAG        32
#define RC_OVERHEAD   (RC_MAGIC_LEN + RC_NONCE + RC_TAG)    /* 48 bytes */

enum { RC_OK = 0, RC_ERR_FORMAT = -1, RC_ERR_AUTH = -2, RC_ERR_TOO_LONG = -3 };

/* CTR mode: buf ^= FEISTEL16(nonce || be32(counter)). Same call encrypts and decrypts. */
int rc_ctr_xor(const rc_key *k, const uint8_t nonce[RC_NONCE], uint8_t *buf, size_t len);

/* out must hold len + RC_OVERHEAD bytes. Layout: magic | nonce | ciphertext | tag */
int rc_seal(const rc_key *k, const uint8_t nonce[RC_NONCE],
            const uint8_t *pt, size_t len, uint8_t *out);

/* Verifies the tag FIRST; only then decrypts. pt must hold in_len - RC_OVERHEAD bytes. */
int rc_open(const rc_key *k, const uint8_t *in, size_t in_len, uint8_t *pt, size_t *pt_len);

#endif

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * A block cipher only scrambles 16 bytes. This "mode" lets us encrypt a
 * whole file of any length, and detect if anyone has changed it.
 *
 * Encrypted file layout (48 bytes bigger than the original):
 *     "RFC1"      4 bytes   magic marker, says "this is our format"
 *     nonce      12 bytes   random, new for every encryption
 *     ciphertext  N bytes   same length as the original file
 *     tag        32 bytes   HMAC fingerprint that detects tampering
 *
 *  - rc_ctr_xor(): CTR ("counter") mode. Encrypt (nonce + 0),
 *    (nonce + 1), ... to make a stream of random-looking bytes, and XOR
 *    it with the data. Doing it twice gives back the original, so the
 *    same function both encrypts and decrypts.
 *  - rc_seal(): encrypt and add the magic, nonce and tag.
 *  - rc_open(): check the magic and the tag, and only if they are right
 *    decrypt. Returns RC_ERR_AUTH if the file was changed or the key is
 *    wrong.
 * ===================================================================== */
