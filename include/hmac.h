#ifndef REFCRYPT_HMAC_H
#define REFCRYPT_HMAC_H

#include "sha256.h"

typedef struct {
    sha256_ctx inner;
    sha256_ctx outer;
} hmac_sha256_ctx;

void hmac_sha256_init(hmac_sha256_ctx *ctx, const void *key, size_t key_len);
void hmac_sha256_update(hmac_sha256_ctx *ctx, const void *data, size_t len);
void hmac_sha256_final(hmac_sha256_ctx *ctx, uint8_t out[SHA256_DIGEST_SIZE]);
void hmac_sha256(const void *key, size_t key_len, const void *data, size_t len,
                 uint8_t out[SHA256_DIGEST_SIZE]);

/* Constant-time comparison: returns 1 if equal, 0 otherwise. */
int ct_equal(const void *a, const void *b, size_t len);

#endif

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * HMAC is a "keyed hash": SHA-256 mixed with a secret key. Only someone
 * who knows the key can produce the right 32-byte tag for a message.
 * We use it to (1) build HKDF and (2) protect ciphertext files from
 * being tampered with.
 *
 *  - hmac_sha256_ctx : holds two SHA-256 contexts, "inner" and "outer".
 *  - init / update / final : compute an HMAC piece by piece.
 *  - hmac_sha256()   : compute an HMAC in one call.
 *  - ct_equal()      : compares two byte arrays in "constant time". It
 *                      always checks every byte, even after finding a
 *                      difference, so an attacker can't learn anything
 *                      from how long the comparison took.
 * ===================================================================== */
