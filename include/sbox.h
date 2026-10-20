#ifndef REFCRYPT_SBOX_H
#define REFCRYPT_SBOX_H

#include <stdint.h>
#include "keysched.h"

/* Builds a key-dependent permutation of 0..255 (unbiased Fisher-Yates). */
void sbox_build(uint8_t sbox[256], kdf_stream *rng);
void sbox_invert(const uint8_t sbox[256], uint8_t inv[256]);
int  sbox_is_permutation(const uint8_t sbox[256]);

#endif

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * An S-box ("substitution box") is a secret lookup table: put in a byte
 * (0..255), get out a different byte. Every output appears exactly once,
 * so it is a "permutation" (a shuffle) of 0..255.
 *
 *  - sbox_build()  : makes the shuffle using random bytes from the key.
 *  - sbox_invert() : makes the reverse table (inv[sbox[x]] = x). It is
 *                    used only in tests, to prove the S-box is reversible.
 *  - sbox_is_permutation(): checks that every value 0..255 appears once.
 * ===================================================================== */
