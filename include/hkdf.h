#ifndef REFCRYPT_HKDF_H
#define REFCRYPT_HKDF_H

#include "hmac.h"

/* HKDF-SHA256 per RFC 5869. */
void hkdf_extract(const void *salt, size_t salt_len, const void *ikm, size_t ikm_len,
                  uint8_t prk[SHA256_DIGEST_SIZE]);

/* Returns 0 on success, -1 if okm_len > 255 * 32. */
int hkdf_expand(const uint8_t prk[SHA256_DIGEST_SIZE], const void *info, size_t info_len,
                uint8_t *okm, size_t okm_len);

int hkdf(const void *salt, size_t salt_len, const void *ikm, size_t ikm_len,
         const void *info, size_t info_len, uint8_t *okm, size_t okm_len);

#endif

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * This header lists the HKDF functions that other files can call.
 * HKDF ("HMAC-based Key Derivation Function") turns ONE secret key into
 * MANY independent secret keys, one for each job in the cipher.
 *
 *  - hkdf_extract(): "cleans up" the input key (ikm) together with a
 *    salt and produces a strong 32-byte pseudo-random key (prk).
 *  - hkdf_expand(): stretches the prk into as many output bytes (okm) as
 *    needed. The "info" label (e.g. "grid", "mac") makes each output
 *    different. Max output is 255 * 32 = 8160 bytes.
 *  - hkdf(): does extract + expand in one call.
 *
 * The #ifndef / #define / #endif lines are "include guards". They stop
 * the header from being included twice in the same file.
 * ===================================================================== */
