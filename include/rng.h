#ifndef REFCRYPT_RNG_H
#define REFCRYPT_RNG_H

#include <stddef.h>
#include <stdint.h>

/* Fills buf with bytes from the operating system's secure RNG.
 * Returns 0 on success, -1 on failure. */
int rng_bytes(uint8_t *buf, size_t len);

#endif

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * rng_bytes() asks the operating system for truly unpredictable random
 * bytes. We use them for secret keys (keygen) and for the nonce, the
 * random 12-byte number that makes every encryption different.
 * Never use rand() for this, because it is predictable.
 * ===================================================================== */
