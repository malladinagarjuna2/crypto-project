#ifndef REFCRYPT_KNOWN_PLAINTEXT_H
#define REFCRYPT_KNOWN_PLAINTEXT_H

#include <stddef.h>
#include "keysched.h"

/* Codebook attack against the ORIGINAL plan's design (one byte = one ray,
 * no nonce, no Feistel). Returns fraction of a secret message recovered. */
double kpa_attack_weak(const rc_key *k, const uint8_t *secret, size_t len);

/* Same attacker strategy against the real cipher (CTR + Feistel + nonce). */
double kpa_attack_real(const rc_key *k, const uint8_t *secret, size_t len);

#endif

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * A "known/chosen-plaintext codebook attack": the attacker gets the
 * system to encrypt every possible byte (0..255) once, writes down what
 * each one turns into, and then uses that "codebook" to read secret
 * messages, without ever learning the key.
 *
 *  - kpa_attack_weak(): runs the attack on the design from the ORIGINAL
 *    plan. It should recover ~100% of the secret. That shows the
 *    weakness was real.
 *  - kpa_attack_real(): runs the same attack on the final cipher. It
 *    should recover only ~1/256 (pure guessing).
 * ===================================================================== */
