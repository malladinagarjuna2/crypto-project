#include "sbox.h"

/* Uniform integer in [0, bound) using rejection sampling (no modulo bias). */
static uint32_t uniform(kdf_stream *rng, uint32_t bound)
{
    uint32_t limit = UINT32_MAX - (UINT32_MAX % bound);
    uint32_t v;
    do {
        v = kdf_stream_u32(rng);
    } while (v >= limit);
    return v % bound;
}

void sbox_build(uint8_t sbox[256], kdf_stream *rng)
{
    int i;
    for (i = 0; i < 256; i++)
        sbox[i] = (uint8_t)i;
    for (i = 255; i > 0; i--) {
        uint32_t j = uniform(rng, (uint32_t)i + 1);
        uint8_t t = sbox[i];
        sbox[i] = sbox[j];
        sbox[j] = t;
    }
}

void sbox_invert(const uint8_t sbox[256], uint8_t inv[256])
{
    int i;
    for (i = 0; i < 256; i++)
        inv[sbox[i]] = (uint8_t)i;
}

int sbox_is_permutation(const uint8_t sbox[256])
{
    uint8_t seen[256] = { 0 };
    int i;
    for (i = 0; i < 256; i++) {
        if (seen[sbox[i]]) return 0;
        seen[sbox[i]] = 1;
    }
    return 1;
}

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 *  - uniform(): picks a random number from 0 up to (bound - 1). A plain
 *    "random % bound" would make some numbers slightly more likely than
 *    others, so we throw away random values from the uneven top end and
 *    try again ("rejection sampling"). Every result is equally likely.
 *  - sbox_build(): the Fisher-Yates shuffle. Start with 0,1,2,...,255.
 *    Going from the last position down to the first, swap each position
 *    with a randomly chosen earlier (or the same) position. Every possible
 *    shuffle is equally likely, and the key decides which one we get.
 *  - sbox_invert(): if sbox[5] = 200, then inv[200] = 5. It undoes the
 *    S-box.
 *  - sbox_is_permutation(): marks each value as "seen". If one shows up
 *    twice, it is not a valid shuffle.
 * ===================================================================== */
