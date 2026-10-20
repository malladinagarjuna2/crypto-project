#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "sac.h"
#include "feistel.h"
#include "raytrace.h"

static int popcount64(uint64_t v)
{
    int c = 0;
    while (v) { v &= v - 1; c++; }
    return c;
}

void sac_feistel(const rc_key *k, int rounds, int trials, sac_result *out)
{
    static uint32_t M[128][128];             /* flip counts per (input bit, output bit) */
    uint64_t seed = 0xACE1ULL + (uint64_t)rounds;
    double total = 0;
    int t, i, j, bad = 0;

    memset(M, 0, sizeof M);
    for (t = 0; t < trials; t++) {
        uint64_t L0, R0, L, R;
        seed = rt_mix64(seed + 1); L0 = seed;
        seed = rt_mix64(seed + 1); R0 = seed;
        L = L0; R = R0;
        feistel_encrypt_rounds(k, &L, &R, rounds);

        for (i = 0; i < 128; i++) {
            uint64_t L2 = L0, R2 = R0, dL, dR;
            if (i < 64) L2 ^= 1ULL << i; else R2 ^= 1ULL << (i - 64);
            feistel_encrypt_rounds(k, &L2, &R2, rounds);
            dL = L ^ L2; dR = R ^ R2;
            total += popcount64(dL) + popcount64(dR);
            for (j = 0; j < 64; j++) {
                M[i][j]      += (uint32_t)((dL >> j) & 1);
                M[i][64 + j] += (uint32_t)((dR >> j) & 1);
            }
        }
    }

    out->mean = total / ((double)trials * 128.0 * 128.0);
    out->max_dev = 0;
    for (i = 0; i < 128; i++)
        for (j = 0; j < 128; j++) {
            double dev = fabs((double)M[i][j] / trials - 0.5);
            if (dev > out->max_dev) out->max_dev = dev;
            if (dev > 0.25) bad++;
        }
    out->frac_bad = bad / (128.0 * 128.0);
}

double sac_key(const uint8_t master[32], int trials)
{
    rc_key *a = (rc_key *)malloc(sizeof *a), *b = (rc_key *)malloc(sizeof *b);
    uint8_t key[32], blk[16], ca[16], cb[16];
    double flipped = 0;
    int t, n, i;

    if (!a || !b) { free(a); free(b); return -1; }
    rc_key_init(a, master);
    for (t = 0; t < trials; t++) {
        memcpy(key, master, 32);
        key[(t * 7) % 32] ^= (uint8_t)(1u << (t % 8));      /* a different single bit each trial */
        rc_key_init(b, key);
        for (n = 0; n < 16; n++) {
            for (i = 0; i < 16; i++) blk[i] = (uint8_t)(n * 16 + i);
            feistel_encrypt_block(a, blk, ca);
            feistel_encrypt_block(b, blk, cb);
            for (i = 0; i < 16; i++) {
                uint8_t d = ca[i] ^ cb[i];
                while (d) { flipped += d & 1; d >>= 1; }
            }
        }
    }
    rc_key_wipe(a); rc_key_wipe(b);
    free(a); free(b);
    return flipped / (trials * 16.0 * 128.0);
}

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * sac_feistel():
 *   For each trial:
 *     1. Pick a random 128-bit block X and encrypt it.
 *     2. For each of the 128 input bits i: flip bit i, encrypt again,
 *        and see which output bits changed (XOR of the two ciphertexts).
 *     3. Count, in matrix M[i][j], how often output bit j flipped when
 *        input bit i was flipped.
 *   At the end every M[i][j] / trials should be close to 0.5.
 *   - mean    = overall fraction of flipped bits.
 *   - max_dev = the worst distance from 0.5 in the whole matrix.
 *   - frac_bad = share of cells more than 0.25 away from 0.5.
 *   With 1 round, half the output is just a copy of the input, so many
 *   cells are exactly 0 or 1 (bad). With enough rounds, all are near 0.5.
 *
 * sac_key():
 *   Builds the cipher with the original key and with a key differing in
 *   one bit (a different bit each trial), encrypts the same 16 blocks
 *   with both, and measures the fraction of ciphertext bits that differ.
 *   Ideal: 0.5.
 * ===================================================================== */
