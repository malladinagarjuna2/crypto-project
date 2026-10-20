#ifndef REFCRYPT_SAC_H
#define REFCRYPT_SAC_H

#include "keysched.h"

typedef struct {
    double mean;            /* average fraction of output bits flipped */
    double max_dev;         /* worst |M[i][j] - 0.5| over the 128x128 SAC matrix */
    double frac_bad;        /* fraction of matrix cells with |M - 0.5| > 0.25 */
} sac_result;

/* Plaintext SAC of the Feistel cipher reduced to `rounds` rounds. */
void sac_feistel(const rc_key *k, int rounds, int trials, sac_result *out);

/* Flip one master-key bit; returns mean fraction of ciphertext bits changed. */
double sac_key(const uint8_t master[32], int trials);

#endif

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * SAC = "Strict Avalanche Criterion". A good cipher must behave so that
 * flipping ANY single input bit flips EVERY output bit with a 50% chance.
 *
 *  - sac_result.mean    : on average, what fraction of output bits flipped
 *                         (ideal 0.50).
 *  - sac_result.max_dev : the worst single (input bit, output bit) pair.
 *                         A value near 0.5 means some output bit ALWAYS or
 *                         NEVER flips, which attackers can exploit.
 *  - sac_result.frac_bad: how many pairs are badly off.
 *  - sac_feistel()      : measures this for 1, 2, 4, ... rounds to show
 *                         how quickly the cipher mixes.
 *  - sac_key()          : the same idea, but flipping a bit of the KEY.
 * ===================================================================== */
