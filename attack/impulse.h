#ifndef REFCRYPT_IMPULSE_H
#define REFCRYPT_IMPULSE_H

#include "keysched.h"

/* Tests the linear (LTI) relation  E(a^b) == E(a) ^ E(b) ^ E(0)  on
 * `pairs` random pairs. If `right_zero` is set, a and b only use the
 * left half (R = 0), the sparse "impulse" style input of the original plan.
 * Returns the fraction of pairs for which the relation held. */
double impulse_linearity(const rc_key *k, int rounds, int pairs, int right_zero);

#endif

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * If a cipher were LINEAR, then encrypting (a XOR b) would equal
 * E(a) XOR E(b) XOR E(0). An attacker could then encrypt only the 128
 * single-bit "impulses" (0...001, 0...010, ...) and predict the
 * encryption of EVERY other block, with no key needed.
 *
 * impulse_linearity() checks how often that equation holds. A good
 * cipher: practically never (fraction 0). A broken one: always (1.0).
 * ===================================================================== */
