#include "impulse.h"
#include "feistel.h"
#include "raytrace.h"

static void enc(const rc_key *k, int rounds, uint64_t L, uint64_t R, uint64_t out[2])
{
    feistel_encrypt_rounds(k, &L, &R, rounds);
    out[0] = L; out[1] = R;
}

double impulse_linearity(const rc_key *k, int rounds, int pairs, int right_zero)
{
    uint64_t seed = 0x1F1F1F1FULL + (uint64_t)rounds * 31 + (uint64_t)right_zero;
    uint64_t e0[2], ea[2], eb[2], eab[2];
    int p, hold = 0;

    enc(k, rounds, 0, 0, e0);
    for (p = 0; p < pairs; p++) {
        uint64_t aL, aR, bL, bR;
        seed = rt_mix64(seed + 1); aL = seed;
        seed = rt_mix64(seed + 1); aR = right_zero ? 0 : seed;
        seed = rt_mix64(seed + 1); bL = seed;
        seed = rt_mix64(seed + 1); bR = right_zero ? 0 : seed;
        enc(k, rounds, aL, aR, ea);
        enc(k, rounds, bL, bR, eb);
        enc(k, rounds, aL ^ bL, aR ^ bR, eab);
        if (eab[0] == (ea[0] ^ eb[0] ^ e0[0]) && eab[1] == (ea[1] ^ eb[1] ^ e0[1]))
            hold++;
    }
    return (double)hold / pairs;
}

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 *  - enc(): helper that encrypts one block with a given number of rounds.
 *  - impulse_linearity():
 *      1. Encrypt the all-zero block once -> E(0).
 *      2. For each pair: make random blocks a and b (with R = 0 when
 *         right_zero is set), encrypt a, b and (a XOR b).
 *      3. If E(a^b) == E(a) ^ E(b) ^ E(0) for the whole block, count it.
 *      4. Return the share of pairs where the "linear" rule held.
 *
 * Why the right_zero option? With ONE round and R = 0, the output is
 * (0, L XOR F(0)), which is perfectly linear in L. The test should catch
 * this (fraction 1.0). That proves the test works. With 16 rounds the
 * fraction must drop to 0.
 * ===================================================================== */
